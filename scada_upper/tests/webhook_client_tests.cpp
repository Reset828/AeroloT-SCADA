#include <QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include "AlarmEngine/webhook_client.h"

class WebhookClientTests : public QObject
{
    Q_OBJECT

private slots:
    void failedRequestsAreRetriedThreeTimesThenDropped();
};

void WebhookClientTests::failedRequestsAreRetriedThreeTimesThenDropped()
{
    QTcpServer httpServer;
    QVERIFY(httpServer.listen(QHostAddress::LocalHost, 0));

    int requestCount = 0;
    QObject::connect(&httpServer, &QTcpServer::newConnection, &httpServer, [&] {
        while (QTcpSocket* socket = httpServer.nextPendingConnection()) {
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const QByteArray request = socket->readAll();
                if (!request.contains("\r\n\r\n")) {
                    return;
                }

                ++requestCount;
                socket->write("HTTP/1.1 503 Service Unavailable\r\n"
                              "Content-Length: 0\r\n"
                              "Connection: close\r\n\r\n");
                socket->disconnectFromHost();
            });
        }
    });

    core::WebhookClient client;
    client.setUrl(QStringLiteral("http://127.0.0.1:%1/webhook")
                      .arg(httpServer.serverPort()));
    client.start();

    QSignalSpy failedSpy(&client, &core::WebhookClient::pushFailed);
    QVERIFY(failedSpy.isValid());

    int droppedCount = 0;
    core::WebhookTask droppedTask;
    QObject::connect(&client, &core::WebhookClient::taskDropped,
                     this, [&](const core::WebhookTask& task) {
        ++droppedCount;
        droppedTask = task;
    });

    const QJsonObject payload{{QStringLiteral("alarmId"), 17},
                              {QStringLiteral("state"), QStringLiteral("triggered")}};
    client.sendAlarm(payload);

    QTRY_COMPARE_WITH_TIMEOUT(droppedCount, 1, 15000);
    QCOMPARE(requestCount, 4);
    QCOMPARE(failedSpy.count(), 4);
    QCOMPARE(droppedTask.retryCount, 3);
    QCOMPARE(droppedTask.payload, payload);
    QCOMPARE(droppedTask.url, client.url());

    client.stop();
}

QTEST_GUILESS_MAIN(WebhookClientTests)

#include "webhook_client_tests.moc"
