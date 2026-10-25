#include "alarm_record_widget.h"

#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

#include "../DataStore/datastore.h"
#include "../DataStore/alarm_log_reader.h"

namespace UI {

AlarmRecordWidget::AlarmRecordWidget(QWidget *parent)
    : AlarmRecordWidget(DataStore::DataStore::instance(), parent)
{
}

AlarmRecordWidget::AlarmRecordWidget(DataStore::DataStore& dataStore,
                                     QWidget *parent)
    : QWidget(parent)
    , m_dataStore(dataStore)
{
    initUI();
}

AlarmRecordWidget::~AlarmRecordWidget()
{
}

void AlarmRecordWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_mainLayout->setSpacing(15);

    createDateTimePicker();
    createAlarmTable();
}

void AlarmRecordWidget::createDateTimePicker()
{
    m_filterPanel = new QWidget(this);
    m_filterPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    m_filterPanel->setFixedHeight(70);
    
    QHBoxLayout *filterLayout = new QHBoxLayout(m_filterPanel);
    filterLayout->setContentsMargins(15, 10, 15, 10);
    filterLayout->setSpacing(15);
    
    QLabel *rangeLabel = new QLabel("时间范围:", this);
    rangeLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    filterLayout->addWidget(rangeLabel);
    
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
    connect(m_timeRangeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AlarmRecordWidget::onTimeRangeChanged);
    filterLayout->addWidget(m_timeRangeSelector);
    
    QLabel *startLabel = new QLabel("开始时间:", this);
    startLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    filterLayout->addWidget(startLabel);
    
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
    filterLayout->addWidget(m_startDateTime);
    
    QLabel *endLabel = new QLabel("结束时间:", this);
    endLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    filterLayout->addWidget(endLabel);
    
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
    filterLayout->addWidget(m_endDateTime);
    
    filterLayout->addSpacing(10);
    
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
    connect(m_queryBtn, &QPushButton::clicked, this, &AlarmRecordWidget::onQueryClicked);
    filterLayout->addWidget(m_queryBtn);
    
    m_exportBtn = new QPushButton("导出", this);
    m_exportBtn->setFixedWidth(80);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setStyleSheet(R"(
        QPushButton {
            background: #059669;
            border: none;
            border-radius: 4px;
            color: white;
            font-size: 13px;
            font-weight: bold;
            padding: 8px 16px;
        }
        QPushButton:hover {
            background: #10b981;
        }
        QPushButton:pressed {
            background: #047857;
        }
    )");
    connect(m_exportBtn, &QPushButton::clicked, this, &AlarmRecordWidget::onExportClicked);
    filterLayout->addWidget(m_exportBtn);
    
    filterLayout->addStretch();
    
    m_mainLayout->addWidget(m_filterPanel);
}

