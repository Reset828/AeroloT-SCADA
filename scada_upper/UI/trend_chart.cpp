#include "trend_chart.h"

#include <QDebug>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QDateTime>
#include <QtConcurrent>
#include <QFutureWatcher>

#include "../DataStore/datastore.h"
#include "../DataStore/history_data_reader.h"

namespace UI {

QHash<int, DeviceChartData> TrendChart::s_dataCache;
QCache<QString, DeviceChartData> TrendChart::s_rangeCache(MAX_CACHE_SIZE);
QMutex TrendChart::s_cacheMutex;

static int extractDeviceId(const QString &deviceId) {
    if (deviceId.startsWith("COM-", Qt::CaseInsensitive)) {
        QString numPart = deviceId.mid(4);
        bool ok = false;
        int id = numPart.toInt(&ok);
        return ok ? id : 1;
    } else if (deviceId.startsWith("TANK-", Qt::CaseInsensitive)) {
        QString numPart = deviceId.mid(5);
        bool ok = false;
        int id = numPart.toInt(&ok);
        return ok ? (id + 3) : 4;
    } else if (deviceId.startsWith("DRY-", Qt::CaseInsensitive)) {
        QString numPart = deviceId.mid(4);
        bool ok = false;
        int id = numPart.toInt(&ok);
        return ok ? (id + 4) : 5;
    } else if (deviceId.startsWith("device_", Qt::CaseInsensitive)) {
        QString numPart = deviceId.mid(7);
        bool ok = false;
        int id = numPart.toInt(&ok);
        return ok ? id : 1;
    }
    
    bool ok = false;
    int id = deviceId.toInt(&ok);
    return ok ? id : 1;
}

TrendChart::TrendChart(const QString &deviceId, QWidget *parent)
    : TrendChart(deviceId, DataStore::DataStore::instance(), parent)
{
}

TrendChart::TrendChart(const QString &deviceId,
                       DataStore::DataStore& dataStore,
                       QWidget *parent)
    : QWidget(parent)
    , m_dataStore(dataStore)
    , m_deviceId(extractDeviceId(deviceId))
    , m_plot(nullptr)
    , m_graph1(nullptr)
    , m_graph2(nullptr)
    , m_graph3(nullptr)
    , m_leftAxis(nullptr)
    , m_rightAxis(nullptr)
    , m_tracer1(nullptr)
    , m_tracer2(nullptr)
    , m_tracer3(nullptr)
    , m_label1(nullptr)
    , m_label2(nullptr)
    , m_label3(nullptr)
    , m_updateTimer(nullptr)
    , m_isRealtimeUpdating(false)
    , m_lastMouseMoveTime(QDateTime::currentDateTime())
    , m_needsReplot(false)
    , m_isProgressiveLoading(false)
    , m_cancelProgressiveLoad(false)
    , m_batchStartTimestamp(0)
    , m_batchEndTimestamp(0)
    , m_currentBatchOffset(0)
    , m_totalRecords(0)
    , m_loadedRecords(0)
    , m_progressiveTimer(nullptr)
    , m_y1Min(1e10)
    , m_y1Max(-1e10)
    , m_y2Min(1e10)
    , m_y2Max(-1e10)
    , m_y3Min(1e10)
    , m_y3Max(-1e10)
{
    m_plot = new QCustomPlot(this);
    m_plot->setOpenGl(true);
    
    m_updateTimer = new QTimer(this);
    m_progressiveTimer = new QTimer(this);
    m_progressiveTimer->setInterval(PROGRESSIVE_INTERVAL_MS);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);
    
    determineDeviceType();
    initChart();
    
    connect(m_updateTimer, &QTimer::timeout, this, &TrendChart::onTimerTimeout);
    connect(m_progressiveTimer, &QTimer::timeout, this, &TrendChart::onProgressiveTimer);
    
