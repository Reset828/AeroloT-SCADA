#ifndef MODBUSENGINE_SCAN_SCHEDULER_H
#define MODBUSENGINE_SCAN_SCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include "modbus_engine.h"
#include "register_map.h"

namespace ModbusEngine {

/**
 * @brief 扫描调度器
 * 
 * 默认0.5秒周期，每次扫描全量5个设备的所有寄存器组
 * 生成的读取命令压入ModbusEngine的命令队列
 */
class ScanScheduler : public QObject {
    Q_OBJECT
public:
    explicit ScanScheduler(ModbusEngine* engine, QObject* parent = nullptr);
    ~ScanScheduler();

    void start();
    void stop();

    // 设置扫描周期（毫秒），默认500ms
    void setScanInterval(int intervalMs);

    // 设置在线设备列表
    void setOnlineDevices(const QList<int>& deviceIds);

    int scanInterval() const { return m_scanIntervalMs; }

signals:
    void scanTriggered(int deviceCount);
    void scanError(const QString& error);

private slots:
    void onScanTimeout();

private:
    void scanAllDevices();

    ModbusEngine* m_modbusEngine;
    QTimer* m_scanTimer;
    QList<int> m_onlineDevices;
    int m_scanIntervalMs;
};

}

#endif