void AlarmRecordWidget::createAlarmTable()
{
    m_tablePanel = new QWidget(this);
    m_tablePanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    
    QVBoxLayout *tableLayout = new QVBoxLayout(m_tablePanel);
    tableLayout->setContentsMargins(10, 10, 10, 10);
    tableLayout->setSpacing(10);
    
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *tableTitle = new QLabel("报警记录", this);
    tableTitle->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: bold;");
    headerLayout->addWidget(tableTitle);
    headerLayout->addStretch();
    
    m_recordCountLabel = new QLabel("共 0 条记录", this);
    m_recordCountLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    headerLayout->addWidget(m_recordCountLabel);
    
    tableLayout->addLayout(headerLayout);
    
    m_alarmTable = new QTableWidget(this);
    m_alarmTable->setColumnCount(5);
    m_alarmTable->setHorizontalHeaderLabels({"序号", "设备名称", "报警时间", "报警类型", "报警描述"});
    
    m_alarmTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_alarmTable->horizontalHeader()->resizeSection(0, 60);
    m_alarmTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_alarmTable->horizontalHeader()->resizeSection(1, 100);
    m_alarmTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_alarmTable->horizontalHeader()->resizeSection(2, 180);
    m_alarmTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_alarmTable->horizontalHeader()->resizeSection(3, 120);
    m_alarmTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    
    m_alarmTable->horizontalHeader()->setStyleSheet(R"(
        QHeaderView::section {
            background: #132f4c;
            color: #60a5fa;
            padding: 10px 12px;
            border: none;
            border-bottom: 2px solid #1e3a5f;
            font-weight: bold;
            font-size: 12px;
        }
    )");
    
    m_alarmTable->setStyleSheet(R"(
        QTableWidget {
            background: #0f2540;
            border: 1px solid #1e3a5f;
            font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #e0e0e0;
            gridline-color: #1e3a5f;
        }
        QTableWidget::item {
            padding: 8px 12px;
            border-bottom: 1px solid #1e3a5f;
        }
        QTableWidget::item:selected {
            background: #1e3a5f;
            color: #60a5fa;
        }
        QScrollBar:vertical {
            background: #0f2540;
            width: 10px;
        }
        QScrollBar::handle:vertical {
            background: #1e3a5f;
            border-radius: 5px;
            min-height: 30px;
        }
    )");
    
    m_alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmTable->setAlternatingRowColors(false);
    m_alarmTable->verticalHeader()->setVisible(false);
    
    tableLayout->addWidget(m_alarmTable, 1);
    
    m_mainLayout->addWidget(m_tablePanel, 1);
}

void AlarmRecordWidget::onQueryClicked()
{
    QDateTime start = m_startDateTime->dateTime();
    QDateTime end = m_endDateTime->dateTime();
    
    if (start >= end) {
        QMessageBox::warning(this, "提示", "开始时间必须早于结束时间");
        return;
    }
    
    loadAlarmRecords(start, end);
}

void AlarmRecordWidget::onTimeRangeChanged(int index)
{
    int hours = m_timeRangeSelector->itemData(index).toInt();
    if (hours == 0) {
        return;
    }
    
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start = end.addSecs(-hours * 3600);
    
    m_startDateTime->setDateTime(start);
    m_endDateTime->setDateTime(end);
    
    loadAlarmRecords(start, end);
}