    connect(m_plot->xAxis, static_cast<void (QCPAxis::*)(const QCPRange &)>(&QCPAxis::rangeChanged),
            this, &TrendChart::onRangeZoom);
    connect(m_plot, &QCustomPlot::mouseMove, this, &TrendChart::onMouseMove);
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, &TrendChart::onAxisDoubleClick);
    
    m_endTime = QDateTime::currentDateTime();
    m_startTime = m_endTime.addDays(-30);
    m_visibleStartTime = m_startTime;
    m_visibleEndTime = m_endTime;
    
    m_lastReplotTime.start();
    
    m_plot->xAxis->setRange(m_startTime.toMSecsSinceEpoch() / 1000.0,
                            m_endTime.toMSecsSinceEpoch() / 1000.0);
    m_plot->replot();
    
    QTimer::singleShot(0, this, [this]() {
        loadHistoricalDataAsync(m_startTime, m_endTime);
    });
}

TrendChart::~TrendChart()
{
    m_cancelProgressiveLoad = true;
    if (m_progressiveTimer) {
        m_progressiveTimer->stop();
    }
    stopRealtimeUpdate();
}

void TrendChart::determineDeviceType()
{
    if (m_deviceId >= 1 && m_deviceId <= 3) {
        m_chartType = DeviceChartType::Compressor;
    } else if (m_deviceId == 4) {
        m_chartType = DeviceChartType::Tank;
    } else if (m_deviceId == 5) {
        m_chartType = DeviceChartType::Dryer;
    } else {
        m_chartType = DeviceChartType::Compressor;
    }
}

void TrendChart::setDeviceId(const QString &deviceId)
{
    int newId = extractDeviceId(deviceId);
    
    if (newId == m_deviceId) {
        return;
    }
    
    stopRealtimeUpdate();
    m_cancelProgressiveLoad = true;
    if (m_progressiveTimer) {
        m_progressiveTimer->stop();
    }
    m_isProgressiveLoading = false;
    
    {
        QMutexLocker locker(&m_batchMutex);
        m_pendingBatches.clear();
    }
    
    m_deviceId = newId;
    DeviceChartType oldChartType = m_chartType;
    determineDeviceType();
    
    if (oldChartType != m_chartType) {
        clearChart();
        initChart();
    }
    
    if (isDataInCache(m_startTime, m_endTime)) {
        applyCachedData();
    } else {
        m_timeData.clear();
        m_value1Data.clear();
        m_value2Data.clear();
        m_value3Data.clear();
        loadHistoricalDataAsync(m_startTime, m_endTime);
    }
}

QString TrendChart::deviceId() const
{
    return QString("device_%1").arg(m_deviceId);
}

void TrendChart::setTimeRange(const QDateTime &start, const QDateTime &end)
{
    m_startTime = start;
    m_endTime = end;
    m_visibleStartTime = start;
    m_visibleEndTime = end;
    loadHistoricalDataAsync(start, end);
}

void TrendChart::refreshData()
{
    loadHistoricalDataAsync(m_visibleStartTime, m_visibleEndTime);
}

void TrendChart::startRealtimeUpdate()
{
    if (!m_isRealtimeUpdating) {
        m_isRealtimeUpdating = true;
        m_updateTimer->start(UPDATE_INTERVAL_MS);
    }
}

void TrendChart::stopRealtimeUpdate()
{
    if (m_isRealtimeUpdating) {
        m_isRealtimeUpdating = false;
        m_updateTimer->stop();
    }
}

bool TrendChart::isRealtimeUpdating() const
{
    return m_isRealtimeUpdating;
}

void TrendChart::clearCache()
{
    QMutexLocker locker(&s_cacheMutex);
    s_dataCache.clear();
    s_rangeCache.clear();
}

