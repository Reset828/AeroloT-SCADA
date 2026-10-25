#include "webhook_client.h"

#include <QDebug>
#include <QJsonDocument>
#include <QRandomGenerator>

namespace core {

// 常量定义：最大重试次数和最大延迟时间（毫秒）
constexpr int MAX_RETRIES = 3;
constexpr int MAX_RETRY_DELAY_MS = 60000;
constexpr int DEFAULT_RETRY_INTERVAL_MS = 1000;

WebhookClient::WebhookClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_retryTimer(new QTimer(this))
    , m_running(false)
    , m_maxRetries(MAX_RETRIES)
{
    // 连接重试定时器超时信号到处理槽
    connect(m_retryTimer, &QTimer::timeout, this, &WebhookClient::onRetryTimeout);

    // 连接网络管理器的finished信号到响应处理槽
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &WebhookClient::onReplyFinished);
}

WebhookClient::~WebhookClient()
{
    stop();
}

void WebhookClient::setUrl(const QString &url)
{
    QMutexLocker locker(&m_mutex);
    m_url = url;
}

QString WebhookClient::url() const
{
    QMutexLocker locker(&m_mutex);
    return m_url;
}

void WebhookClient::sendAlarm(const QJsonObject &payload)
{
    // 检查URL是否已配置
    if (m_url.isEmpty()) {
        qWarning() << "Webhook URL not configured, alarm payload:" << payload;
        emit pushFailed(payload, "URL not configured");
        return;
    }

    // 创建任务并立即执行
    WebhookTask task;
    task.url = m_url;
    task.payload = payload;
    task.retryCount = 0;
    task.timestamp = core::Timestamp::currentMillis();

    executeRequest(task);
}

void WebhookClient::start()
{
    QMutexLocker locker(&m_mutex);
    if (!m_running) {
        m_running = true;
        qDebug() << "WebhookClient started";
    }
}

void WebhookClient::stop()
{
    QMutexLocker locker(&m_mutex);
    if (m_running) {
        m_running = false;
        m_retryTimer->stop();
        m_retryQueue.clear();
        qDebug() << "WebhookClient stopped, cleared retry queue";
    }
}

void WebhookClient::onReplyFinished(QNetworkReply *reply)
{
    // 从pendingTasks中获取对应的任务信息
    WebhookTask task;
    bool hasTask = false;

    auto it = m_pendingTasks.find(reply);
    if (it != m_pendingTasks.end()) {
        task = it.value();
        hasTask = true;
        m_pendingTasks.erase(it);
    }

    // 检查HTTP响应状态
    QNetworkReply::NetworkError error = reply->error();
    QString errorString = reply->errorString();

    if (error == QNetworkReply::NoError) {
        // 请求成功
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode >= 200 && statusCode < 300) {
            qDebug() << "Webhook push success, status:" << statusCode;
            if (hasTask) {
                emit pushSuccess(task.payload);
            }
        } else {
            // HTTP错误状态码
            QString msg = QString("HTTP %1: %2").arg(statusCode).arg(errorString);
            qWarning() << "Webhook push failed:" << msg;
            if (hasTask) {
                enqueueRetry(task);
                emit pushFailed(task.payload, msg);
            }
        }
    } else {
        // 网络错误
        qWarning() << "Webhook network error:" << errorString;
        if (hasTask) {
            enqueueRetry(task);
            emit pushFailed(task.payload, errorString);
        }
    }

    // 清理reply资源
    reply->deleteLater();
}

void WebhookClient::onRetryTimeout()
{
    processRetryQueue();
}

