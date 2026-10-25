#ifndef WEBHOOK_CLIENT_H
#define WEBHOOK_CLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>
#include <QMutex>
#include <QUrl>
#include <QMap>
#include <QThread>

#include "../core/timestamp.h"

namespace core {

/**
 * @brief Webhook任务结构体，包含报警推送所需信息
 */
struct WebhookTask {
    QString url;                    // Webhook目标URL
    QJsonObject payload;           // JSON负载数据
    int retryCount = 0;            // 当前重试次数
    qint64 timestamp = 0;          // 任务创建时间戳
};

/**
 * @brief WebhookClient类，负责报警消息的HTTP POST推送
 *        支持失败重试，指数退避策略
 */
class WebhookClient : public QObject
{
    Q_OBJECT

public:
    explicit WebhookClient(QObject *parent = nullptr);
    ~WebhookClient();

    void setUrl(const QString &url);
    QString url() const;
    void sendAlarm(const QJsonObject &payload);
    void start();
    void stop();

signals:
    void pushSuccess(const QJsonObject &payload);
    void pushFailed(const QJsonObject &payload, const QString &error);
    void taskDropped(const WebhookTask &task);

private slots:
    void onReplyFinished(QNetworkReply *reply);
    void onRetryTimeout();

private:
    void executeRequest(const WebhookTask &task);
    int calculateRetryDelay(int retryCount);
    void enqueueRetry(const WebhookTask &task);
    void processRetryQueue();
    void auditLog(const QString &message);

    QNetworkAccessManager *m_networkManager;
    mutable QString m_url;
    QQueue<WebhookTask> m_retryQueue;
    mutable QMutex m_mutex;
    QTimer *m_retryTimer;
    bool m_running;
    int m_maxRetries;
    QMap<QNetworkReply *, WebhookTask> m_pendingTasks;
};

class WebhookClientThread : public QObject
{
    Q_OBJECT

public:
    explicit WebhookClientThread(QObject *parent = nullptr);
    ~WebhookClientThread();

    void setUrl(const QString &url);
    void sendAlarm(const QJsonObject &payload);
    void start();
    void stop();

signals:
    void pushSuccess(const QJsonObject &payload);
    void pushFailed(const QJsonObject &payload, const QString &error);
    void taskDropped(const WebhookTask &task);

private:
    QThread *m_thread;
    WebhookClient *m_client;
};

} // namespace core

#endif // WEBHOOK_CLIENT_H