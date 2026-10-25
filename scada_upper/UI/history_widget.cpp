#include "history_widget.h"

#include <QDebug>
#include <QDateTime>
#include <QMessageBox>

#include "../DataStore/datastore.h"
#include "../DataStore/history_data_reader.h"

namespace UI {

HistoryDataWorker::~HistoryDataWorker()
{
    DataStore::HistoryDataReader::closeCurrentThreadConnections();
}

void HistoryDataWorker::loadData(const QString &dbPath, int deviceId, qint64 startMs, qint64 endMs, const QString &deviceType, int maxPoints)
{
    const DataStore::HistoryDataResult result = DataStore::HistoryDataReader::read(
        dbPath, deviceId, startMs, endMs, deviceType, maxPoints);
    if (!result.error.isEmpty()) {
        emit loadError(result.error);
        return;
    }

    emit dataReady(result.keys1, result.values1,
                   result.keys2, result.values2,
                   result.keys3, result.values3,
                   result.pointCount, result.sampledCount);
}

HistoryWidget::HistoryWidget(QWidget *parent)
    : HistoryWidget(DataStore::DataStore::instance(), parent)
{
}

HistoryWidget::HistoryWidget(DataStore::DataStore& dataStore, QWidget *parent)
    : QWidget(parent)
    , m_dataStore(dataStore)
    , m_currentDeviceId("COM-001")
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_queryStartMs(0)
    , m_queryEndMs(0)
    , m_queryDeviceId(1)
    , m_queryDeviceType("COM")
    , m_isLoading(false)
    , m_needsFullLoad(false)
    , m_viewportTimer(nullptr)
    , m_pendingViewStart(0)
    , m_pendingViewEnd(0)
{
    initUI();

    m_workerThread = new QThread(this);
    m_worker = new HistoryDataWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &HistoryDataWorker::dataReady, this, &HistoryWidget::onDataReady);
    connect(m_worker, &HistoryDataWorker::loadError, this, &HistoryWidget::onLoadError);

    m_workerThread->start();

    m_viewportTimer = new QTimer(this);
    m_viewportTimer->setSingleShot(true);
    m_viewportTimer->setInterval(300);
    connect(m_viewportTimer, &QTimer::timeout, this, &HistoryWidget::onViewportTimerTimeout);

    m_graph1->setName("排气温度 (°C)");
    m_graph2->setName("排气压力 (bar)");
    m_graph3->setName("运行电流 (A)");

    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addDays(-1);
    m_startDateTime->setDateTime(start);
    m_endDateTime->setDateTime(end);
    m_queryStartMs = start.toMSecsSinceEpoch();
    m_queryEndMs = end.toMSecsSinceEpoch();
    m_queryDeviceId = 1;
    m_queryDeviceType = "COM";

    QTimer::singleShot(100, this, [this]() {
        loadViewportData(m_queryStartMs / 1000.0, m_queryEndMs / 1000.0);
    });
}

HistoryWidget::~HistoryWidget()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void HistoryWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_mainLayout->setSpacing(15);

    createControlPanel();
    createChartPanel();
}

