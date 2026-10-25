#ifndef TREND_CHART_H
#define TREND_CHART_H

#include <QWidget>
#include <QVector>
#include <QTimer>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QCache>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QElapsedTimer>
#include <atomic>

#include "qcustomplot.h"

namespace DataStore { class DataStore; }

namespace UI {

enum class DeviceChartType {
    Compressor,
    Tank,
    Dryer
};

struct TimeRange {
    QDateTime start;
    QDateTime end;
    
    bool operator==(const TimeRange &other) const {
        return start == other.start && end == other.end;
    }
};

struct DeviceChartData {
    QVector<QDateTime> timeData;
    QVector<double> value1Data;
    QVector<double> value2Data;
    QVector<double> value3Data;
    TimeRange timeRange;
    qint64 loadTime;
};

struct DataBatch {
    QVector<double> keyData;
    QVector<double> value1Data;
    QVector<double> value2Data;
    QVector<double> value3Data;
    bool isLastBatch;
};

class TrendChart : public QWidget
{
    Q_OBJECT

public:
    explicit TrendChart(const QString &deviceId, QWidget *parent = nullptr);
    TrendChart(const QString &deviceId, DataStore::DataStore& dataStore,
               QWidget *parent = nullptr);
    ~TrendChart();

    void setDeviceId(const QString &deviceId);
    QString deviceId() const;
    
    void setTimeRange(const QDateTime &start, const QDateTime &end);
    void refreshData();
    
    void startRealtimeUpdate();
    void stopRealtimeUpdate();
    bool isRealtimeUpdating() const;
    
    static void clearCache();

signals:
    void dataLoaded(int pointCount);
    void requestRangeData(const QDateTime &start, const QDateTime &end);

private slots:
    void onTimerTimeout();
    void onMouseMove(QMouseEvent *event);
    void onRangeZoom(QCPRange newRange);
    void onAxisDoubleClick();
    void onProgressiveTimer();

private:
    void initChart();
    void setupChartStyle();
    void loadHistoricalData();
    void loadHistoricalDataAsync(const QDateTime &start, const QDateTime &end);
    void fetchNextBatch();
    void applyBatchToGraph(const DataBatch &batch);
    void finalizeProgressiveLoad();
    void determineDeviceType();
    void updateChart();
    void updateChartFast();
    void addRealtimeDataPoint();
    void clearChart();
    void applyCachedData();
    void cleanupOldData();
    bool isDataInCache(const QDateTime &start, const QDateTime &end) const;
    DeviceChartData getCachedData(const QDateTime &start, const QDateTime &end) const;
    void cacheData(const DeviceChartData &data);
    void resetYAxisRange();
    
    DataStore::DataStore& m_dataStore;
    int m_deviceId;
    DeviceChartType m_chartType;
    QCustomPlot *m_plot;
    
    QVector<QDateTime> m_timeData;
    QVector<double> m_value1Data;
    QVector<double> m_value2Data;
    QVector<double> m_value3Data;
    
    QCPGraph *m_graph1;
    QCPGraph *m_graph2;
    QCPGraph *m_graph3;
    
    QCPAxis *m_leftAxis;
    QCPAxis *m_rightAxis;
    
    QCPItemTracer *m_tracer1;
    QCPItemTracer *m_tracer2;
    QCPItemTracer *m_tracer3;
    QCPItemText *m_label1;
    QCPItemText *m_label2;
    QCPItemText *m_label3;
    
    QTimer *m_updateTimer;
    QDateTime m_startTime;
    QDateTime m_endTime;
    QDateTime m_visibleStartTime;
    QDateTime m_visibleEndTime;
    
    static constexpr int UPDATE_INTERVAL_MS = 1000;
    static constexpr int MAX_DATA_POINTS = 500;
    static constexpr int CACHE_EXPIRE_MS = 10000;
    static constexpr int MOUSE_MOVE_THROTTLE_MS = 100;
    static constexpr int MAX_CACHE_SIZE = 50;
    static constexpr int MIN_ZOOM_DAYS = 1;
    static constexpr int MAX_ZOOM_DAYS = 30;
    static constexpr int BATCH_SIZE = 1000;
    static constexpr int PROGRESSIVE_INTERVAL_MS = 16;
    
    bool m_isRealtimeUpdating;
    QDateTime m_lastDataTime;
    QDateTime m_lastMouseMoveTime;
    bool m_needsReplot;
    QElapsedTimer m_lastReplotTime;
    QQueue<QDateTime> m_replotQueue;
    
    std::atomic<bool> m_isProgressiveLoading;
    std::atomic<bool> m_cancelProgressiveLoad;
    qint64 m_batchStartTimestamp;
    qint64 m_batchEndTimestamp;
    qint64 m_currentBatchOffset;
    int m_totalRecords;
    int m_loadedRecords;
    QTimer *m_progressiveTimer;
    QQueue<DataBatch> m_pendingBatches;
    QMutex m_batchMutex;
    double m_y1Min;
    double m_y1Max;
    double m_y2Min;
    double m_y2Max;
    double m_y3Min;
    double m_y3Max;
    
    static QHash<int, DeviceChartData> s_dataCache;
    static QCache<QString, DeviceChartData> s_rangeCache;
    static QMutex s_cacheMutex;
};

}

#endif
