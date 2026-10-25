#include "websocket_server.h"
#include <QDebug>
#include <QMutexLocker>
#include <QDateTime>
#include <QCoreApplication>

namespace WebSocketServer {

WebSocketServer::WebSocketServer(quint16 port, QObject* parent)
    : QObject(parent)
    , m_port(port)
    , m_server(nullptr)
    , m_running(false)
{
    m_server = new QWebSocketServer("SCADA WebSocket Server", QWebSocketServer::NonSecureMode, this);
    connect(m_server, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);
    connect(m_server, &QWebSocketServer::acceptError, this, &WebSocketServer::onServerError);
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    QMutexLocker locker(&m_mutex);
    
    if (m_server && m_server->isListening()) {
        return true;
    }

    if (!m_server) {
        m_server = new QWebSocketServer("SCADA WebSocket Server", QWebSocketServer::NonSecureMode, this);
        connect(m_server, &QWebSocketServer::newConnection, this, &WebSocketServer::onNewConnection);
        connect(m_server, &QWebSocketServer::acceptError, this, &WebSocketServer::onServerError);
    }
    
    if (m_server->listen(QHostAddress::Any, m_port)) {
        m_running = true;
        qDebug() << "WebSocket server started on port" << m_port;
        return true;
    } else {
        QString err = m_server ? m_server->errorString() : "Server is null";
        qWarning() << "WebSocket server failed to start:" << err;
        emit serverError(err);
        return false;
    }
}

void WebSocketServer::stop() {
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) {
        return;
    }

    qDebug() << "WebSocket server stopping, closing" << m_clients.size() << "client connections";
    
    for (QWebSocket* client : m_clients) {
        if (client) {
            client->close(QWebSocketProtocol::CloseCodeGoingAway, "Server shutting down");
            client->disconnect();
        }
    }
    m_clients.clear();

    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
    m_running = false;
    qDebug() << "WebSocket server stopped";
}

bool WebSocketServer::hasClients() const {
    QMutexLocker locker(&m_mutex);
    return !m_clients.isEmpty();
}

void WebSocketServer::broadcast(const QJsonObject& message) {
    broadcastToAll(message);
}

void WebSocketServer::onNewConnection() {
    QWebSocket* client = m_server->nextPendingConnection();
    if (!client) {
        return;
    }

    QString clientAddr = client->peerAddress().toString();

    connect(client, &QWebSocket::textMessageReceived, this, &WebSocketServer::onTextMessageReceived);
    connect(client, &QWebSocket::disconnected, this, &WebSocketServer::onSocketDisconnected);

    {
        QMutexLocker locker(&m_mutex);
        m_clients.insert(client);
    }

    emit clientConnected(clientAddr);
}

void WebSocketServer::onSocketDisconnected() {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }

    QString clientAddr = client->peerAddress().toString();

    {
        QMutexLocker locker(&m_mutex);
        m_clients.remove(client);
    }

    emit clientDisconnected(clientAddr);
    client->deleteLater();
}

void WebSocketServer::onTextMessageReceived(const QString& message) {
    QWebSocket* client = qobject_cast<QWebSocket*>(sender());
    if (!client) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return;
    }

    if (!doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();
    
    if (obj["type"].toString() == "heartbeat") {
        QJsonObject response;
        response["type"] = "heartbeat_response";
        response["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        sendToClient(client, response);
        return;
    }
    
    QString clientId = QString::number(reinterpret_cast<quint64>(client));
    emit messageReceived(clientId, obj);
}

void WebSocketServer::onServerError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    if (m_server) {
        QString errorStr = m_server->errorString();
        emit serverError(errorStr);
    }
}

void WebSocketServer::sendToClient(QWebSocket* client, const QJsonObject& message) {
    if (!client) {
        return;
    }
    
    if (client->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QJsonDocument doc(message);
    client->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void WebSocketServer::broadcastToAll(const QJsonObject& message) {
    QMutexLocker locker(&m_mutex);
    
    if (m_clients.isEmpty()) {
        return;
    }
    
    QJsonDocument doc(message);
    QString jsonString = doc.toJson(QJsonDocument::Compact);

    QList<QWebSocket*> clientsCopy = m_clients.values();
    locker.unlock();

    for (QWebSocket* client : clientsCopy) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendTextMessage(jsonString);
        }
    }
}

