#include <QtTest>
#include <QDataStream>
#include <QHostAddress>
#include <QModbusDataUnit>
#include <QModbusPdu>
#include <QModbusTcpServer>
#include <QTcpServer>

#include "ModbusEngine/modbus_worker.h"
#include "ModbusEngine/modbus_engine.h"

namespace {

struct RequestRecord
{
    QModbusPdu::FunctionCode functionCode;
    quint16 startAddress;
};

class RecordingModbusServer : public QModbusTcpServer
{
public:
    QVector<RequestRecord> requests;

protected:
    QModbusResponse processRequest(const QModbusPdu& request) override
    {
        quint16 startAddress = 0;
        QDataStream stream(request.data());
        stream >> startAddress;
        requests.append({request.functionCode(), startAddress});
        return QModbusTcpServer::processRequest(request);
    }
};

ModbusEngine::Command readCommand(int address)
{
    ModbusEngine::Command command{};
    command.type = ModbusEngine::CommandType::Read;
    command.registerType = core::ModbusRegisterType::Holding;
    command.slaveId = 1;
    command.startAddress = address;
    command.quantity = 1;
    return command;
}

ModbusEngine::Command writeCommand(int address, quint16 value)
{
    ModbusEngine::Command command = readCommand(address);
    command.type = ModbusEngine::CommandType::Write;
    command.writeValues.append(value);
    return command;
}

quint16 reserveLocalPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;

    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

bool startServer(RecordingModbusServer& server, quint16 port)
{
    QModbusDataUnitMap map;
    QList<quint16> values;
    for (quint16 value = 1000; value < 1064; ++value)
        values.append(value);
    map.insert(QModbusDataUnit::HoldingRegisters,
               QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, values));
    server.setServerAddress(1);
    if (!server.setMap(map))
        return false;

    server.setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                  QHostAddress(QHostAddress::LocalHost).toString());
    server.setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    return server.connectDevice();
}

}

class ModbusWorkerTests : public QObject
{
    Q_OBJECT

private slots:
    void queuedWritesPrecedeReadsAndResponsesAreDelivered();
    void reconnectsAfterServerDisconnect();
    void engineFacadeQueuesCommandsAndForwardsSignals();
};

