#include "realtime_widget.h"

#include <QDebug>
#include <QDateTime>

namespace UI {

RealtimeWidget::RealtimeWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentDeviceId("COM-001")
    , m_currentDeviceType("COM")
    , m_startTimeSec(0)
    , m_updateTimer(new QTimer(this))
{
    initUI();

    m_elapsedTimer.start();
    m_startTimeSec = 0;

    connect(m_updateTimer, &QTimer::timeout, this, &RealtimeWidget::onTimerTimeout);
    m_updateTimer->start(500);

    setupChartForDevice(m_currentDeviceId);
}

RealtimeWidget::~RealtimeWidget()
{
}

void RealtimeWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_mainLayout->setSpacing(10);

    createDeviceSelector();
    createStatusPanel();
    createParamsPanel();
    createChartPanel();
}

void RealtimeWidget::createDeviceSelector()
{
    m_selectorPanel = new QWidget(this);
    m_selectorPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    m_selectorPanel->setFixedHeight(50);

    QHBoxLayout *selectorLayout = new QHBoxLayout(m_selectorPanel);
    selectorLayout->setContentsMargins(15, 8, 15, 8);

    QLabel *selectorLabel = new QLabel("选择设备:", this);
    selectorLabel->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: bold;");
    selectorLayout->addWidget(selectorLabel);

    m_deviceSelector = new QComboBox(this);
    m_deviceSelector->setStyleSheet(R"(
        QComboBox {
            background: #132f4c;
            border: 1px solid #1e3a5f;
            border-radius: 4px;
            color: #e0e0e0;
            font-size: 13px;
            padding: 6px 12px;
            min-width: 150px;
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

    connect(m_deviceSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RealtimeWidget::onDeviceSelected);

    selectorLayout->addWidget(m_deviceSelector);
    selectorLayout->addStretch();

    m_mainLayout->addWidget(m_selectorPanel);
}

void RealtimeWidget::createStatusPanel()
{
    m_statusPanel = new QWidget(this);
    m_statusPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    m_statusPanel->setMinimumHeight(80);
    m_statusPanel->setMaximumHeight(100);

    QVBoxLayout *statusLayout = new QVBoxLayout(m_statusPanel);
    statusLayout->setContentsMargins(15, 8, 15, 8);
    statusLayout->setSpacing(6);

    QHBoxLayout *headerLayout = new QHBoxLayout();

    m_deviceNameLabel = new QLabel("空压机1", this);
    m_deviceNameLabel->setStyleSheet("color: #60a5fa; font-size: 16px; font-weight: bold;");
    headerLayout->addWidget(m_deviceNameLabel);

    headerLayout->addStretch();

    m_statusIcon = new QLabel("●", this);
    m_statusIcon->setFont(QFont("Segoe UI", 18));
    m_statusIcon->setStyleSheet("color: #94a3b8;");
    headerLayout->addWidget(m_statusIcon);

    m_statusLabel = new QLabel("未知", this);
    m_statusLabel->setStyleSheet("color: #94a3b8; font-size: 14px; font-weight: bold;");
    headerLayout->addWidget(m_statusLabel);

    statusLayout->addLayout(headerLayout);

    m_mainLayout->addWidget(m_statusPanel);
}

void RealtimeWidget::createParamsPanel()
{
    m_paramsPanel = new QWidget(this);
    m_paramsPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    m_paramsPanel->setMinimumHeight(160);
    m_paramsPanel->setMaximumHeight(200);

    QVBoxLayout *paramsLayout = new QVBoxLayout(m_paramsPanel);
    paramsLayout->setContentsMargins(15, 8, 15, 8);
    paramsLayout->setSpacing(8);

    QLabel *paramsTitle = new QLabel("运行参数", this);
    paramsTitle->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: bold;");
    paramsLayout->addWidget(paramsTitle);

    m_paramsGrid = new QGridLayout();
    m_paramsGrid->setSpacing(15);
    m_paramsGrid->setColumnStretch(1, 1);
    m_paramsGrid->setColumnStretch(3, 1);

    QStringList paramNames = {"排气压力", "排气温度", "运行电流", "运行状态", "运行时间", "变频频率", "报警代码", "预留参数"};
    QStringList paramUnits = {"bar", "°C", "A", "", "h", "Hz", "", ""};

    for (int i = 0; i < 8; ++i) {
        QLabel *nameLabel = new QLabel(paramNames[i], this);
        nameLabel->setStyleSheet("color: #94a3b8; font-size: 13px;");
        m_paramNameLabels.append(nameLabel);

        QLabel *valueLabel = new QLabel("--", this);
        valueLabel->setStyleSheet("color: #e0e0e0; font-size: 14px; font-weight: bold;");
        m_paramValueLabels.append(valueLabel);

        int row = i / 2;
        int col = (i % 2) * 2;

        QHBoxLayout *paramLayout = new QHBoxLayout();
        paramLayout->addWidget(nameLabel);
        paramLayout->addStretch();
        paramLayout->addWidget(valueLabel);

        if (!paramUnits[i].isEmpty()) {
            QLabel *unitLabel = new QLabel(paramUnits[i], this);
            unitLabel->setStyleSheet("color: #64748b; font-size: 12px;");
            paramLayout->addWidget(unitLabel);
        }

        m_paramsGrid->addLayout(paramLayout, row, col);
    }

    paramsLayout->addLayout(m_paramsGrid);
    m_mainLayout->addWidget(m_paramsPanel);
}

void RealtimeWidget::createChartPanel()
{
    m_chartPanel = new QWidget(this);
    m_chartPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    m_chartPanel->setMinimumHeight(280);

    QVBoxLayout *chartLayout = new QVBoxLayout(m_chartPanel);
    chartLayout->setContentsMargins(10, 8, 10, 10);
    chartLayout->setSpacing(8);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *chartTitle = new QLabel("实时趋势图", this);
    chartTitle->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: bold;");
    headerLayout->addWidget(chartTitle);
    headerLayout->addStretch();

    m_dataInfoLabel = new QLabel("等待数据...", this);
    m_dataInfoLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    headerLayout->addWidget(m_dataInfoLabel);

    chartLayout->addLayout(headerLayout);

    m_plot = new QCustomPlot(this);
    m_plot->setOpenGl(true);
    m_plot->setNotAntialiasedElements(QCP::aeAll);
    m_plot->setPlottingHint(QCP::phFastPolylines, true);
    m_plot->setPlottingHint(QCP::phImmediateRefresh, false);
    m_plot->setBackground(QBrush(QColor(15, 37, 64)));
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    QCPAxis *xAxis = m_plot->xAxis;
    xAxis->setBasePen(QPen(Qt::white, 2));
    xAxis->setTickPen(QPen(Qt::white, 1));
    xAxis->setSubTickPen(QPen(Qt::white, 1));
    xAxis->setTickLabelColor(Qt::white);
    xAxis->setLabelColor(Qt::white);
    xAxis->setLabel("时间 (秒)");

    QCPAxis *yAxis = m_plot->yAxis;
    yAxis->setBasePen(QPen(Qt::white, 2));
    yAxis->setTickPen(QPen(Qt::white, 1));
    yAxis->setSubTickPen(QPen(Qt::white, 1));
    yAxis->setTickLabelColor(Qt::white);
    yAxis->setLabelColor(Qt::white);

    m_plot->legend->setVisible(true);
    m_plot->legend->setTextColor(Qt::white);
    m_plot->legend->setBrush(QColor(0, 0, 0, 150));
    m_plot->legend->setBorderPen(QPen(Qt::NoPen));

    chartLayout->addWidget(m_plot, 1);

    m_mainLayout->addWidget(m_chartPanel, 1);
}

void RealtimeWidget::setupChartForDevice(const QString &deviceId)
{
    m_plot->setUpdatesEnabled(false);

    m_plot->clearGraphs();
    for (QCPAxis *axis : m_yAxes) {
        m_plot->axisRect()->removeAxis(axis);
    }
    m_graphs.clear();
    m_yAxes.clear();

    m_timeBuffer.clear();
    m_valueBuffers.clear();
    m_startTimeSec = 0;
    m_elapsedTimer.restart();

    QColor graphColors[] = {
        QColor(255, 100, 100),
        QColor(100, 150, 255),
        QColor(100, 255, 150)
    };

    if (deviceId.startsWith("COM")) {
        m_currentDeviceType = "COM";

        m_valueBuffers.resize(3);
        for (int i = 0; i < 3; ++i) {
            m_valueBuffers[i].clear();
        }

        QCPGraph *graph1 = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
        graph1->setName("排气温度 (°C)");
        graph1->setPen(QPen(graphColors[0], 2));
        m_graphs.append(graph1);

        QCPAxis *yAxis2 = m_plot->axisRect()->addAxis(QCPAxis::atRight);
        yAxis2->setBasePen(QPen(graphColors[1], 2));
        yAxis2->setTickPen(QPen(graphColors[1], 1));
        yAxis2->setSubTickPen(QPen(graphColors[1], 1));
        yAxis2->setTickLabelColor(graphColors[1]);
        yAxis2->setLabelColor(graphColors[1]);
        yAxis2->setLabel("排气压力 (bar)");
        m_yAxes.append(yAxis2);

        QCPGraph *graph2 = m_plot->addGraph(m_plot->xAxis, yAxis2);
        graph2->setName("排气压力 (bar)");
        graph2->setPen(QPen(graphColors[1], 2));
        m_graphs.append(graph2);

        QCPAxis *yAxis3 = m_plot->axisRect()->addAxis(QCPAxis::atRight);
        yAxis3->setBasePen(QPen(graphColors[2], 2));
        yAxis3->setTickPen(QPen(graphColors[2], 1));
        yAxis3->setSubTickPen(QPen(graphColors[2], 1));
        yAxis3->setTickLabelColor(graphColors[2]);
        yAxis3->setLabelColor(graphColors[2]);
        yAxis3->setLabel("运行电流 (A)");
        m_yAxes.append(yAxis3);

        QCPGraph *graph3 = m_plot->addGraph(m_plot->xAxis, yAxis3);
        graph3->setName("运行电流 (A)");
        graph3->setPen(QPen(graphColors[2], 2));
        m_graphs.append(graph3);

        m_plot->yAxis->setLabel("排气温度 (°C)");
        m_plot->yAxis->setBasePen(QPen(graphColors[0], 2));
        m_plot->yAxis->setTickPen(QPen(graphColors[0], 1));
        m_plot->yAxis->setSubTickPen(QPen(graphColors[0], 1));
        m_plot->yAxis->setTickLabelColor(graphColors[0]);
        m_plot->yAxis->setLabelColor(graphColors[0]);

    } else if (deviceId.startsWith("TANK")) {
        m_currentDeviceType = "TANK";

        m_valueBuffers.resize(2);
        for (int i = 0; i < 2; ++i) {
            m_valueBuffers[i].clear();
        }

        QCPGraph *graph1 = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
        graph1->setName("罐内压力 (bar)");
        graph1->setPen(QPen(graphColors[0], 2));
        m_graphs.append(graph1);

        QCPAxis *yAxis2 = m_plot->axisRect()->addAxis(QCPAxis::atRight);
        yAxis2->setBasePen(QPen(graphColors[1], 2));
        yAxis2->setTickPen(QPen(graphColors[1], 1));
        yAxis2->setSubTickPen(QPen(graphColors[1], 1));
        yAxis2->setTickLabelColor(graphColors[1]);
        yAxis2->setLabelColor(graphColors[1]);
        yAxis2->setLabel("罐内温度 (°C)");
        m_yAxes.append(yAxis2);

        QCPGraph *graph2 = m_plot->addGraph(m_plot->xAxis, yAxis2);
        graph2->setName("罐内温度 (°C)");
        graph2->setPen(QPen(graphColors[1], 2));
        m_graphs.append(graph2);

        m_plot->yAxis->setLabel("罐内压力 (bar)");
        m_plot->yAxis->setBasePen(QPen(graphColors[0], 2));
        m_plot->yAxis->setTickPen(QPen(graphColors[0], 1));
        m_plot->yAxis->setSubTickPen(QPen(graphColors[0], 1));
        m_plot->yAxis->setTickLabelColor(graphColors[0]);
        m_plot->yAxis->setLabelColor(graphColors[0]);

    } else if (deviceId.startsWith("DRY")) {
        m_currentDeviceType = "DRY";

        m_valueBuffers.resize(3);
        for (int i = 0; i < 3; ++i) {
            m_valueBuffers[i].clear();
        }

        QCPGraph *graph1 = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
        graph1->setName("进口压力 (bar)");
        graph1->setPen(QPen(graphColors[0], 2));
        m_graphs.append(graph1);

        QCPAxis *yAxis2 = m_plot->axisRect()->addAxis(QCPAxis::atRight);
        yAxis2->setBasePen(QPen(graphColors[1], 2));
        yAxis2->setTickPen(QPen(graphColors[1], 1));
        yAxis2->setSubTickPen(QPen(graphColors[1], 1));
        yAxis2->setTickLabelColor(graphColors[1]);
        yAxis2->setLabelColor(graphColors[1]);
        yAxis2->setLabel("出口压力 (bar)");
        m_yAxes.append(yAxis2);

        QCPGraph *graph2 = m_plot->addGraph(m_plot->xAxis, yAxis2);
        graph2->setName("出口压力 (bar)");
        graph2->setPen(QPen(graphColors[1], 2));
        m_graphs.append(graph2);

        QCPAxis *yAxis3 = m_plot->axisRect()->addAxis(QCPAxis::atRight);
        yAxis3->setBasePen(QPen(graphColors[2], 2));
        yAxis3->setTickPen(QPen(graphColors[2], 1));
        yAxis3->setSubTickPen(QPen(graphColors[2], 1));
        yAxis3->setTickLabelColor(graphColors[2]);
        yAxis3->setLabelColor(graphColors[2]);
        yAxis3->setLabel("露点 (°C)");
        m_yAxes.append(yAxis3);

        QCPGraph *graph3 = m_plot->addGraph(m_plot->xAxis, yAxis3);
        graph3->setName("露点 (°C)");
        graph3->setPen(QPen(graphColors[2], 2));
        m_graphs.append(graph3);

        m_plot->yAxis->setLabel("进口压力 (bar)");
        m_plot->yAxis->setBasePen(QPen(graphColors[0], 2));
        m_plot->yAxis->setTickPen(QPen(graphColors[0], 1));
        m_plot->yAxis->setSubTickPen(QPen(graphColors[0], 1));
        m_plot->yAxis->setTickLabelColor(graphColors[0]);
        m_plot->yAxis->setLabelColor(graphColors[0]);
    }

    m_plot->xAxis->setRange(0, TIME_WINDOW_SEC);

    m_plot->setUpdatesEnabled(true);
    m_plot->replot();
}

void RealtimeWidget::addDataPoint(double timeSec, const core::DeviceData &data)
{
    m_timeBuffer.append(timeSec);

    if (m_currentDeviceType == "COM") {
        m_valueBuffers[0].append(data.temperature);
        m_valueBuffers[1].append(data.pressure);
        m_valueBuffers[2].append(data.phaseCurrent);
    } else if (m_currentDeviceType == "TANK") {
        m_valueBuffers[0].append(data.pressure);
        m_valueBuffers[1].append(data.temperature);
    } else if (m_currentDeviceType == "DRY") {
        m_valueBuffers[0].append(data.pressure);
        m_valueBuffers[1].append(data.outletPressure);
        m_valueBuffers[2].append(data.dewPoint);
    }

    while (m_timeBuffer.size() > MAX_POINTS) {
        m_timeBuffer.removeFirst();
        for (auto &buf : m_valueBuffers) {
            buf.removeFirst();
        }
    }

    for (int i = 0; i < m_graphs.size(); ++i) {
        m_graphs[i]->setData(m_timeBuffer, m_valueBuffers[i], true);
    }

    double currentSec = m_timeBuffer.last();
    if (currentSec > TIME_WINDOW_SEC) {
        m_plot->xAxis->setRange(currentSec - TIME_WINDOW_SEC, currentSec);
    } else {
        m_plot->xAxis->setRange(0, TIME_WINDOW_SEC);
    }

    for (int i = 0; i < m_graphs.size(); ++i) {
        m_graphs[i]->rescaleValueAxis(false, true);
    }
}

void RealtimeWidget::updateChartData(const core::DeviceData &data)
{
    double elapsedSec = m_elapsedTimer.elapsed() / 1000.0;

    if (m_startTimeSec == 0) {
        m_startTimeSec = elapsedSec;
    }

    double timeSec = elapsedSec - m_startTimeSec;

    m_plot->setUpdatesEnabled(false);
    addDataPoint(timeSec, data);
    m_plot->setUpdatesEnabled(true);
    m_plot->replot();

    m_dataInfoLabel->setText(QString("已采集 %1 秒 | %2 个数据点").arg(static_cast<int>(timeSec)).arg(m_timeBuffer.size()));
}

void RealtimeWidget::onDeviceSelected(int index)
{
    QString deviceId = m_deviceSelector->itemData(index).toString();
    m_currentDeviceId = deviceId;

    emit deviceChanged(deviceId);

    m_deviceNameLabel->setText(getDeviceName(deviceId));

    setupChartForDevice(deviceId);

    if (m_currentData.contains(deviceId)) {
        updateDisplay(deviceId, m_currentData[deviceId]);
    } else {
        for (QLabel *label : m_paramValueLabels) {
            label->setText("--");
        }
        m_statusLabel->setText("未知");
        m_statusLabel->setStyleSheet("color: #94a3b8; font-size: 14px; font-weight: bold;");
        m_statusIcon->setStyleSheet("color: #94a3b8;");
        m_dataInfoLabel->setText("等待数据...");
    }
}

void RealtimeWidget::onTimerTimeout()
{
}

void RealtimeWidget::onDeviceDataUpdated(const QString &deviceId, const core::DeviceData &data)
{
    m_currentData[deviceId] = data;

    if (deviceId == m_currentDeviceId) {
        updateDisplay(deviceId, data);
        updateChartData(data);
    }
}

void RealtimeWidget::updateDisplay(const QString &deviceId, const core::DeviceData &data)
{
    m_deviceNameLabel->setText(getDeviceName(deviceId));

    QString statusText = getStatusText(data.status);
    QString statusColor = getStatusColor(data.status);

    m_statusLabel->setText(statusText);
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(statusColor));
    m_statusIcon->setStyleSheet(QString("color: %1;").arg(statusColor));

    if (deviceId.startsWith("COM")) {
        m_paramNameLabels[0]->setText("排气压力");
        m_paramValueLabels[0]->setText(QString::number(data.pressure, 'f', 2));

        m_paramNameLabels[1]->setText("排气温度");
        m_paramValueLabels[1]->setText(QString::number(data.temperature, 'f', 1));

        m_paramNameLabels[2]->setText("运行电流");
        m_paramValueLabels[2]->setText(QString::number(data.phaseCurrent, 'f', 1));

        m_paramNameLabels[3]->setText("运行状态");
        m_paramValueLabels[3]->setText(statusText);
        m_paramValueLabels[3]->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(statusColor));

        m_paramNameLabels[4]->setText("运行时间");
        m_paramValueLabels[4]->setText(QString::number(data.runtimeHours, 'f', 1));

        m_paramNameLabels[5]->setText("变频频率");
        m_paramValueLabels[5]->setText(QString::number(data.vfdFrequency, 'f', 1));

        m_paramNameLabels[6]->setText("报警代码");
        m_paramValueLabels[6]->setText(data.alarmCode == 0 ? "无" : QString::number(data.alarmCode));
        if (data.alarmCode == 0) {
            m_paramValueLabels[6]->setStyleSheet("color: #4ade80; font-size: 14px; font-weight: bold;");
        } else {
            m_paramValueLabels[6]->setStyleSheet("color: #ef4444; font-size: 14px; font-weight: bold;");
        }

        m_paramNameLabels[7]->setText("预留参数");
        m_paramValueLabels[7]->setText("--");

    } else if (deviceId.startsWith("TANK")) {
        m_paramNameLabels[0]->setText("罐内压力");
        m_paramValueLabels[0]->setText(QString::number(data.pressure, 'f', 2));

        m_paramNameLabels[1]->setText("罐内温度");
        m_paramValueLabels[1]->setText(QString::number(data.temperature, 'f', 1));

        m_paramNameLabels[2]->setText("运行状态");
        m_paramValueLabels[2]->setText(statusText);
        m_paramValueLabels[2]->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(statusColor));

        m_paramNameLabels[3]->setText("预留参数1");
        m_paramValueLabels[3]->setText("--");

        m_paramNameLabels[4]->setText("预留参数2");
        m_paramValueLabels[4]->setText("--");

        m_paramNameLabels[5]->setText("预留参数3");
        m_paramValueLabels[5]->setText("--");

        m_paramNameLabels[6]->setText("报警代码");
        m_paramValueLabels[6]->setText(data.alarmCode == 0 ? "无" : QString::number(data.alarmCode));
        if (data.alarmCode == 0) {
            m_paramValueLabels[6]->setStyleSheet("color: #4ade80; font-size: 14px; font-weight: bold;");
        } else {
            m_paramValueLabels[6]->setStyleSheet("color: #ef4444; font-size: 14px; font-weight: bold;");
        }

        m_paramNameLabels[7]->setText("预留参数4");
        m_paramValueLabels[7]->setText("--");

    } else if (deviceId.startsWith("DRY")) {
        m_paramNameLabels[0]->setText("进口压力");
        m_paramValueLabels[0]->setText(QString::number(data.pressure, 'f', 2));

        m_paramNameLabels[1]->setText("出口压力");
        m_paramValueLabels[1]->setText(QString::number(data.outletPressure, 'f', 2));

        m_paramNameLabels[2]->setText("露点温度");
        m_paramValueLabels[2]->setText(QString::number(data.dewPoint, 'f', 1));

        m_paramNameLabels[3]->setText("再生状态");
        m_paramValueLabels[3]->setText(data.regenStatus == 0 ? "待机" : "再生中");
        if (data.regenStatus == 0) {
            m_paramValueLabels[3]->setStyleSheet("color: #94a3b8; font-size: 14px; font-weight: bold;");
        } else {
            m_paramValueLabels[3]->setStyleSheet("color: #fbbf24; font-size: 14px; font-weight: bold;");
        }

        m_paramNameLabels[4]->setText("运行状态");
        m_paramValueLabels[4]->setText(statusText);
        m_paramValueLabels[4]->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold;").arg(statusColor));

        m_paramNameLabels[5]->setText("预留参数1");
        m_paramValueLabels[5]->setText("--");

        m_paramNameLabels[6]->setText("报警代码");
        m_paramValueLabels[6]->setText(data.alarmCode == 0 ? "无" : QString::number(data.alarmCode));
        if (data.alarmCode == 0) {
            m_paramValueLabels[6]->setStyleSheet("color: #4ade80; font-size: 14px; font-weight: bold;");
        } else {
            m_paramValueLabels[6]->setStyleSheet("color: #ef4444; font-size: 14px; font-weight: bold;");
        }

        m_paramNameLabels[7]->setText("预留参数2");
        m_paramValueLabels[7]->setText("--");
    }

    for (int i = 0; i < 8; ++i) {
        if (i != 3 && i != 6) {
            m_paramValueLabels[i]->setStyleSheet("color: #e0e0e0; font-size: 14px; font-weight: bold;");
        }
    }
}

QString RealtimeWidget::getDeviceName(const QString &deviceId)
{
    if (deviceId == "COM-001") return "空压机1";
    if (deviceId == "COM-002") return "空压机2";
    if (deviceId == "COM-003") return "空压机3";
    if (deviceId == "TANK-001") return "储气罐";
    if (deviceId == "DRY-001") return "干燥机";
    return deviceId;
}

QString RealtimeWidget::getStatusText(int status)
{
    switch (status) {
        case 0: return "停止";
        case 1: return "运行";
        case 2: return "故障";
        default: return "未知";
    }
}

QString RealtimeWidget::getStatusColor(int status)
{
    switch (status) {
        case 0: return "#94a3b8";
        case 1: return "#4ade80";
        case 2: return "#ef4444";
        default: return "#94a3b8";
    }
}

}
