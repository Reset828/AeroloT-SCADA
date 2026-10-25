#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QWebSocket>

#include "WebSocketServer/websocket_server.h"

class WebSocketServerTests : public QObject
{
    Q_OBJECT

private slots:
    void heartbeatMessagesAndBroadcasts();
};

void WebSocketServerTests::heartbeatMessagesAndBroadcasts()
{
    WebSocketServer::WebSocketServer server(18080);
    QVERIFY(server.start());
    QVERIFY(server.isRunning());
    QVERIFY(server.start());

    QSignalSpy connectedSpy(&server, &WebSocketServer::WebSocketServer::clientConnected);
    QSignalSpy messageSpy(&server, &WebSocketServer::WebSocketServer::messageReceived);
    QSignalSpy disconnectedSpy(&server, &WebSocketServer::WebSocketServer::clientDisconnected);
    QVERIFY(connectedSpy.isValid());
    QVERIFY(messageSpy.isValid());
    QVERIFY(disconnectedSpy.isValid());

    QWebSocket client;
    QSignalSpy textMessageSpy(&client, &QWebSocket::textMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:18080")));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::ConnectedState, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);
    QVERIFY(server.hasClients());

    const QJsonObject heartbeat{{QStringLiteral("type"), QStringLiteral("heartbeat")}};
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(heartbeat).toJson(QJsonDocument::Compact)));
    QTRY_COMPARE_WITH_TIMEOUT(textMessageSpy.count(), 1, 3000);
    const auto heartbeatReply = QJsonDocument::fromJson(
        textMessageSpy.takeFirst().at(0).toString().toUtf8()).object();
    QCOMPARE(heartbeatReply.value(QStringLiteral("type")).toString(),
             QStringLiteral("heartbeat_response"));
    QVERIFY(heartbeatReply.value(QStringLiteral("timestamp")).toVariant().toLongLong() > 0);
    QCOMPARE(messageSpy.count(), 0);

    const QJsonObject clientMessage{{QStringLiteral("type"), QStringLiteral("status_request")},
                                    {QStringLiteral("requestId"), 42}};
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(clientMessage).toJson(QJsonDocument::Compact)));
    QTRY_COMPARE_WITH_TIMEOUT(messageSpy.count(), 1, 3000);
    const auto received = messageSpy.takeFirst();
    QVERIFY(!received.at(0).toString().isEmpty());
    QCOMPARE(received.at(1).toJsonObject(), clientMessage);

    const QJsonObject broadcast{{QStringLiteral("type"), QStringLiteral("status")},
                                {QStringLiteral("connected"), true}};
    server.broadcast(broadcast);
    QTRY_COMPARE_WITH_TIMEOUT(textMessageSpy.count(), 1, 3000);
    QCOMPARE(QJsonDocument::fromJson(
                 textMessageSpy.takeFirst().at(0).toString().toUtf8()).object(),
             broadcast);

    client.close();
    QTRY_COMPARE_WITH_TIMEOUT(disconnectedSpy.count(), 1, 3000);
    QVERIFY(!server.hasClients());
    server.stop();
    QVERIFY(!server.isRunning());
}

QTEST_GUILESS_MAIN(WebSocketServerTests)

#include "websocket_server_tests.moc"