void TrendChart::initChart()
{
    setupChartStyle();
    
    switch (m_chartType) {
    case DeviceChartType::Compressor:
        m_graph1 = m_plot->addGraph();
        m_graph1->setName(QString::fromUtf8("排气温度 (°C)"));
        m_graph1->setPen(QPen(QColor(255, 100, 100), 2));
        m_graph1->setAdaptiveSampling(true);
        
        m_graph2 = m_plot->addGraph();
        m_graph2->setName(QString::fromUtf8("排气压力 (bar)"));
        m_graph2->setPen(QPen(QColor(100, 150, 255), 2));
        m_graph2->setAdaptiveSampling(true);
        
        m_graph3 = m_plot->addGraph();
        m_graph3->setName(QString::fromUtf8("运行电流 (A)"));
        m_graph3->setPen(QPen(QColor(100, 255, 150), 2));
        m_graph3->setAdaptiveSampling(true);
        break;
        
    case DeviceChartType::Tank:
        m_graph1 = m_plot->addGraph();
        m_graph1->setName(QString::fromUtf8("罐压 (bar)"));
        m_graph1->setPen(QPen(QColor(100, 150, 255), 2));
        m_graph1->setAdaptiveSampling(true);
        
        m_graph2 = m_plot->addGraph();
        m_graph2->setName(QString::fromUtf8("温度 (°C)"));
        m_graph2->setPen(QPen(QColor(255, 100, 100), 2));
        m_graph2->setAdaptiveSampling(true);
        break;
        
    case DeviceChartType::Dryer:
        m_graph1 = m_plot->addGraph();
        m_graph1->setName(QString::fromUtf8("进口压力 (bar)"));
        m_graph1->setPen(QPen(QColor(100, 150, 255), 2));
        m_graph1->setAdaptiveSampling(true);
        
        m_graph2 = m_plot->addGraph();
        m_graph2->setName(QString::fromUtf8("出口压力 (bar)"));
        m_graph2->setPen(QPen(QColor(100, 200, 255), 2));
        m_graph2->setAdaptiveSampling(true);
        
        m_graph3 = m_plot->addGraph();
        m_graph3->setName(QString::fromUtf8("露点 (°C)"));
        m_graph3->setPen(QPen(QColor(255, 200, 100), 2));
        m_graph3->setAdaptiveSampling(true);
        break;
    }
    
    m_leftAxis = m_plot->yAxis;
    m_leftAxis->setBasePen(QPen(Qt::white, 2));
    m_leftAxis->setTickPen(QPen(Qt::white, 1));
    m_leftAxis->setSubTickPen(QPen(Qt::white, 1));
    m_leftAxis->setTickLabelColor(Qt::white);
    m_leftAxis->setLabelColor(Qt::white);
    
    switch (m_chartType) {
    case DeviceChartType::Compressor:
        m_leftAxis->setLabel(QString::fromUtf8("温度 (°C) / 压力 (bar)"));
        break;
    case DeviceChartType::Tank:
        m_leftAxis->setLabel(QString::fromUtf8("压力 (bar) / 温度 (°C)"));
        break;
    case DeviceChartType::Dryer:
        m_leftAxis->setLabel(QString::fromUtf8("压力 (bar)"));
        break;
    }
    
    m_rightAxis = m_plot->yAxis2;
    m_rightAxis->setBasePen(QPen(Qt::white, 2));
    m_rightAxis->setTickPen(QPen(Qt::white, 1));
    m_rightAxis->setSubTickPen(QPen(Qt::white, 1));
    m_rightAxis->setTickLabelColor(Qt::white);
    m_rightAxis->setLabelColor(Qt::white);
    m_rightAxis->setVisible(false);
    
    QCPAxis *xAxis = m_plot->xAxis;
    xAxis->setBasePen(QPen(Qt::white, 2));
    xAxis->setTickPen(QPen(Qt::white, 1));
    xAxis->setSubTickPen(QPen(Qt::white, 1));
    xAxis->setTickLabelColor(Qt::white);
    xAxis->setLabelColor(Qt::white);
    xAxis->setTickLabelRotation(-45);
    
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("MM-dd hh:mm");
    xAxis->setTicker(dateTicker);
    
    m_plot->legend->setVisible(true);
    m_plot->legend->setTextColor(Qt::white);
    m_plot->legend->setBrush(QColor(0, 0, 0, 150));
    m_plot->legend->setBorderPen(QPen(Qt::NoPen));
    
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    
    m_tracer1 = new QCPItemTracer(m_plot);
    m_tracer1->setVisible(false);
    m_tracer1->setStyle(QCPItemTracer::tsCrosshair);
    m_tracer1->setPen(QPen(QColor(255, 255, 255, 200), 1));
    
    m_label1 = new QCPItemText(m_plot);
    m_label1->setVisible(false);
    m_label1->setColor(QColor(255, 255, 255, 200));
    m_label1->setBrush(QColor(0, 0, 0, 180));
}