void HistoryWidget::createControlPanel()
{
    m_controlPanel = new QWidget(this);
    m_controlPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    m_controlPanel->setFixedHeight(70);
    
    QHBoxLayout *controlLayout = new QHBoxLayout(m_controlPanel);
    controlLayout->setContentsMargins(15, 10, 15, 10);
    controlLayout->setSpacing(15);
    
    QLabel *deviceLabel = new QLabel("设备:", this);
    deviceLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    controlLayout->addWidget(deviceLabel);
    
    m_deviceSelector = new QComboBox(this);
    m_deviceSelector->setStyleSheet(R"(
        QComboBox {
            background: #132f4c;
            border: 1px solid #1e3a5f;
            border-radius: 4px;
            color: #e0e0e0;
            font-size: 12px;
            padding: 6px 10px;
            min-width: 120px;
        }
        QComboBox:hover {
            border: 1px solid #60a5fa;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background: #132f4c;
            color: #e0e0e0;
            selection-background-color: #1e3a5f;
        }
    )");
    
    m_deviceSelector->addItem("空压机1", "COM-001");
    m_deviceSelector->addItem("空压机2", "COM-002");
    m_deviceSelector->addItem("空压机3", "COM-003");
    m_deviceSelector->addItem("储气罐", "TANK-001");
    m_deviceSelector->addItem("干燥机", "DRY-001");
    
    connect(m_deviceSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HistoryWidget::onDeviceChanged);
    controlLayout->addWidget(m_deviceSelector);
    
    QLabel *rangeLabel = new QLabel("时间范围:", this);
    rangeLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    controlLayout->addWidget(rangeLabel);
    
    m_timeRangeSelector = new QComboBox(this);
    m_timeRangeSelector->addItem("最近1小时", 1);
    m_timeRangeSelector->addItem("最近6小时", 6);
    m_timeRangeSelector->addItem("最近12小时", 12);
    m_timeRangeSelector->addItem("最近24小时", 24);
    m_timeRangeSelector->addItem("最近3天", 72);
    m_timeRangeSelector->addItem("最近7天", 168);
    m_timeRangeSelector->addItem("自定义", 0);
    m_timeRangeSelector->setCurrentIndex(3);
    m_timeRangeSelector->setFixedWidth(130);
    m_timeRangeSelector->setStyleSheet(R"(
        QComboBox {
            background: #132f4c;
            border: 1px solid #1e3a5f;
            border-radius: 4px;
            color: #e0e0e0;
            font-size: 12px;
            padding: 6px 10px;
        }
        QComboBox:hover {
            border: 1px solid #60a5fa;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background: #132f4c;
            color: #e0e0e0;
            selection-background-color: #1e3a5f;
        }
    )");
    connect(m_timeRangeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HistoryWidget::onTimeRangeChanged);
    controlLayout->addWidget(m_timeRangeSelector);
    
    QLabel *startLabel = new QLabel("开始时间:", this);
    startLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    controlLayout->addWidget(startLabel);
    
    m_startDateTime = new QDateTimeEdit(this);
    m_startDateTime->setDateTime(QDateTime::currentDateTime().addDays(-1));
    m_startDateTime->setCalendarPopup(true);
    m_startDateTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_startDateTime->setStyleSheet(R"(
        QDateTimeEdit {
            background: #132f4c;
            border: 1px solid #1e3a5f;
            border-radius: 4px;
            color: #e0e0e0;
            font-size: 12px;
            padding: 6px 10px;
            min-width: 180px;
        }
        QDateTimeEdit:hover {
            border: 1px solid #60a5fa;
        }
        QDateTimeEdit::drop-down {
            border: none;
            width: 20px;
        }
    )");
    controlLayout->addWidget(m_startDateTime);
    
    QLabel *endLabel = new QLabel("结束时间:", this);
    endLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    controlLayout->addWidget(endLabel);
    
    m_endDateTime = new QDateTimeEdit(this);
    m_endDateTime->setDateTime(QDateTime::currentDateTime());
    m_endDateTime->setCalendarPopup(true);
    m_endDateTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_endDateTime->setStyleSheet(R"(
        QDateTimeEdit {
            background: #132f4c;
            border: 1px solid #1e3a5f;
            border-radius: 4px;
            color: #e0e0e0;
            font-size: 12px;
            padding: 6px 10px;
            min-width: 180px;
        }
        QDateTimeEdit:hover {
            border: 1px solid #60a5fa;
        }
        QDateTimeEdit::drop-down {
            border: none;
            width: 20px;
        }
    )");
    controlLayout->addWidget(m_endDateTime);
    
    controlLayout->addSpacing(10);
    
    m_queryBtn = new QPushButton("查询", this);
    m_queryBtn->setFixedWidth(80);
    m_queryBtn->setCursor(Qt::PointingHandCursor);
    m_queryBtn->setStyleSheet(R"(
        QPushButton {
            background: #2563eb;
            border: none;
            border-radius: 4px;
            color: white;
            font-size: 13px;
            font-weight: bold;
            padding: 8px 16px;
        }
        QPushButton:hover {
            background: #3b82f6;
        }
        QPushButton:pressed {
            background: #1d4ed8;
        }
    )");
    connect(m_queryBtn, &QPushButton::clicked, this, &HistoryWidget::onQueryClicked);
    controlLayout->addWidget(m_queryBtn);
    
    controlLayout->addStretch();
    
    m_mainLayout->addWidget(m_controlPanel);
}

