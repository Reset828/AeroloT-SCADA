#include "main_window.h"
#include "../DataStore/datastore.h"
#include "titlebar.h"
#include <QMouseEvent>
#include <QCloseEvent>
#include <QMessageBox>
#include <QApplication>
#include <QDateTime>
#include <QPalette>
#include <QLinearGradient>
#include <algorithm>

namespace UI {

MainWindow::MainWindow(const MainWindowDependencies& dependencies, QWidget *parent)
    : QMainWindow(parent)
    , m_timer(new QTimer(this))
    , m_modbusSession(dependencies.modbusSession)
    , m_currentDeviceId("COM-001")
    , m_dataStore(dependencies.dataStore)
    , m_isConnected(false)
    , m_alarmManager(dependencies.alarmManager)
    , m_wsServer(dependencies.webSocketServer)
    , m_webhookClient(dependencies.webhookClient)
    , m_deviceDataService(m_dataStore, m_alarmManager, *this)
{
    qDebug() << "MainWindow: Constructor starting";
    
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setWindowTitle("AeroloT-SCADA");
    resize(1400, 900);
    setMinimumSize(1024, 700);

    QPalette palette;
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, QColor("#0a1929"));
    gradient.setColorAt(1, QColor("#132f4c"));
    palette.setBrush(QPalette::Window, gradient);
    setPalette(palette);

    initUI();
    m_alarmManager.initialize();

    menuBar()->setFixedHeight(28);
    
    m_titleBar = new TitleBar(this);
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRequested, this, [this]() {
        if (isMaximized()) showNormal(); else showMaximized();
    });
    connect(m_titleBar, &TitleBar::closeRequested, this, &MainWindow::close);
    connect(m_titleBar, &TitleBar::connectRequested, this, &MainWindow::onConnectRequested);
    connect(m_titleBar, &TitleBar::disconnectRequested, this, &MainWindow::onDisconnectRequested);
    m_titleBar->setGeometry(0, 28, width(), 38);

    connect(&m_modbusSession, &ModbusEngine::ModbusSession::dataReceived,
            this, &MainWindow::onModbusDataReceived);
    connect(&m_modbusSession, &ModbusEngine::ModbusSession::connectionStateChanged,
            this, &MainWindow::onModbusConnectionStateChanged);
    connect(&m_modbusSession, &ModbusEngine::ModbusSession::modbusErrorOccurred,
            this, &MainWindow::onModbusError);

    m_wsServer.start();

    connect(&m_wsServer, &WebSocketServer::WebSocketServerManager::clientConnected,
            this, [this](const QString& clientInfo) {
        qDebug() << "Mobile client connected:" << clientInfo;
        QString logMsg = QString("移动端连接: %1").arg(clientInfo);
        m_commLogWidget->appendWebSocketLog(clientInfo, logMsg);
        m_dataStore.insertCommLog(QDateTime::currentMSecsSinceEpoch(), "WebSocket", "WS", clientInfo, logMsg);
        broadcastConnectionState();
    });

    m_webhookClient.setUrl("http://localhost:8080/webhook/alarm");
    m_webhookClient.start();

    connect(&m_alarmManager, &AlarmEngine::AlarmManager::alarmTriggered, this, [this](const AlarmEngine::AlarmRecord& record) {
        QString deviceIdStr = QString("device_%1").arg(record.deviceId);
        QString alarmTypeStr;
        switch (record.alarmType) {
            case 1: alarmTypeStr = "压力超限"; break;
            case 2: alarmTypeStr = "温度超限"; break;
            case 3: alarmTypeStr = "电流异常"; break;
            case 4: alarmTypeStr = "压差超限"; break;
            case 5: alarmTypeStr = "露点超限"; break;
            default: alarmTypeStr = "未知报警"; break;
        }
        if (m_wsServer.hasClients()) {
            broadcastAlarm(deviceIdStr, record.alarmLevel == 2 ? "critical" : "warning", record.message);
        }
        m_alarmRecordWidget->onAlarmReceived(deviceIdStr, alarmTypeStr, record.message, record.alarmLevel);
    });

    connect(&m_alarmManager, &AlarmEngine::AlarmManager::alarmRecovered, this, [this](const AlarmEngine::AlarmRecord& record) {
        QString deviceIdStr = QString("device_%1").arg(record.deviceId);
        if (m_wsServer.hasClients()) {
            broadcastAlarm(deviceIdStr, "recovery", record.message);
        }
    });

    this->installEventFilter(this);

    showNoConnectionState();
    
    DataStore::AuditRecord startupRecord;
    startupRecord.timestamp = QDateTime::currentMSecsSinceEpoch();
    startupRecord.userName = "system";
    startupRecord.operationType = 0;
    startupRecord.targetParameter = "application_startup";
    startupRecord.oldValue = "";
    startupRecord.newValue = "started";
    startupRecord.result = 1;
    m_dataStore.appendAudit(startupRecord);
    
    qDebug() << "MainWindow: Constructor complete";
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow: Destructor starting";
    if (m_timer) {
        m_timer->stop();
    }
    m_modbusSession.shutdown();
    m_wsServer.stop();
    m_webhookClient.stop();
    qDebug() << "MainWindow: Destructor complete";
}