void TrendChart::setupChartStyle()
{
    m_plot->setBackground(QBrush(QColor(15, 37, 64)));
}

void TrendChart::loadHistoricalData()
{
    loadHistoricalDataAsync(m_visibleStartTime, m_visibleEndTime);
}

void TrendChart::loadHistoricalDataAsync(const QDateTime &start, const QDateTime &end)
{
    QString cacheKey = QString("%1_%2_%3").arg(m_deviceId).arg(start.toMSecsSinceEpoch()).arg(end.toMSecsSinceEpoch());
    
    {
        QMutexLocker locker(&s_cacheMutex);
        if (s_rangeCache.contains(cacheKey)) {
            DeviceChartData *cached = s_rangeCache.object(cacheKey);
            if (cached && (QDateTime::currentMSecsSinceEpoch() - cached->loadTime) < CACHE_EXPIRE_MS) {
                m_timeData = cached->timeData;
                m_value1Data = cached->value1Data;
                m_value2Data = cached->value2Data;
                m_value3Data = cached->value3Data;
                updateChart();
                return;
            }
        }
    }
    
    m_cancelProgressiveLoad = true;
    if (m_progressiveTimer) {
        m_progressiveTimer->stop();
    }
    
    {
        QMutexLocker locker(&m_batchMutex);
        m_pendingBatches.clear();
    }
    
    m_isProgressiveLoading = false;
    
    m_timeData.clear();
    m_value1Data.clear();
    m_value2Data.clear();
    m_value3Data.clear();
    
    if (m_graph1) m_graph1->data()->clear();
    if (m_graph2) m_graph2->data()->clear();
    if (m_graph3) m_graph3->data()->clear();
    
    m_y1Min = 1e10; m_y1Max = -1e10;
    m_y2Min = 1e10; m_y2Max = -1e10;
    m_y3Min = 1e10; m_y3Max = -1e10;
    
    m_plot->xAxis->setRange(start.toMSecsSinceEpoch() / 1000.0,
                            end.toMSecsSinceEpoch() / 1000.0);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    
    m_batchStartTimestamp = start.toMSecsSinceEpoch();
    m_batchEndTimestamp = end.toMSecsSinceEpoch();
    m_currentBatchOffset = 0;
    m_totalRecords = 0;
    m_loadedRecords = 0;
    m_cancelProgressiveLoad = false;

    const QString dbPath = m_dataStore.databasePath();
    const int deviceId = m_deviceId;
    const qint64 batchStart = m_batchStartTimestamp;
    const qint64 batchEnd = m_batchEndTimestamp;
    auto* watcher = new QFutureWatcher<DataStore::HistoryTrendCountResult>(this);
    connect(watcher, &QFutureWatcher<DataStore::HistoryTrendCountResult>::finished,
            this, [this, watcher]() {
        const DataStore::HistoryTrendCountResult result = watcher->result();
        watcher->deleteLater();

        if (result.failure == DataStore::HistoryReadFailure::Open) {
            qCritical() << "Failed to open database for count:" << result.error;
            return;
        }
        if (result.failure == DataStore::HistoryReadFailure::Prepare) {
            qCritical() << "Failed to prepare count query:" << result.error;
            return;
        }
        if (result.failure == DataStore::HistoryReadFailure::Execute) {
            qCritical() << "Failed to execute count query:" << result.error;
            return;
        }

        const int totalRecords = result.pointCount;
        qDebug() << "Total records for device" << m_deviceId << ":" << totalRecords;

        if (m_cancelProgressiveLoad) return;

        m_totalRecords = totalRecords;

        if (totalRecords == 0) {
            m_plot->replot(QCustomPlot::rpQueuedReplot);
            emit dataLoaded(0);
            return;
        }

        m_isProgressiveLoading = true;
        m_progressiveTimer->start(PROGRESSIVE_INTERVAL_MS);
        fetchNextBatch();
    });

    watcher->setFuture(QtConcurrent::run([dbPath, deviceId, batchStart, batchEnd]() {
        return DataStore::HistoryDataReader::countTrendData(
            dbPath, deviceId, batchStart, batchEnd);
    }));
}

