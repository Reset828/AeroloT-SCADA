#include <QtTest>

#include "ModbusEngine/command_queue.h"

namespace {

ModbusEngine::Command command(ModbusEngine::CommandType type, int address)
{
    ModbusEngine::Command result{};
    result.type = type;
    result.registerType = core::ModbusRegisterType::Holding;
    result.slaveId = 1;
    result.startAddress = address;
    result.quantity = 1;
    return result;
}

}

class CommandQueueTests : public QObject
{
    Q_OBJECT

private slots:
    void writesAreReturnedBeforeReads();
    void orderWithinEachQueueIsFifo();
    void pushMethodsAssignQueuePriority();
    void clearRemovesPendingCommands();
};

void CommandQueueTests::writesAreReturnedBeforeReads()
{
    ModbusEngine::CommandQueue queue;
    queue.pushRead(command(ModbusEngine::CommandType::Read, 10));
    queue.pushWrite(command(ModbusEngine::CommandType::Write, 20));
    queue.pushRead(command(ModbusEngine::CommandType::Read, 30));

    QCOMPARE(queue.pop().startAddress, 20);
    QCOMPARE(queue.pop().startAddress, 10);
    QCOMPARE(queue.pop().startAddress, 30);
    QVERIFY(queue.isEmpty());
}

void CommandQueueTests::orderWithinEachQueueIsFifo()
{
    ModbusEngine::CommandQueue queue;
    queue.pushWrite(command(ModbusEngine::CommandType::Write, 1));
    queue.pushWrite(command(ModbusEngine::CommandType::Write, 2));
    queue.pushRead(command(ModbusEngine::CommandType::Read, 3));
    queue.pushRead(command(ModbusEngine::CommandType::Read, 4));

    QCOMPARE(queue.pop().startAddress, 1);
    QCOMPARE(queue.pop().startAddress, 2);
    QCOMPARE(queue.pop().startAddress, 3);
    QCOMPARE(queue.pop().startAddress, 4);
}

void CommandQueueTests::pushMethodsAssignQueuePriority()
{
    ModbusEngine::CommandQueue queue;
    auto read = command(ModbusEngine::CommandType::Read, 1);
    auto write = command(ModbusEngine::CommandType::Write, 2);
    read.priority = ModbusEngine::CommandPriority::High;
    write.priority = ModbusEngine::CommandPriority::Low;

    queue.pushRead(read);
    queue.pushWrite(write);

    const auto queuedWrite = queue.pop();
    QCOMPARE(static_cast<int>(queuedWrite.priority), static_cast<int>(ModbusEngine::CommandPriority::High));
    QCOMPARE(static_cast<int>(queuedWrite.type), static_cast<int>(ModbusEngine::CommandType::Write));
    const auto queuedRead = queue.pop();
    QCOMPARE(static_cast<int>(queuedRead.priority), static_cast<int>(ModbusEngine::CommandPriority::Low));
    QCOMPARE(static_cast<int>(queuedRead.type), static_cast<int>(ModbusEngine::CommandType::Read));
}

void CommandQueueTests::clearRemovesPendingCommands()
{
    ModbusEngine::CommandQueue queue;
    queue.pushWrite(command(ModbusEngine::CommandType::Write, 1));
    queue.pushRead(command(ModbusEngine::CommandType::Read, 2));

    QCOMPARE(queue.size(), 2);
    QVERIFY(queue.hasPendingWrite());
    queue.clear();
    QVERIFY(queue.isEmpty());
    QVERIFY(!queue.hasPendingWrite());
    QCOMPARE(queue.size(), 0);
}

QTEST_GUILESS_MAIN(CommandQueueTests)

#include "command_queue_tests.moc"