core::DeviceType MainWindow::getDeviceType(const QString &deviceId)
{
    if (deviceId.startsWith("COM")) return core::DeviceType::Compressor;
    if (deviceId.startsWith("TANK")) return core::DeviceType::Tank;
    if (deviceId.startsWith("DRY")) return core::DeviceType::Dryer;
    return core::DeviceType::Compressor;
}

void MainWindow::initUI()
{
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("CentralWidget");
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QHBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 67, 0, 0);
    m_mainLayout->setSpacing(0);

    createMenuBar();

    m_navBar = new NavigationBar(this);
    connect(m_navBar, &NavigationBar::navigationChanged, this, &MainWindow::onNavigationChanged);
    m_mainLayout->addWidget(m_navBar);

    m_contentStack = new QStackedWidget(this);
    m_contentStack->setStyleSheet("background: #0a1929;");

    m_realtimeWidget = new RealtimeWidget(this);
    connect(m_realtimeWidget, &RealtimeWidget::deviceChanged, this, [this](const QString &deviceId) {
        m_currentDeviceId = deviceId;
    });
    m_alarmRecordWidget = new AlarmRecordWidget(m_dataStore, this);
    m_historyWidget = new HistoryWidget(m_dataStore, this);
    m_commLogWidget = new CommLogWidget(m_dataStore, this);

    m_contentStack->addWidget(m_realtimeWidget);
    m_contentStack->addWidget(m_alarmRecordWidget);
    m_contentStack->addWidget(m_historyWidget);
    m_contentStack->addWidget(m_commLogWidget);

    m_mainLayout->addWidget(m_contentStack, 1);

    statusBar()->setStyleSheet("QStatusBar { background: #0a1929; color: #e0e0e0; border-top: 1px solid #1e3a5f; padding: 4px; }");
    m_statusLabel = new QLabel("\u672A\u8FDE\u63A5\u4E0B\u4F4D\u673A");
    m_statusLabel->setStyleSheet("color: #f97316; font-weight: bold; font-size: 12px;");
    statusBar()->addWidget(m_statusLabel);
    m_timeLabel = new QLabel();
    m_timeLabel->setStyleSheet("color: #60a5fa; font-size: 12px;");
    statusBar()->addPermanentWidget(m_timeLabel);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTimerTimeout);
    m_timer->start(500);
    onTimerTimeout();
}

void MainWindow::showNoConnectionState()
{
    m_statusLabel->setText("\u672A\u8FDE\u63A5\u4E0B\u4F4D\u673A");
    m_statusLabel->setStyleSheet("color: #f97316; font-weight: bold; font-size: 12px;");
}

