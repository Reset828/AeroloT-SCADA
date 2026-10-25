#include "command_queue.h"

namespace ModbusEngine {

CommandQueue::CommandQueue(QObject* parent)
    : QObject(parent)
{
}

void CommandQueue::pushRead(const Command& cmd) {
    QMutexLocker locker(&m_mutex);
    Command readCmd = cmd;
    readCmd.priority = CommandPriority::Low;
    m_readQueue.push_back(readCmd);
}

void CommandQueue::pushWrite(const Command& cmd) {
    QMutexLocker locker(&m_mutex);
    Command writeCmd = cmd;
    writeCmd.priority = CommandPriority::High;
    m_writeQueue.push_back(writeCmd);
}

Command CommandQueue::pop() {
    QMutexLocker locker(&m_mutex);
    Command cmd;
    // 写队列优先：先取写命令
    if (!m_writeQueue.empty()) {
        cmd = m_writeQueue.front();
        m_writeQueue.pop_front();
    } else if (!m_readQueue.empty()) {
        cmd = m_readQueue.front();
        m_readQueue.pop_front();
    }
    return cmd;
}

bool CommandQueue::isEmpty() const {
    QMutexLocker locker(&m_mutex);
    return m_writeQueue.empty() && m_readQueue.empty();
}

bool CommandQueue::hasPendingWrite() const {
    QMutexLocker locker(&m_mutex);
    return !m_writeQueue.empty();
}

void CommandQueue::clear() {
    QMutexLocker locker(&m_mutex);
    m_writeQueue.clear();
    m_readQueue.clear();
}

int CommandQueue::size() const {
    QMutexLocker locker(&m_mutex);
    return static_cast<int>(m_writeQueue.size() + m_readQueue.size());
}

}
