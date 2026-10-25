#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QListWidget>
#include <QPoint>
#include <QWidget>
#include <QComboBox>
#include <QStackedWidget>

#include "titlebar.h"
#include "trend_chart.h"
#include "navigation_bar.h"
#include "realtime_widget.h"
#include "alarm_record_widget.h"
#include "history_widget.h"
#include "commlog_widget.h"
#include "../ModbusEngine/modbus_session.h"
#include "../AlarmEngine/alarm_evaluator.h"
#include "../AlarmEngine/alarm_manager.h"
#include "../WebSocketServer/websocket_server.h"
#include "../AlarmEngine/webhook_client.h"
#include "../Application/device_data_service.h"
#include "../core/device_data.h"
#include "../core/types.h"

namespace DataStore { class DataStore; }

namespace UI {

struct MainWindowDependencies {
    DataStore::DataStore& dataStore;
    AlarmEngine::AlarmManager& alarmManager;
    ModbusEngine::ModbusSession& modbusSession;
    WebSocketServer::WebSocketServerManager& webSocketServer;
    core::WebhookClientThread& webhookClient;
};

class MainWindow : public QMainWindow, private Application::DeviceDataPublisher
{
    Q_OBJECT

public:
    explicit MainWindow(const MainWindowDependencies& dependencies,
                        QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTimerTimeout();
    void onDeviceSelected(QListWidgetItem *item);
    void onConnectRequested();
    void onDisconnectRequested();
    void onModbusDataReceived(int slaveId, const QVector<quint16>& values, QModbusDataUnit::RegisterType registerType);
    void onModbusConnectionStateChanged(core::ConnectionState state);
    void onModbusError(const QString& errorString);
    void onNavigationChanged(int index);

private:
    void initUI();
    void createMenuBar();
    void updateDeviceInfo(const QString &deviceId);
    void setConnectionStatus(bool connected);
    void showNoConnectionState();
    core::DeviceType getDeviceType(const QString &deviceId);
    void processModbusData(int slaveId, const QVector<quint16> &values);
    void broadcastConnectionState();
    void broadcastDeviceData(const QString &deviceId,
                             const core::DeviceData &data) override;
    void broadcastAlarm(const QString &deviceId, const QString &alarmType, const QString &message);

    QWidget *m_centralWidget;
    QHBoxLayout *m_mainLayout;
    
    NavigationBar *m_navBar;
    QStackedWidget *m_contentStack;
    
    RealtimeWidget *m_realtimeWidget;
    AlarmRecordWidget *m_alarmRecordWidget;
    HistoryWidget *m_historyWidget;
    CommLogWidget *m_commLogWidget;
    
    QLabel *m_statusLabel;
    QLabel *m_timeLabel;
    QTimer *m_timer;

    ModbusEngine::ModbusSession& m_modbusSession;

    QString m_currentDeviceId;
    DataStore::DataStore &m_dataStore;
    TitleBar *m_titleBar = nullptr;
    bool m_windowDragging = false;
    QPoint m_dragPos;
    bool m_isConnected = false;
    
    AlarmEngine::AlarmManager& m_alarmManager;
    
    WebSocketServer::WebSocketServerManager& m_wsServer;
    core::WebhookClientThread& m_webhookClient;
    Application::DeviceDataService m_deviceDataService;
};

} // namespace UI

#endif // MAIN_WINDOW_H