void MainWindow::setConnectionStatus(bool connected)
{
    m_isConnected = connected;
    if (connected) {
        m_statusLabel->setText("\u5DF2\u8FDE\u63A5\u4E0B\u4F4D\u673A");
        m_statusLabel->setStyleSheet("color: #4ade80; font-weight: bold; font-size: 12px;");
    } else {
        m_statusLabel->setText("\u672A\u8FDE\u63A5\u4E0B\u4F4D\u673A");
        m_statusLabel->setStyleSheet("color: #f97316; font-weight: bold; font-size: 12px;");
        showNoConnectionState();
    }
    
    broadcastConnectionState();
}

void MainWindow::createMenuBar()
{
    menuBar()->setStyleSheet("QMenuBar { background: #0f2540; border-bottom: 1px solid #1e3a5f; padding: 2px; font-size: 12px; color: #e0e0e0; }"
                             "QMenuBar::item { padding: 6px 12px; color: #e0e0e0; }"
                             "QMenuBar::item:selected { background: #1e3a5f; }"
                             "QMenu { background: #0f2540; border: 1px solid #1e3a5f; font-size: 12px; color: #e0e0e0; }"
                             "QMenu::item { padding: 6px 20px; color: #e0e0e0; }"
                             "QMenu::item:selected { background: #1e3a5f; }");

    QMenu *fileMenu = menuBar()->addMenu("\u6587\u4EF6");
    QAction *exitAction = new QAction("\u9000\u51FA", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);

    QMenu *connMenu = menuBar()->addMenu("\u8FDE\u63A5");
    QAction *connectAction = new QAction("\u8FDE\u63A5\u4E0B\u4F4D\u673A", this);
    connect(connectAction, &QAction::triggered, this, &MainWindow::onConnectRequested);
    connMenu->addAction(connectAction);

    QMenu *helpMenu = menuBar()->addMenu("\u5E2E\u52A9");
    QAction *aboutAction = new QAction("\u5173\u4E8E", this);
    connect(aboutAction, &QAction::triggered, []() {
        QMessageBox::about(nullptr, "\u5173\u4E8E", "AeroloT-SCADA v1.0\n\n\u7A7A\u538B\u7AD9\u7269\u8054\u63A7\u7CFB\u7EDF");
    });
    helpMenu->addAction(aboutAction);
}

void MainWindow::onConnectRequested()
{
    if (m_isConnected) {
        return;
    }

    DataStore::AuditRecord auditRecord;
    auditRecord.timestamp = QDateTime::currentMSecsSinceEpoch();
    auditRecord.userName = "system";
    auditRecord.operationType = 1;
    auditRecord.targetParameter = "connect";
    auditRecord.oldValue = "disconnected";
    auditRecord.newValue = "connecting";
    auditRecord.result = 1;
    m_dataStore.appendAudit(auditRecord);

    m_modbusSession.connectToHost("127.0.0.1", 1502);
}

void MainWindow::onDisconnectRequested()
{
    if (!m_isConnected) {
        return;
    }

    DataStore::AuditRecord auditRecord;
    auditRecord.timestamp = QDateTime::currentMSecsSinceEpoch();
    auditRecord.userName = "system";
    auditRecord.operationType = 1;
    auditRecord.targetParameter = "disconnect";
    auditRecord.result = 1;
    auditRecord.oldValue = "connected";
    auditRecord.newValue = "disconnected";
    m_dataStore.appendAudit(auditRecord);

    m_modbusSession.disconnectFromHost();
}