void AlarmRecordWidget::onExportClicked()
{
    if (m_alarmTable->rowCount() == 0) {
        QMessageBox::information(this, "提示", "没有可导出的数据");
        return;
    }
    
    QString filePath = QFileDialog::getSaveFileName(this, "导出报警记录", "", "CSV Files (*.csv)");
    if (filePath.isEmpty()) {
        return;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法打开文件进行写入");
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "\xEF\xBB\xBF";
    
    out << "序号,设备名称,报警时间,报警类型,报警描述\n";
    
    for (int row = 0; row < m_alarmTable->rowCount(); ++row) {
        for (int col = 0; col < m_alarmTable->columnCount(); ++col) {
            QTableWidgetItem *item = m_alarmTable->item(row, col);
            if (item) {
                out << "\"" << item->text() << "\"";
            } else {
                out << "\"\"";
            }
            if (col < m_alarmTable->columnCount() - 1) {
                out << ",";
            }
        }
        out << "\n";
    }
    
    file.close();
    QMessageBox::information(this, "成功", "报警记录已导出至:\n" + filePath);
}

void AlarmRecordWidget::loadAlarmRecords(const QDateTime &start, const QDateTime &end)
{
    m_alarmTable->setRowCount(0);

    const DataStore::AlarmLogReadResult result = DataStore::AlarmLogReader::read(
        m_dataStore.databasePath(),
        start.toMSecsSinceEpoch(), end.toMSecsSinceEpoch());
    if (result.failure == DataStore::AlarmLogReadFailure::Open) {
        qDebug() << "Failed to open database for alarm query:" << result.error;
        return;
    }
    if (result.failure == DataStore::AlarmLogReadFailure::Prepare) {
        qDebug() << "Failed to prepare alarm query:" << result.error;
        return;
    }
    if (result.failure == DataStore::AlarmLogReadFailure::Execute) {
        qDebug() << "Failed to execute alarm query:" << result.error;
        return;
    }
    
    int row = 0;
    for (const DataStore::AlarmLogEntry& record : result.records) {
        const int deviceId = record.deviceId;
        const QString& alarmTime = record.alarmTime;
        const int alarmType = record.alarmType;
        const QString& message = record.message;
        const int severity = record.alarmLevel;
        
        QString deviceIdStr = QString("device_%1").arg(deviceId);
        QString deviceName = getDeviceName(deviceIdStr);
        QString alarmTypeStr;
        
        switch (alarmType) {
            case 1: alarmTypeStr = "压力超限"; break;
            case 2: alarmTypeStr = "温度超限"; break;
            case 3: alarmTypeStr = "电流异常"; break;
            case 4: alarmTypeStr = "设备故障"; break;
            case 5: alarmTypeStr = "通信中断"; break;
            default: alarmTypeStr = "未知类型"; break;
        }
        
        addAlarmToTable(deviceName, alarmTime, alarmTypeStr, message, getSeverityText(severity));
        row++;
    }
    
    m_recordCountLabel->setText(QString("共 %1 条记录").arg(row));
}

void AlarmRecordWidget::addAlarmToTable(const QString &deviceName, const QString &time, const QString &type, const QString &desc, const QString &severity)
{
    int row = m_alarmTable->rowCount();
    m_alarmTable->insertRow(row);
    
    QTableWidgetItem *seqItem = new QTableWidgetItem(QString::number(row + 1));
    seqItem->setTextAlignment(Qt::AlignCenter);
    m_alarmTable->setItem(row, 0, seqItem);
    
    QTableWidgetItem *deviceItem = new QTableWidgetItem(deviceName);
    deviceItem->setTextAlignment(Qt::AlignCenter);
    m_alarmTable->setItem(row, 1, deviceItem);
    
    QTableWidgetItem *timeItem = new QTableWidgetItem(time);
    timeItem->setTextAlignment(Qt::AlignCenter);
    m_alarmTable->setItem(row, 2, timeItem);
    
    QTableWidgetItem *typeItem = new QTableWidgetItem(type);
    typeItem->setTextAlignment(Qt::AlignCenter);
    m_alarmTable->setItem(row, 3, typeItem);
    
    QTableWidgetItem *descItem = new QTableWidgetItem(desc);
    m_alarmTable->setItem(row, 4, descItem);
    
    if (severity == "严重") {
        descItem->setForeground(QColor("#ef4444"));
    } else if (severity == "警告") {
        descItem->setForeground(QColor("#fbbf24"));
    }
}

void AlarmRecordWidget::onAlarmReceived(const QString &deviceId, const QString &alarmType, const QString &message, int severity)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString deviceName = getDeviceName(deviceId);
    QString severityStr = getSeverityText(severity);
    
    addAlarmToTable(deviceName, time, alarmType, message, severityStr);
    
    int currentCount = m_alarmTable->rowCount();
    m_recordCountLabel->setText(QString("共 %1 条记录").arg(currentCount));
}

QString AlarmRecordWidget::getDeviceName(const QString &deviceId)
{
    if (deviceId == "COM-001" || deviceId == "device_1") return "空压机1";
    if (deviceId == "COM-002" || deviceId == "device_2") return "空压机2";
    if (deviceId == "COM-003" || deviceId == "device_3") return "空压机3";
    if (deviceId == "TANK-001" || deviceId == "device_4") return "储气罐";
    if (deviceId == "DRY-001" || deviceId == "device_5") return "干燥机";
    return deviceId;
}

QString AlarmRecordWidget::getSeverityText(int severity)
{
    switch (severity) {
        case 0: return "信息";
        case 1: return "警告";
        case 2: return "严重";
        default: return "未知";
    }
}

QString AlarmRecordWidget::getSeverityColor(int severity)
{
    switch (severity) {
        case 0: return "#60a5fa";
        case 1: return "#fbbf24";
        case 2: return "#ef4444";
        default: return "#94a3b8";
    }
}

}