void HistoryWidget::createChartPanel()
{
    m_chartPanel = new QWidget(this);
    m_chartPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    
    QVBoxLayout *chartLayout = new QVBoxLayout(m_chartPanel);
    chartLayout->setContentsMargins(10, 10, 10, 10);
    chartLayout->setSpacing(10);
    
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *chartTitle = new QLabel("历史趋势图", this);
    chartTitle->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: bold;");
    headerLayout->addWidget(chartTitle);
    headerLayout->addStretch();
    
    m_dataInfoLabel = new QLabel("请选择时间范围进行查询", this);
    m_dataInfoLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    headerLayout->addWidget(m_dataInfoLabel);
    
    chartLayout->addLayout(headerLayout);
    
    m_plot = new QCustomPlot(this);
    m_plot->setOpenGl(true);
    //m_plot->setNoAntialiasing(true);
    m_plot->setNotAntialiasedElements(QCP::aeAll);
    m_plot->setPlottingHint(QCP::phFastPolylines, true);
    m_plot->setPlottingHint(QCP::phImmediateRefresh, false);
    
    m_graph1 = m_plot->addGraph();
    m_graph1->setName("参数1");
    m_graph1->setPen(QPen(QColor(255, 100, 100), 2));
    
    m_graph2 = m_plot->addGraph();
    m_graph2->setName("参数2");
    m_graph2->setPen(QPen(QColor(100, 150, 255), 2));
    
    m_graph3 = m_plot->addGraph();
    m_graph3->setName("参数3");
    m_graph3->setPen(QPen(QColor(100, 255, 150), 2));
    
    setupChartStyle();
    
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    
    connect(m_plot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, &HistoryWidget::onViewportChanged);
    
    chartLayout->addWidget(m_plot, 1);
    
    m_mainLayout->addWidget(m_chartPanel, 1);
}

void HistoryWidget::setupChartStyle()
{
    m_plot->setBackground(QBrush(QColor(15, 37, 64)));
    
    QCPAxis *xAxis = m_plot->xAxis;
    xAxis->setBasePen(QPen(Qt::white, 2));
    xAxis->setTickPen(QPen(Qt::white, 1));
    xAxis->setSubTickPen(QPen(Qt::white, 1));
    xAxis->setTickLabelColor(Qt::white);
    xAxis->setLabelColor(Qt::white);
    xAxis->setTickLabelRotation(-45);
    
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("MM-dd HH:mm");
    xAxis->setTicker(dateTicker);
    xAxis->setLabel("时间");
    
    QCPAxis *yAxis = m_plot->yAxis;
    yAxis->setBasePen(QPen(Qt::white, 2));
    yAxis->setTickPen(QPen(Qt::white, 1));
    yAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis->setTickLabelColor(Qt::white);
    yAxis->setLabelColor(Qt::white);
    yAxis->setLabel("数值");
    
    m_plot->legend->setVisible(true);
    m_plot->legend->setTextColor(Qt::white);
    m_plot->legend->setBrush(QColor(0, 0, 0, 150));
    m_plot->legend->setBorderPen(QPen(Qt::NoPen));
}

void HistoryWidget::onQueryClicked()
{
    QDateTime start = m_startDateTime->dateTime();
    QDateTime end = m_endDateTime->dateTime();
    
    if (start >= end) {
        QMessageBox::warning(this, "提示", "开始时间必须早于结束时间");
        return;
    }
    
    m_queryStartMs = start.toMSecsSinceEpoch();
    m_queryEndMs = end.toMSecsSinceEpoch();
    m_queryDeviceId = getDeviceDbId(m_currentDeviceId);
    
    if (m_currentDeviceId.startsWith("COM")) {
        m_queryDeviceType = "COM";
        m_graph1->setName("排气温度 (°C)");
        m_graph2->setName("排气压力 (bar)");
        m_graph3->setName("运行电流 (A)");
        m_graph3->setVisible(true);
    } else if (m_currentDeviceId.startsWith("TANK")) {
        m_queryDeviceType = "TANK";
        m_graph1->setName("罐压 (bar)");
        m_graph2->setName("温度 (°C)");
        m_graph3->setVisible(false);
    } else if (m_currentDeviceId.startsWith("DRY")) {
        m_queryDeviceType = "DRY";
        m_graph1->setName("进口压力 (bar)");
        m_graph2->setName("出口压力 (bar)");
        m_graph3->setName("露点 (°C)");
    }
    
    loadViewportData(m_queryStartMs / 1000.0, m_queryEndMs / 1000.0);
}

void HistoryWidget::onDeviceChanged(int index)
{
    m_currentDeviceId = m_deviceSelector->itemData(index).toString();
}

void HistoryWidget::onTimeRangeChanged(int index)
{
    int hours = m_timeRangeSelector->itemData(index).toInt();
    if (hours == 0) {
        return;
    }
    
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addSecs(-hours * 3600);
    
    m_startDateTime->setDateTime(start);
    m_endDateTime->setDateTime(end);
    
    m_queryStartMs = start.toMSecsSinceEpoch();
    m_queryEndMs = end.toMSecsSinceEpoch();
    m_queryDeviceId = getDeviceDbId(m_currentDeviceId);
    
    if (m_currentDeviceId.startsWith("COM")) {
        m_queryDeviceType = "COM";
    } else if (m_currentDeviceId.startsWith("TANK")) {
        m_queryDeviceType = "TANK";
    } else if (m_currentDeviceId.startsWith("DRY")) {
        m_queryDeviceType = "DRY";
    }
    
    loadViewportData(m_queryStartMs / 1000.0, m_queryEndMs / 1000.0);
}