void MainWindow::onModbusConnectionStateChanged(core::ConnectionState state)
{
    DataStore::AuditRecord auditRecord;
    auditRecord.timestamp = QDateTime::currentMSecsSinceEpoch();
    auditRecord.userName = "system";
    auditRecord.operationType = 2;
    auditRecord.targetParameter = "connection_state";
    
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    
    if (state == core::ConnectionState::Connected) {
        setConnectionStatus(true);
        m_titleBar->setConnectionState(true);
        qDebug() << "Setting online devices 1,2,3,4,5";
        m_modbusSession.startScanning();
        
        QString logMsg = "已连接到下位机";
        m_commLogWidget->appendModbusLog(0, "连接状态", logMsg);
        m_dataStore.insertCommLog(timestamp, "Modbus", "TX→RX", "下位机", logMsg);
        
        auditRecord.result = 1;
        auditRecord.oldValue = "disconnected";
        auditRecord.newValue = "connected";
        m_dataStore.appendAudit(auditRecord);
    } else if (state == core::ConnectionState::Disconnected) {
        m_modbusSession.stopScanning();
        setConnectionStatus(false);
        m_titleBar->setConnectionState(false);
        
        QString logMsg = "与下位机断开连接";
        m_commLogWidget->appendModbusLog(0, "连接状态", logMsg);
        m_dataStore.insertCommLog(timestamp, "Modbus", "TX→RX", "下位机", logMsg);
        
        auditRecord.result = 1;
        auditRecord.oldValue = "connected";
        auditRecord.newValue = "disconnected";
        m_dataStore.appendAudit(auditRecord);
    } else if (state == core::ConnectionState::Reconnecting) {
        m_modbusSession.stopScanning();
        m_isConnected = false;
        m_titleBar->setConnectionState(false);
        
        m_statusLabel->setText("\u65AD\u5F00\u8FDE\u63A5\uFF0C\u6B63\u5728\u5C1D\u8BD5\u91CD\u8FDE...");
        m_statusLabel->setStyleSheet("color: #f97316; font-weight: bold; font-size: 12px;");
        
        QString logMsg = "正在尝试重连下位机...";
        m_commLogWidget->appendModbusLog(0, "连接状态", logMsg);
        m_dataStore.insertCommLog(timestamp, "Modbus", "TX→RX", "下位机", logMsg);
        
        auditRecord.result = 1;
        auditRecord.oldValue = "connected";
        auditRecord.newValue = "reconnecting";
        m_dataStore.appendAudit(auditRecord);
    } else if (state == core::ConnectionState::Connecting) {
        m_statusLabel->setText("\u6B63\u5728\u8FDE\u63A5...");
        m_statusLabel->setStyleSheet("color: #60a5fa; font-weight: bold; font-size: 12px;");
    }
}

void MainWindow::onModbusError(const QString& errorString)
{
    qDebug() << "Modbus error:" << errorString;
    m_commLogWidget->appendModbusLog(0, "通信错误", errorString);
}

void MainWindow::onModbusDataReceived(int slaveId, const QVector<quint16>& values, QModbusDataUnit::RegisterType registerType)
{
    Q_UNUSED(registerType);
    if (values.size() > 0) {
        processModbusData(slaveId, values);
    }
}

void MainWindow::processModbusData(int slaveId, const QVector<quint16> &values)
{
    const auto mapped = m_deviceDataService.processModbusRegisters(
        slaveId, values, QDateTime::currentMSecsSinceEpoch());
    if (!mapped)
        return;

    const auto& data = mapped->data;
    const auto& updatedDeviceId = mapped->deviceKey;

    if (updatedDeviceId == m_currentDeviceId) {
        QMetaObject::invokeMethod(this, [this, data, updatedDeviceId]() {
            m_realtimeWidget->onDeviceDataUpdated(updatedDeviceId, data);
        }, Qt::QueuedConnection);
    }
}

void MainWindow::updateDeviceInfo(const QString &deviceId)
{
    if (!m_isConnected) {
        return;
    }
    
    auto data = m_dataStore.currentData();
    
    if (data.contains(deviceId)) {
        const auto &d = data[deviceId];
        m_realtimeWidget->onDeviceDataUpdated(deviceId, d);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "\u786E\u8BA4\u5173\u95ED", "\u786E\u5B9A\u8981\u5173\u95ED\u7CFB\u5417\uFF1F", QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (m_timer) m_timer->stop();
        m_modbusSession.disconnectFromHost();
        m_dataStore.shutdown();
        event->accept();
        QApplication::quit();
    } else {
        event->ignore();
    }
}

