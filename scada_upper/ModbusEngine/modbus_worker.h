#ifndef MODBUSENGINE_MODBUS_WORKER_H
#define MODBUSENGINE_MODBUS_WORKER_H

#include <QObject>
#include <QTimer>
#include <QModbusTcpClient>
#include <QModbusReply>
#include "command_queue.h"
#include "core/types.h"

namespace ModbusEngine {

class ModbusWorker : public QObject {
    Q_OBJECT
public:
    explicit ModbusWorker(QObject* parent = nullptr);
    ~ModbusWorker();

public slots:
    void startConnection(const QString& host, quint16 port);
    void stopConnection();
    void pushReadCommand(const Command& cmd);
    void pushWriteCommand(const Command& cmd);
    void checkConnection();

signals:
    void connectionStateChanged(core::ConnectionState state);
    void modbusErrorOccurred(const QString& errorString);
    void dataReceived(int slaveId, const QVector<quint16>& values, QModbusDataUnit::RegisterType registerType);
    void connectionReady();

private slots:
    void onStateChanged(QModbusClient::State state);
    void onErrorOccurred(QModbusClient::Error error);
    void onReplyFinished();
    void onReconnectTimerTimeout();
    void onRequestTimeout();

private:
    void sendNextCommand();
    void handleReply(QModbusReply* reply);
    void checkConnectionLost();
    void setState(core::ConnectionState state);

    QModbusTcpClient* m_modbusClient;
    CommandQueue* m_commandQueue;
    QTimer* m_reconnectTimer;
    QTimer* m_requestTimeoutTimer;

    QString m_host;
    quint16 m_port;

    core::ConnectionState m_connectionState;
    bool m_requestInFlight;
    QModbusReply* m_currentReply;
    int m_consecutiveTimeouts;
    static const int MAX_TIMEOUTS = 3;
};

}

#endif