void WebhookClient::executeRequest(const WebhookTask &task)
{
    QNetworkRequest request;
    request.setUrl(QUrl(task.url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    int timeoutMs = 5000 + (task.retryCount * 2000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, false);

    QJsonDocument doc(task.payload);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, jsonData);
    reply->setProperty("taskData", QVariant::fromValue(task));
    reply->setProperty("timeoutMs", timeoutMs);

    QTimer::singleShot(timeoutMs, reply, [reply, this, task]() {
        if (reply && reply->isRunning()) {
            qWarning() << "Webhook request timeout after" << task.retryCount << "retries, aborting";
            reply->abort();
        }
    });

    m_pendingTasks.insert(reply, task);

    qDebug() << "Webhook request sent, retry count:" << task.retryCount << ", timeout:" << timeoutMs << "ms";
}

int WebhookClient::calculateRetryDelay(int retryCount)
{
    // 指数退避算法: delay = min(1000 * pow(2, retryCount-1), 60000)
    // 第1次重试(n=0): 1s × 2^0 = 1s
    // 第2次重试(n=1): 1s × 2^1 = 2s
    // 第3次重试(n=2): 1s × 2^2 = 4s
    // 最大延迟60秒
    int n = retryCount - 1;
    if (n < 0) n = 0;
    int baseDelay = qMin(static_cast<int>(DEFAULT_RETRY_INTERVAL_MS * qPow(2, n)), MAX_RETRY_DELAY_MS);
    
    // 添加随机抖动 (±25%) 防止多个客户端同时重试
    int jitter = static_cast<int>(baseDelay * 0.25);
    int randomJitter = QRandomGenerator::global()->bounded(-jitter, jitter);
    int finalDelay = baseDelay + randomJitter;
    
    return qMax(100, finalDelay); // 确保最小延迟100ms
}

void WebhookClient::enqueueRetry(const WebhookTask &task)
{
    QMutexLocker locker(&m_mutex);

    // 检查重试次数是否超过上限
    if (task.retryCount >= m_maxRetries) {
        // 丢弃任务并记录审计日志
        QString msg = QString("Webhook task dropped after %1 retries, URL: %2, payload: %3")
                          .arg(task.retryCount)
                          .arg(task.url)
                          .arg(QJsonDocument(task.payload).toJson(QJsonDocument::Compact));
        auditLog(msg);

        emit taskDropped(task);
        return;
    }

    // 创建新的重试任务（增加重试计数）
    WebhookTask newTask = task;
    newTask.retryCount = task.retryCount + 1;
    newTask.timestamp = core::Timestamp::currentMillis();

    // 加入重试队列
    m_retryQueue.enqueue(newTask);

    // 计算延迟并启动/重启定时器
    int delay = calculateRetryDelay(newTask.retryCount);
    m_retryTimer->start(delay);

    qDebug() << "Task enqueued for retry, retry count:" << newTask.retryCount
             << ", delay:" << delay << "ms";
}

void WebhookClient::processRetryQueue()
{
    QMutexLocker locker(&m_mutex);

    if (m_retryQueue.isEmpty() || !m_running) {
        m_retryTimer->stop();
        return;
    }

    // 取出队首任务执行
    WebhookTask task = m_retryQueue.dequeue();
    locker.unlock();
    executeRequest(task);
}

void WebhookClient::auditLog(const QString &message)
{
    QString timestamp = QString::number(core::Timestamp::currentMillis());
    QString logLine = QString("[%1] AUDIT: %2").arg(timestamp, message);
    qDebug() << logLine;
}

// ==================== WebhookClientThread ====================

WebhookClientThread::WebhookClientThread(QObject *parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_client(new WebhookClient())
{
    m_client->moveToThread(m_thread);
    
    connect(m_thread, &QThread::finished, m_client, &QObject::deleteLater);
    connect(m_client, &WebhookClient::pushSuccess, this, &WebhookClientThread::pushSuccess);
    connect(m_client, &WebhookClient::pushFailed, this, &WebhookClientThread::pushFailed);
    connect(m_client, &WebhookClient::taskDropped, this, &WebhookClientThread::taskDropped);
}

WebhookClientThread::~WebhookClientThread()
{
    stop();
}

void WebhookClientThread::setUrl(const QString &url)
{
    QMetaObject::invokeMethod(m_client, "setUrl", Qt::QueuedConnection, Q_ARG(QString, url));
}

void WebhookClientThread::sendAlarm(const QJsonObject &payload)
{
    QMetaObject::invokeMethod(m_client, "sendAlarm", Qt::QueuedConnection, Q_ARG(QJsonObject, payload));
}

void WebhookClientThread::start()
{
    if (!m_thread->isRunning()) {
        m_thread->start();
        QMetaObject::invokeMethod(m_client, "start", Qt::QueuedConnection);
    }
}

void WebhookClientThread::stop()
{
    if (m_thread->isRunning()) {
        QMetaObject::invokeMethod(m_client, "stop", Qt::QueuedConnection);
        m_thread->quit();
        if (!m_thread->wait(3000)) {
            qWarning() << "WebhookClientThread: Thread did not finish, terminating";
            m_thread->terminate();
            m_thread->wait();
        }
    }
}

} // namespace core