void HistoryWidget::onViewportChanged(const QCPRange &newRange)
{
    if (m_isLoading) return;
    if (m_queryStartMs == 0 && m_queryEndMs == 0) return;

    double viewStart = newRange.lower;
    double viewEnd = newRange.upper;

    if (viewStart < m_queryStartMs / 1000.0 - 1 || viewEnd > m_queryEndMs / 1000.0 + 1) return;

    m_pendingViewStart = viewStart;
    m_pendingViewEnd = viewEnd;
    m_viewportTimer->start();
}

void HistoryWidget::onViewportTimerTimeout()
{
    loadViewportDataFull(m_pendingViewStart, m_pendingViewEnd);
}

void HistoryWidget::loadViewportData(double startSec, double endSec)
{
    QMutexLocker locker(&m_loadMutex);
    if (m_isLoading) return;
    m_isLoading = true;
    m_needsFullLoad = true;

    qint64 startMs = static_cast<qint64>(startSec * 1000);
    qint64 endMs = static_cast<qint64>(endSec * 1000);

    QMetaObject::invokeMethod(m_worker, "loadData", Qt::QueuedConnection,
                              Q_ARG(QString, m_dataStore.databasePath()),
                              Q_ARG(int, m_queryDeviceId),
                              Q_ARG(qint64, startMs),
                              Q_ARG(qint64, endMs),
                              Q_ARG(QString, m_queryDeviceType),
                              Q_ARG(int, 200));
}

void HistoryWidget::loadViewportDataFull(double startSec, double endSec)
{
    QMutexLocker locker(&m_loadMutex);
    if (m_isLoading) return;
    m_isLoading = true;
    m_needsFullLoad = false;

    qint64 startMs = static_cast<qint64>(startSec * 1000);
    qint64 endMs = static_cast<qint64>(endSec * 1000);

    QMetaObject::invokeMethod(m_worker, "loadData", Qt::QueuedConnection,
                              Q_ARG(QString, m_dataStore.databasePath()),
                              Q_ARG(int, m_queryDeviceId),
                              Q_ARG(qint64, startMs),
                              Q_ARG(qint64, endMs),
                              Q_ARG(QString, m_queryDeviceType),
                              Q_ARG(int, 800));
}

void HistoryWidget::onDataReady(QVector<double> keys1, QVector<double> values1,
                                 QVector<double> keys2, QVector<double> values2,
                                 QVector<double> keys3, QVector<double> values3,
                                 int pointCount, int sampledCount)
{
    QMutexLocker locker(&m_loadMutex);

    if (pointCount == 0) {
        m_dataInfoLabel->setText("所选时间段内无历史数据");
        m_isLoading = false;
        return;
    }

    m_plot->setUpdatesEnabled(false);

    m_graph1->setData(keys1, values1, true);
    m_graph2->setData(keys2, values2, true);
    m_graph3->setData(keys3, values3, true);

    if (sampledCount < pointCount) {
        m_dataInfoLabel->setText(QString("共 %1 个数据点 (已采样至 %2 点)").arg(pointCount).arg(sampledCount));
    } else {
        m_dataInfoLabel->setText(QString("共 %1 个数据点").arg(pointCount));
    }

    m_plot->xAxis->setRange(keys1.first(), keys1.last());
    m_plot->rescaleAxes();
    m_plot->setUpdatesEnabled(true);
    m_plot->replot();
    m_isLoading = false;

    if (m_needsFullLoad && sampledCount < pointCount) {
        m_needsFullLoad = false;
        double startSec = keys1.first();
        double endSec = keys1.last();
        locker.unlock();
        QTimer::singleShot(50, this, [this, startSec, endSec]() {
            loadViewportDataFull(startSec, endSec);
        });
    }
}

void HistoryWidget::onLoadError(const QString &error)
{
    QMutexLocker locker(&m_loadMutex);
    qDebug() << "History data load error:" << error;
    m_dataInfoLabel->setText("数据加载失败");
    m_isLoading = false;
}

QString HistoryWidget::getDeviceName(const QString &deviceId)
{
    if (deviceId == "COM-001") return "空压机1";
    if (deviceId == "COM-002") return "空压机2";
    if (deviceId == "COM-003") return "空压机3";
    if (deviceId == "TANK-001") return "储气罐";
    if (deviceId == "DRY-001") return "干燥机";
    return deviceId;
}

int HistoryWidget::getDeviceDbId(const QString &deviceId)
{
    if (deviceId == "COM-001") return 1;
    if (deviceId == "COM-002") return 2;
    if (deviceId == "COM-003") return 3;
    if (deviceId == "TANK-001") return 4;
    if (deviceId == "DRY-001") return 5;
    return 1;
}

}