void ModbusWorkerTests::queuedWritesPrecedeReadsAndResponsesAreDelivered()
{
    RecordingModbusServer server;
    const quint16 port = reserveLocalPort();
    QVERIFY(port != 0);
    QVERIFY(startServer(server, port));
    QTRY_COMPARE_WITH_TIMEOUT(server.state(), QModbusDevice::ConnectedState, 1000);

    ModbusEngine::ModbusWorker worker;
    QVector<core::ConnectionState> states;
    QVector<QVector<quint16>> responses;
    connect(&worker, &ModbusEngine::ModbusWorker::connectionStateChanged,
            this, [&states](core::ConnectionState state) { states.append(state); });
    connect(&worker, &ModbusEngine::ModbusWorker::dataReceived,
            this, [&responses](int, const QVector<quint16>& values,
                               QModbusDataUnit::RegisterType) {
        responses.append(values);
    });

    worker.pushReadCommand(readCommand(10));
    worker.pushWriteCommand(writeCommand(20, 2020));
    worker.pushReadCommand(readCommand(11));
    worker.pushWriteCommand(writeCommand(21, 2121));
    worker.startConnection(QStringLiteral("127.0.0.1"), port);

    QTRY_VERIFY_WITH_TIMEOUT(states.contains(core::ConnectionState::Connected), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 4, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(responses.size(), 4, 3000);

    QCOMPARE(server.requests.at(0).functionCode, QModbusPdu::WriteSingleRegister);
    QCOMPARE(server.requests.at(0).startAddress, quint16(20));
    QCOMPARE(server.requests.at(1).functionCode, QModbusPdu::WriteSingleRegister);
    QCOMPARE(server.requests.at(1).startAddress, quint16(21));
    QCOMPARE(server.requests.at(2).functionCode, QModbusPdu::ReadHoldingRegisters);
    QCOMPARE(server.requests.at(2).startAddress, quint16(10));
    QCOMPARE(server.requests.at(3).functionCode, QModbusPdu::ReadHoldingRegisters);
    QCOMPARE(server.requests.at(3).startAddress, quint16(11));
    QCOMPARE(responses.at(0), QVector<quint16>{2020});
    QCOMPARE(responses.at(1), QVector<quint16>{2121});
    QCOMPARE(responses.at(2), QVector<quint16>{1010});
    QCOMPARE(responses.at(3), QVector<quint16>{1011});

    worker.stopConnection();
    QTRY_VERIFY_WITH_TIMEOUT(states.contains(core::ConnectionState::Disconnected), 1000);
}

void ModbusWorkerTests::reconnectsAfterServerDisconnect()
{
    RecordingModbusServer server;
    const quint16 port = reserveLocalPort();
    QVERIFY(port != 0);
    QVERIFY(startServer(server, port));
    QTRY_COMPARE_WITH_TIMEOUT(server.state(), QModbusDevice::ConnectedState, 1000);

    ModbusEngine::ModbusWorker worker;
    QVector<core::ConnectionState> states;
    connect(&worker, &ModbusEngine::ModbusWorker::connectionStateChanged,
            this, [&states](core::ConnectionState state) { states.append(state); });
    worker.startConnection(QStringLiteral("127.0.0.1"), port);
    QTRY_VERIFY_WITH_TIMEOUT(states.contains(core::ConnectionState::Connected), 3000);

    server.disconnectDevice();
    QTRY_VERIFY_WITH_TIMEOUT(states.contains(core::ConnectionState::Reconnecting), 3000);

    QVERIFY(startServer(server, port));
    QTRY_COMPARE_WITH_TIMEOUT(server.state(), QModbusDevice::ConnectedState, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(states.last() == core::ConnectionState::Connected, 7000);

    worker.stopConnection();
}

void ModbusWorkerTests::engineFacadeQueuesCommandsAndForwardsSignals()
{
    RecordingModbusServer server;
    const quint16 port = reserveLocalPort();
    QVERIFY(port != 0);
    QVERIFY(startServer(server, port));
    QTRY_COMPARE_WITH_TIMEOUT(server.state(), QModbusDevice::ConnectedState, 1000);

    ModbusEngine::ModbusEngine engine;
    QVector<core::ConnectionState> states;
    QVector<QVector<quint16>> responses;
    connect(&engine, &ModbusEngine::ModbusEngine::connectionStateChanged,
            this, [&states](core::ConnectionState state) { states.append(state); });
    connect(&engine, &ModbusEngine::ModbusEngine::dataReceived,
            this, [&responses](int, const QVector<quint16>& values,
                               QModbusDataUnit::RegisterType) {
        responses.append(values);
    });

    engine.pushRead(readCommand(12));
    engine.pushWrite(writeCommand(22, 2222));
    engine.connectToHost(QStringLiteral("127.0.0.1"), port);

    QTRY_VERIFY_WITH_TIMEOUT(states.contains(core::ConnectionState::Connected), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 2, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(responses.size(), 2, 3000);

    QCOMPARE(server.requests.at(0).functionCode, QModbusPdu::WriteSingleRegister);
    QCOMPARE(server.requests.at(0).startAddress, quint16(22));
    QCOMPARE(server.requests.at(1).functionCode, QModbusPdu::ReadHoldingRegisters);
    QCOMPARE(server.requests.at(1).startAddress, quint16(12));
    QCOMPARE(responses.at(0), QVector<quint16>{2222});
    QCOMPARE(responses.at(1), QVector<quint16>{1012});

    engine.disconnectFromHost();
    QTRY_VERIFY_WITH_TIMEOUT(states.last() == core::ConnectionState::Disconnected, 1000);
}

QTEST_GUILESS_MAIN(ModbusWorkerTests)

#include "modbus_worker_tests.moc"