void TrendChart::fetchNextBatch()
{
    if (m_cancelProgressiveLoad) return;
    
    qint64 currentOffset = m_currentBatchOffset;
    qint64 batchStart = m_batchStartTimestamp;
    qint64 batchEnd = m_batchEndTimestamp;
    int deviceId = m_deviceId;
    DeviceChartType chartType = m_chartType;
    QString deviceType;
    switch (chartType) {
    case DeviceChartType::Compressor:
        deviceType = QStringLiteral("COM");
        break;
    case DeviceChartType::Tank:
        deviceType = QStringLiteral("TANK");
        break;
    case DeviceChartType::Dryer:
        deviceType = QStringLiteral("DRY");
        break;
    default:
        deviceType = QStringLiteral("COM");
        break;
    }

    const QString dbPath = m_dataStore.databasePath();
    auto* watcher = new QFutureWatcher<DataStore::HistoryTrendBatchResult>(this);
    connect(watcher, &QFutureWatcher<DataStore::HistoryTrendBatchResult>::finished,
            this, [this, watcher]() {
        const DataStore::HistoryTrendBatchResult result = watcher->result();
        watcher->deleteLater();

        if (result.failure == DataStore::HistoryReadFailure::Open) {
            qCritical() << "Failed to open database for batch:" << result.error;
            return;
        }
        if (result.failure == DataStore::HistoryReadFailure::Prepare) {
            qCritical() << "Failed to prepare batch query:" << result.error;
            return;
        }
        if (result.failure == DataStore::HistoryReadFailure::Execute) {
            qCritical() << "Failed to execute batch query:" << result.error;
            return;
        }

        if (m_cancelProgressiveLoad) return;

        DataBatch batch;
        batch.keyData = result.keys;
        batch.value1Data = result.values1;
        batch.value2Data = result.values2;
        batch.value3Data = result.values3;
        batch.isLastBatch = result.isLastBatch;

        QMutexLocker locker(&m_batchMutex);
        m_pendingBatches.enqueue(batch);
    });

    watcher->setFuture(QtConcurrent::run(
        [dbPath, currentOffset, batchStart, batchEnd, deviceId, deviceType]() {
            return DataStore::HistoryDataReader::readTrendBatch(
                dbPath, deviceId, batchStart, batchEnd, deviceType,
                currentOffset, BATCH_SIZE);
        }));
}

void TrendChart::onProgressiveTimer()
{
    if (m_cancelProgressiveLoad) {
        m_progressiveTimer->stop();
        m_isProgressiveLoading = false;
        return;
    }
    
    DataBatch batch;
    bool hasBatch = false;
    
    {
        QMutexLocker locker(&m_batchMutex);
        if (!m_pendingBatches.isEmpty()) {
            batch = m_pendingBatches.dequeue();
            hasBatch = true;
        }
    }
    
    if (hasBatch) {
        applyBatchToGraph(batch);
        
        m_currentBatchOffset += BATCH_SIZE;
        m_loadedRecords += batch.keyData.size();
        
        if (!batch.isLastBatch) {
            fetchNextBatch();
        } else {
            finalizeProgressiveLoad();
        }
    }
}

