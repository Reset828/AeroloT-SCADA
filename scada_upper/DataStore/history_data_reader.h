#ifndef DATASTORE_HISTORY_DATA_READER_H
#define DATASTORE_HISTORY_DATA_READER_H

#include <QVector>
#include <QString>

namespace DataStore {

struct HistoryDataResult {
    QVector<double> keys1;
    QVector<double> values1;
    QVector<double> keys2;
    QVector<double> values2;
    QVector<double> keys3;
    QVector<double> values3;
    int pointCount{0};
    int sampledCount{0};
    QString error;
};

enum class HistoryReadFailure {
    None,
    Open,
    Prepare,
    Execute
};

struct HistoryTrendCountResult {
    int pointCount{0};
    HistoryReadFailure failure{HistoryReadFailure::None};
    QString error;
};

struct HistoryTrendBatchResult {
    QVector<double> keys;
    QVector<double> values1;
    QVector<double> values2;
    QVector<double> values3;
    bool isLastBatch{false};
    HistoryReadFailure failure{HistoryReadFailure::None};
    QString error;
};

struct LatestHistoryValues {
    bool found{false};
    double temperature{0.0};
    double pressure{0.0};
    double phaseCurrent{0.0};
    double outletPressure{0.0};
    double dewPoint{0.0};
    HistoryReadFailure failure{HistoryReadFailure::None};
    QString error;
};

class HistoryDataReader {
public:
    static HistoryDataResult read(const QString& dbPath, int deviceId,
                                  qint64 startMs, qint64 endMs,
                                  const QString& deviceType, int maxPoints);
    static HistoryTrendCountResult countTrendData(const QString& dbPath, int deviceId,
                                                   qint64 startMs, qint64 endMs);
    static HistoryTrendBatchResult readTrendBatch(const QString& dbPath, int deviceId,
                                                   qint64 startMs, qint64 endMs,
                                                   const QString& deviceType,
                                                   qint64 offset, int batchSize);
    static LatestHistoryValues readLatestValues(const QString& dbPath, int deviceId);
    static void closeCurrentThreadConnections();
};

}

#endif