void MainWindow::onTimerTimeout()
{
    m_timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    
    if (m_isConnected && !m_currentDeviceId.isEmpty()) {
        auto data = m_dataStore.currentData();
        if (data.contains(m_currentDeviceId)) {
            const auto &d = data[m_currentDeviceId];
            m_realtimeWidget->onDeviceDataUpdated(m_currentDeviceId, d);
            
            static int saveCounter = 0;
            saveCounter++;
            if (saveCounter >= 60) {
                saveCounter = 0;
                m_dataStore.appendCurrentHistorySnapshot();
            }
        }
    }
}

void MainWindow::onDeviceSelected(QListWidgetItem *item)
{
    QString deviceId = item->data(Qt::UserRole).toString();
    m_currentDeviceId = deviceId;
    updateDeviceInfo(deviceId);
}

void MainWindow::onNavigationChanged(int index)
{
    m_contentStack->setCurrentIndex(index);
    
    if (index == 3 && m_commLogWidget) {
        m_commLogWidget->loadHistoryLogs();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_windowDragging = true;
            m_dragPos = me->globalPos() - frameGeometry().topLeft();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && m_windowDragging) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        move(me->globalPos() - m_dragPos);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease && m_windowDragging) {
        m_windowDragging = false;
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::broadcastConnectionState()
{
    QJsonObject msg;
    msg["type"] = "connection_state";
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    msg["connected"] = m_isConnected;
    
    if (m_isConnected) {
        msg["message"] = "已连接到下位机";
    } else {
        msg["message"] = "未连接到下位机";
    }
    
    m_wsServer.broadcastData(msg);
}

void MainWindow::broadcastDeviceData(const QString &deviceId, const core::DeviceData &data)
{
    if (!m_isConnected) {
        return;
    }
    
    if (!m_wsServer.hasClients()) {
        return;
    }
    
    QJsonObject msg;
    msg["type"] = "realtime_data";
    msg["timestamp"] = data.timestamp;
    
    QJsonObject deviceObj;
    deviceObj["id"] = deviceId;
    deviceObj["status"] = data.status;
    deviceObj["pressure"] = data.pressure;
    deviceObj["temperature"] = data.temperature;
    
    core::DeviceType type = getDeviceType(deviceId);
    if (type == core::DeviceType::Compressor) {
        deviceObj["type"] = "compressor";
        deviceObj["current"] = data.phaseCurrent;
        deviceObj["alarmCode"] = data.alarmCode;
    } else if (type == core::DeviceType::Tank) {
        deviceObj["type"] = "tank";
    } else if (type == core::DeviceType::Dryer) {
        deviceObj["type"] = "dryer";
        deviceObj["outletPressure"] = data.outletPressure;
        deviceObj["dewPoint"] = data.dewPoint;
        deviceObj["regenStatus"] = data.regenStatus;
    }
    
    msg["device"] = deviceObj;
    
    m_wsServer.broadcastData(msg);
}

void MainWindow::broadcastAlarm(const QString &deviceId, const QString &alarmType, const QString &message)
{
    QJsonObject alarmMsg;
    alarmMsg["type"] = "alarm";
    alarmMsg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    alarmMsg["deviceId"] = deviceId;
    alarmMsg["alarmType"] = alarmType;
    alarmMsg["message"] = message;
    
    m_wsServer.broadcastData(alarmMsg);
    
    QJsonObject webhookPayload;
    webhookPayload["deviceId"] = deviceId;
    webhookPayload["alarmType"] = alarmType;
    webhookPayload["message"] = message;
    webhookPayload["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    m_webhookClient.sendAlarm(webhookPayload);
}

} // namespace UI