void TrendChart::applyBatchToGraph(const DataBatch &batch)
{
    if (batch.keyData.isEmpty()) return;
    
    if (m_graph1) {
        for (int i = 0; i < batch.keyData.size(); i++) {
            m_graph1->addData(batch.keyData[i], batch.value1Data[i]);
        }
    }
    if (m_graph2) {
        for (int i = 0; i < batch.keyData.size(); i++) {
            m_graph2->addData(batch.keyData[i], batch.value2Data[i]);
        }
    }
    if (m_graph3) {
        for (int i = 0; i < batch.keyData.size(); i++) {
            m_graph3->addData(batch.keyData[i], batch.value3Data[i]);
        }
    }
    
    for (int i = 0; i < batch.keyData.size(); i++) {
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(batch.keyData[i] * 1000.0));
        m_timeData.append(dt);
        m_value1Data.append(batch.value1Data[i]);
        m_value2Data.append(batch.value2Data[i]);
        m_value3Data.append(batch.value3Data[i]);
        
        m_y1Min = qMin(m_y1Min, batch.value1Data[i]);
        m_y1Max = qMax(m_y1Max, batch.value1Data[i]);
        m_y2Min = qMin(m_y2Min, batch.value2Data[i]);
        m_y2Max = qMax(m_y2Max, batch.value2Data[i]);
        if (m_graph3) {
            m_y3Min = qMin(m_y3Min, batch.value3Data[i]);
            m_y3Max = qMax(m_y3Max, batch.value3Data[i]);
        }
    }
    
    resetYAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void TrendChart::finalizeProgressiveLoad()
{
    m_progressiveTimer->stop();
    m_isProgressiveLoading = false;
    
    if (!m_timeData.isEmpty()) {
        DeviceChartData *cacheEntry = new DeviceChartData{
            m_timeData, m_value1Data, m_value2Data, m_value3Data,
            {QDateTime::fromMSecsSinceEpoch(m_batchStartTimestamp),
             QDateTime::fromMSecsSinceEpoch(m_batchEndTimestamp)},
            QDateTime::currentMSecsSinceEpoch()
        };
        
        QString cacheKey = QString("%1_%2_%3").arg(m_deviceId).arg(m_batchStartTimestamp).arg(m_batchEndTimestamp);
        QMutexLocker locker(&s_cacheMutex);
        s_rangeCache.insert(cacheKey, cacheEntry);
    }
    
    resetYAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    emit dataLoaded(m_timeData.size());
    
    qDebug() << "Progressive load complete for device" << m_deviceId
             << ", total points:" << m_timeData.size();
}

void TrendChart::resetYAxisRange()
{
    if (m_y1Min >= m_y1Max) {
        m_y1Min = 0;
        m_y1Max = 100;
    }
    if (m_y2Min >= m_y2Max) {
        m_y2Min = 0;
        m_y2Max = 100;
    }
    
    double padding1 = (m_y1Max - m_y1Min) * 0.1;
    if (padding1 < 0.01) padding1 = 1.0;
    m_leftAxis->setRange(m_y1Min - padding1, m_y1Max + padding1);
    
    if (m_graph3 && m_y3Min < m_y3Max) {
        double padding3 = (m_y3Max - m_y3Min) * 0.1;
        if (padding3 < 0.01) padding3 = 1.0;
        m_rightAxis->setRange(m_y3Min - padding3, m_y3Max + padding3);
        m_rightAxis->setVisible(true);
    } else {
        m_rightAxis->setVisible(false);
    }
}

void TrendChart::updateChart()
{
    if (!m_graph1 || !m_graph2) return;
    
    m_needsReplot = true;
    
    if (m_lastReplotTime.elapsed() < 50) {
        QTimer::singleShot(50, this, [this]() {
            if (m_needsReplot) {
                updateChartFast();
            }
        });
        return;
    }
    
    updateChartFast();
}

void TrendChart::updateChartFast()
{
    if (!m_graph1 || !m_graph2) return;
    
    QVector<double> keyData;
    keyData.reserve(m_timeData.size());
    for (const QDateTime &dt : m_timeData) {
        keyData.append(dt.toMSecsSinceEpoch() / 1000.0);
    }
    
    m_graph1->setData(keyData, m_value1Data);
    m_graph2->setData(keyData, m_value2Data);
    if (m_graph3) {
        m_graph3->setData(keyData, m_value3Data);
    }
    
    if (!m_timeData.isEmpty()) {
        double minKey = keyData.first();
        double maxKey = keyData.last();
        m_plot->xAxis->setRange(minKey, maxKey);
        
        m_y1Min = 1e10; m_y1Max = -1e10;
        m_y2Min = 1e10; m_y2Max = -1e10;
        m_y3Min = 1e10; m_y3Max = -1e10;
        
        for (double v : m_value1Data) { m_y1Min = qMin(m_y1Min, v); m_y1Max = qMax(m_y1Max, v); }
        for (double v : m_value2Data) { m_y2Min = qMin(m_y2Min, v); m_y2Max = qMax(m_y2Max, v); }
        if (m_graph3) {
            for (double v : m_value3Data) { m_y3Min = qMin(m_y3Min, v); m_y3Max = qMax(m_y3Max, v); }
        }
        
        resetYAxisRange();
    }
    
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    m_needsReplot = false;
    m_lastReplotTime.restart();
}

