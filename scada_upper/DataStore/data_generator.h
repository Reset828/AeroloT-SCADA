#ifndef DATASTORE_DATA_GENERATOR_H
#define DATASTORE_DATA_GENERATOR_H

#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QRandomGenerator>
#include <cmath>
#include "database_manager.h"

namespace DataStore {

class DataGenerator : public QObject
{
    Q_OBJECT
public:
    explicit DataGenerator(QObject* parent = nullptr) : QObject(parent) {}

    // 生成从 startDate 开始的历史数据，持续 days 天
    Q_INVOKABLE static bool generateHistoricalData(int deviceId, const QDate& startDate, int days) {
        QSqlDatabase db = DatabaseManager::instance().getDatabase();
        if (!db.isOpen()) {
            qCritical() << "Database is not open";
            return false;
        }

        QSqlQuery query(db);
        
        // 开启事务
        if (!query.exec("BEGIN TRANSACTION")) {
            qCritical() << "Failed to begin transaction:" << query.lastError().text();
            return false;
        }

        int recordsPerDay = 1728; // 每 50 秒一个数据点，每天约 1728 条
        QDateTime currentDateTime = startDate.startOfDay();
        
        for (int day = 0; day < days; ++day) {
            for (int i = 0; i < recordsPerDay; ++i) {
                double pressure = 7.0 + QRandomGenerator::global()->bounded(200) / 100.0; // 7.0-9.0 bar
                double temperature = 75.0 + QRandomGenerator::global()->bounded(1000) / 100.0; // 75-85°C
                double current = 100.0 + QRandomGenerator::global()->bounded(2000) / 100.0; // 100-120A
                
                qint64 timestamp = currentDateTime.toMSecsSinceEpoch();
                QString historyTime = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");
                
                QString sql = "INSERT INTO history_data (timestamp, history_time, device_id, device_type, pressure, temperature, status, phase_current) "
                              "VALUES (:timestamp, :history_time, :device_id, :device_type, :pressure, :temperature, :status, :phase_current)";
                query.prepare(sql);
                query.bindValue(":timestamp", timestamp);
                query.bindValue(":history_time", historyTime);
                query.bindValue(":device_id", deviceId);
                query.bindValue(":device_type", 0);  // 0 表示压缩机类型
                query.bindValue(":pressure", pressure);
                query.bindValue(":temperature", temperature);
                query.bindValue(":status", 1);  // 运行状态
                query.bindValue(":phase_current", current);
                
                if (!query.exec()) {
                    qCritical() << "Failed to insert record:" << query.lastError().text();
                    query.exec("ROLLBACK");
                    return false;
                }
                
                currentDateTime = currentDateTime.addSecs(50);
            }
        }
        
        if (!query.exec("COMMIT")) {
            qCritical() << "Failed to commit transaction:" << query.lastError().text();
            return false;
        }
        
        qDebug() << "Generated" << days << "days of historical data for device" << deviceId;
        return true;
    }

    // 为所有 5 台设备生成历史数据
    Q_INVOKABLE static bool generateAllDevicesData(const QDate& startDate, int days) {
        for (int deviceId = 1; deviceId <= 5; ++deviceId) {
            if (!generateHistoricalData(deviceId, startDate, days)) {
                return false;
            }
        }
        return true;
    }
};

}

#endif