// ==================== WebSocketThreadWorker ====================

WebSocketThreadWorker::WebSocketThreadWorker(quint16 port, QObject* parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_port(port)
{
}

WebSocketThreadWorker::~WebSocketThreadWorker() {
    stopServer();
}

bool WebSocketThreadWorker::hasClients() const {
    return m_server && m_server->hasClients();
}

void WebSocketThreadWorker::startServer() {
    if (m_server && m_server->isRunning()) {
        return;
    }
    
    if (!m_server) {
        m_server = new WebSocketServer(m_port, nullptr);
    }
    
    connect(m_server, &WebSocketServer::clientConnected, 
            this, &WebSocketThreadWorker::clientConnected);
    connect(m_server, &WebSocketServer::clientDisconnected, 
            this, &WebSocketThreadWorker::clientDisconnected);
    connect(m_server, &WebSocketServer::serverError, 
            this, &WebSocketThreadWorker::serverError);
    
    bool success = m_server->start();
    emit serverStarted(success);
}

void WebSocketThreadWorker::stopServer() {
    if (m_server) {
        m_server->stop();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void WebSocketThreadWorker::broadcastData(const QJsonObject& data) {
    if (m_server) {
        m_server->broadcast(data);
    }
}

// ==================== WebSocketServerManager ====================

WebSocketServerManager::WebSocketServerManager(quint16 port, QObject* parent)
    : QObject(parent)
    , m_thread(new QThread(this))
    , m_worker(new WebSocketThreadWorker(port))
{
    m_worker->moveToThread(m_thread);
    
    connect(m_worker, &WebSocketThreadWorker::clientConnected, 
            this, &WebSocketServerManager::clientConnected);
    connect(m_worker, &WebSocketThreadWorker::clientDisconnected, 
            this, &WebSocketServerManager::clientDisconnected);
    connect(m_worker, &WebSocketThreadWorker::serverError, 
            this, &WebSocketServerManager::serverError);
    connect(m_worker, &WebSocketThreadWorker::serverStarted,
            this, [this](bool success) {
        if (success) {
            qDebug() << "WebSocketServerManager: Server started successfully";
        } else {
            qWarning() << "WebSocketServerManager: Server failed to start";
        }
    });
    
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
}

WebSocketServerManager::~WebSocketServerManager() {
    stop();
}

void WebSocketServerManager::start() {
    if (m_thread->isRunning()) {
        qDebug() << "WebSocketServerManager: Thread already running, skipping start";
        return;
    }
    
    qDebug() << "WebSocketServerManager: Starting thread and server";
    m_worker = new WebSocketThreadWorker(18080);
    m_worker->moveToThread(m_thread);
    
    connect(m_worker, &WebSocketThreadWorker::clientConnected, 
            this, &WebSocketServerManager::clientConnected);
    connect(m_worker, &WebSocketThreadWorker::clientDisconnected, 
            this, &WebSocketServerManager::clientDisconnected);
    connect(m_worker, &WebSocketThreadWorker::serverError, 
            this, &WebSocketServerManager::serverError);
    connect(m_worker, &WebSocketThreadWorker::serverStarted,
            this, [this](bool success) {
        if (success) {
            qDebug() << "WebSocketServerManager: Server started successfully";
        } else {
            qWarning() << "WebSocketServerManager: Server failed to start";
        }
    });
    
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    
    m_thread->start();
    QMetaObject::invokeMethod(m_worker, "startServer", Qt::QueuedConnection);
}

void WebSocketServerManager::stop() {
    if (m_thread->isRunning()) {
        qDebug() << "WebSocketServerManager: Stopping server and thread";
        QMetaObject::invokeMethod(m_worker, "stopServer", Qt::QueuedConnection);
        m_thread->quit();
        if (!m_thread->wait(3000)) {
            qWarning() << "WebSocketServerManager: Thread did not finish, terminating";
            m_thread->terminate();
            m_thread->wait();
        }
    }
}

bool WebSocketServerManager::hasClients() const {
    return m_worker->hasClients();
}

void WebSocketServerManager::broadcastData(const QJsonObject& data) {
    QMetaObject::invokeMethod(m_worker, "broadcastData", 
                              Qt::QueuedConnection, 
                              Q_ARG(QJsonObject, data));
}

}