void TrendChart::addRealtimeDataPoint()
{
    QDateTime now = QDateTime::currentDateTime();

    const DataStore::LatestHistoryValues latest =
        DataStore::HistoryDataReader::readLatestValues(
            m_dataStore.databasePath(), m_deviceId);
    if (latest.failure == DataStore::HistoryReadFailure::Open) {
        qCritical() << "Failed to open trend chart realtime database connection:" << latest.error;
        return;
    }
    if (!latest.found) {
        return;
    }

    double value1 = 0.0, value2 = 0.0, value3 = 0.0;
    double key = now.toMSecsSinceEpoch() / 1000.0;
    
    switch (m_chartType) {
    case DeviceChartType::Compressor:
        value1 = latest.temperature;
        value2 = latest.pressure;
        value3 = latest.phaseCurrent;
        break;
    case DeviceChartType::Tank:
        value1 = latest.pressure;
        value2 = latest.temperature;
        break;
    case DeviceChartType::Dryer:
        value1 = latest.pressure;
        value2 = latest.outletPressure;
        value3 = latest.dewPoint;
        break;
    }
    
    m_timeData.append(now);
    m_value1Data.append(value1);
    m_value2Data.append(value2);
    if (m_chartType == DeviceChartType::Compressor || m_chartType == DeviceChartType::Dryer) {
        m_value3Data.append(value3);
    }
    
    if (m_graph1) m_graph1->addData(key, value1);
    if (m_graph2) m_graph2->addData(key, value2);
    if (m_graph3 && (m_chartType == DeviceChartType::Compressor || m_chartType == DeviceChartType::Dryer)) {
        m_graph3->addData(key, value3);
    }
    
    m_y1Min = qMin(m_y1Min, value1);
    m_y1Max = qMax(m_y1Max, value1);
    m_y2Min = qMin(m_y2Min, value2);
    m_y2Max = qMax(m_y2Max, value2);
    if (m_graph3) {
        m_y3Min = qMin(m_y3Min, value3);
        m_y3Max = qMax(m_y3Max, value3);
    }
    
    cleanupOldData();
    
    if (m_isRealtimeUpdating && isVisible()) {
        resetYAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void TrendChart::cleanupOldData()
{
    while (m_timeData.size() > MAX_DATA_POINTS) {
        m_timeData.removeFirst();
        m_value1Data.removeFirst();
        m_value2Data.removeFirst();
        if (!m_value3Data.isEmpty()) {
            m_value3Data.removeFirst();
        }
    }
}

void TrendChart::onTimerTimeout()
{
    addRealtimeDataPoint();
}

void TrendChart::onMouseMove(QMouseEvent *event)
{
    QDateTime now = QDateTime::currentDateTime();
    if (now.msecsTo(m_lastMouseMoveTime) < MOUSE_MOVE_THROTTLE_MS) {
        return;
    }
    m_lastMouseMoveTime = now;
    
    double x = m_plot->xAxis->pixelToCoord(event->pos().x());
    
    if (m_tracer1 && m_graph1 && m_graph1->data()->size() > 0) {
        m_tracer1->setVisible(true);
        m_tracer1->setGraphKey(x);
        m_tracer1->updatePosition();
        
        m_label1->setVisible(true);
        m_label1->position->setParentAnchor(m_tracer1->position);
        
        double yValue = m_tracer1->position->coords().y();
        m_label1->setText(QString::fromUtf8("值：%1").arg(yValue, 0, 'f', 2));
        m_label1->position->setCoords(10, -10);
    }
    
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void TrendChart::onRangeZoom(QCPRange newRange)
{
    double rangeDays = newRange.size() / (60.0 * 60.0 * 24.0);
    
    if (rangeDays < MIN_ZOOM_DAYS || rangeDays > MAX_ZOOM_DAYS) {
        double center = newRange.center();
        double newSize = qBound(MIN_ZOOM_DAYS * 24.0 * 60.0 * 60.0,
                               newRange.size(),
                               MAX_ZOOM_DAYS * 24.0 * 60.0 * 60.0);
        m_plot->xAxis->setRange(center - newSize/2, center + newSize/2);
        newRange = m_plot->xAxis->range();
    }
    
    m_visibleStartTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(newRange.lower * 1000.0));
    m_visibleEndTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(newRange.upper * 1000.0));
    
    if (!isDataInCache(m_visibleStartTime, m_visibleEndTime)) {
        loadHistoricalDataAsync(m_visibleStartTime, m_visibleEndTime);
    } else {
        DeviceChartData cached = getCachedData(m_visibleStartTime, m_visibleEndTime);
        m_timeData = cached.timeData;
        m_value1Data = cached.value1Data;
        m_value2Data = cached.value2Data;
        m_value3Data = cached.value3Data;
        updateChart();
    }
}

void TrendChart::onAxisDoubleClick()
{
    m_visibleStartTime = m_startTime;
    m_visibleEndTime = m_endTime;
    m_plot->xAxis->setRange(m_startTime.toMSecsSinceEpoch() / 1000.0, m_endTime.toMSecsSinceEpoch() / 1000.0);
    loadHistoricalDataAsync(m_startTime, m_endTime);
}

void TrendChart::clearChart()
{
    m_plot->clearGraphs();
    m_graph1 = nullptr;
    m_graph2 = nullptr;
    m_graph3 = nullptr;
    
    m_timeData.clear();
    m_value1Data.clear();
    m_value2Data.clear();
    m_value3Data.clear();
    
    m_y1Min = 1e10; m_y1Max = -1e10;
    m_y2Min = 1e10; m_y2Max = -1e10;
    m_y3Min = 1e10; m_y3Max = -1e10;
}

void TrendChart::applyCachedData()
{
    QMutexLocker locker(&s_cacheMutex);
    if (s_dataCache.contains(m_deviceId)) {
        const DeviceChartData &cache = s_dataCache[m_deviceId];
        m_timeData = cache.timeData;
        m_value1Data = cache.value1Data;
        m_value2Data = cache.value2Data;
        m_value3Data = cache.value3Data;
    }
    updateChart();
}

bool TrendChart::isDataInCache(const QDateTime &start, const QDateTime &end) const
{
    QMutexLocker locker(&s_cacheMutex);
    QString cacheKey = QString("%1_%2_%3").arg(m_deviceId).arg(start.toMSecsSinceEpoch()).arg(end.toMSecsSinceEpoch());
    return s_rangeCache.contains(cacheKey);
}

DeviceChartData TrendChart::getCachedData(const QDateTime &start, const QDateTime &end) const
{
    QMutexLocker locker(&s_cacheMutex);
    QString cacheKey = QString("%1_%2_%3").arg(m_deviceId).arg(start.toMSecsSinceEpoch()).arg(end.toMSecsSinceEpoch());
    if (s_rangeCache.contains(cacheKey)) {
        return *s_rangeCache.object(cacheKey);
    }
    return DeviceChartData{};
}

void TrendChart::cacheData(const DeviceChartData &data)
{
    QMutexLocker locker(&s_cacheMutex);
    QString cacheKey = QString("%1_%2_%3").arg(m_deviceId)
                                          .arg(data.timeRange.start.toMSecsSinceEpoch())
                                          .arg(data.timeRange.end.toMSecsSinceEpoch());
    DeviceChartData *cacheEntry = new DeviceChartData(data);
    s_rangeCache.insert(cacheKey, cacheEntry);
}

}
