#ifndef MODBUSENGINE_COMMANDQUEUE_H
#define MODBUSENGINE_COMMANDQUEUE_H

#include <deque>
#include <QObject>
#include <QMutex>
#include <QVariant>
#include "core/types.h"

namespace ModbusEngine {

enum class CommandType {
    Read,
    Write
};

enum class CommandPriority {
    Low,      // 普通读取
    High      // 写操作优先
};

struct Command {
    CommandType type;
    CommandPriority priority = CommandPriority::Low;
    core::ModbusRegisterType registerType;
    int slaveId;
    int startAddress;
    int quantity;
    QVariantList writeValues;
};

/**
 * @brief 带写优先级和读写互斥的命令队列
 * 
 * 设计原则：
 * 1. 双队列：写队列（高优先级）在前，读队列在后
 * 2. 写优先：写命令总是插到写队列末尾，读命令插到读队列末尾
 * 3. 读写互斥：正在等待写响应时，不发送新读命令
 * 4. 线程安全：所有操作受 QMutex 保护
 */
class CommandQueue : public QObject {
    Q_OBJECT
public:
    explicit CommandQueue(QObject* parent = nullptr);

    // 压入普通读取命令（低优先级，追加到读队列末尾）
    void pushRead(const Command& cmd);

    // 压入写命令（高优先级，追加到写队列末尾，写优先于所有读）
    void pushWrite(const Command& cmd);

    // 弹出下一条命令（写队列优先）
    Command pop();

    // 是否有待处理的命令
    bool isEmpty() const;

    // 是否有待处理的写命令
    bool hasPendingWrite() const;

    // 清空所有队列
    void clear();

    // 当前队列中的总命令数
    int size() const;

private:
    std::deque<Command> m_writeQueue;  // 写命令队列（高优先级）
    std::deque<Command> m_readQueue;   // 读命令队列（低优先级）
    mutable QMutex m_mutex;
};

}

#endif
