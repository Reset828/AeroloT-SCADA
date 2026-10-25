#ifndef WEBSOCKETSERVER_WEBSOCKETSERVER_H
#define WEBSOCKETSERVER_WEBSOCKETSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSet>
#include <QMutex>
#include <QThread>
#include <QQueue>
#include <QWaitCondition>

namespace WebSocketServer {

class WebSocketServer : public QObject {
    Q_OBJECT
public:
    explicit WebSocketServer(quint16 port = 18080, QObject* parent = nullptr);
    ~WebSocketServer();

    bool start();
    void stop();
    bool isRunning() const { return m_server && m_server->isListening(); }
    void broadcast(const QJsonObject& message);
    bool hasClients() const;

signals:
    void clientConnected(const QString& clientInfo);
    void clientDisconnected(const QString& clientInfo);
    void serverError(const QString& error);
    void messageReceived(const QString& clientId, const QJsonObject& message);

private slots:
    void onNewConnection();
    void onSocketDisconnected();
    void onTextMessageReceived(const QString& message);
    void onServerError(QAbstractSocket::SocketError error);

private:
    void sendToClient(QWebSocket* client, const QJsonObject& message);
    void broadcastToAll(const QJsonObject& message);

    quint16 m_port;
    QWebSocketServer* m_server;
    QSet<QWebSocket*> m_clients;
    mutable QMutex m_mutex;
    bool m_running;
};

class WebSocketThreadWorker : public QObject {
    Q_OBJECT
public:
    explicit WebSocketThreadWorker(quint16 port = 18080, QObject* parent = nullptr);
    ~WebSocketThreadWorker();

    bool hasClients() const;

public slots:
    void startServer();
    void stopServer();
    void broadcastData(const QJsonObject& data);

signals:
    void clientConnected(const QString& clientInfo);
    void clientDisconnected(const QString& clientInfo);
    void serverError(const QString& error);
    void serverStarted(bool success);

private:
    WebSocketServer* m_server;
    quint16 m_port;
};

class WebSocketServerManager : public QObject {
    Q_OBJECT
public:
    explicit WebSocketServerManager(quint16 port = 18080, QObject* parent = nullptr);
    ~WebSocketServerManager();

    void start();
    void stop();
    bool hasClients() const;
    void broadcastData(const QJsonObject& data);

signals:
    void clientConnected(const QString& clientInfo);
    void clientDisconnected(const QString& clientInfo);
    void serverError(const QString& error);

private:
    QThread* m_thread;
    WebSocketThreadWorker* m_worker;
};

}

#endif
