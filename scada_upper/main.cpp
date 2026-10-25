#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QTimer>
#include <memory>

#include "UI/main_window.h"
#include "DataStore/datastore.h"
#include "DataStore/database_manager.h"
#include "ModbusEngine/modbus_session.h"
#include "WebSocketServer/websocket_server.h"
#include "AlarmEngine/alarm_manager.h"
#include "AlarmEngine/webhook_client.h"
#include "core/timestamp.h"

int main(int argc, char *argv[])
{
    qDebug() << "=== Application Starting ===";
    
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("AeroloT");
    QCoreApplication::setOrganizationDomain("aerolot.com");
    QCoreApplication::setApplicationName("AeroloT-SCADA");
    QCoreApplication::setApplicationVersion("1.0.0");

    qDebug() << "Loading stylesheet...";
    QString appDir = QCoreApplication::applicationDirPath();
    QString stylePath = appDir + "/styles/global.qss";
    QFile styleFile(stylePath);
    
    if (styleFile.exists() && styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = styleFile.readAll();
        app.setStyleSheet(styleSheet);
        styleFile.close();
        qDebug() << "Stylesheet loaded successfully";
    } else {
        qDebug() << "Stylesheet not found at:" << stylePath;
    }

    qDebug() << "Initializing database...";
    QString dbPath = appDir + "/data/scada.db";
    QFileInfo dirInfo(dbPath);
    if (!dirInfo.exists()) {
        QDir().mkpath(dirInfo.path());
    }

    bool dbInitOk = false;
    DataStore::DataStore* store = nullptr;
    try {
        qDebug() << "Creating DataStore instance...";
        store = &DataStore::DataStore::instance();
        qDebug() << "Calling store.initialize()...";
        dbInitOk = store->initialize(dbPath);
        qDebug() << "store.initialize() returned:" << dbInitOk;
    } catch (const std::exception& e) {
        qCritical() << "Exception initializing DataStore:" << e.what();
    } catch (...) {
        qCritical() << "Unknown exception initializing DataStore";
    }
    qDebug() << "DataStore initialization result:" << dbInitOk;
    
    if (!dbInitOk) {
        qWarning() << "Failed to initialize database at:" << dbPath << "- continuing anyway";
    }

    AlarmEngine::AlarmManager& alarmManager = AlarmEngine::AlarmManager::instance();
    alarmManager.setAlarmRecordWriter(
        DataStore::DatabaseManager::instance());

    qDebug() << "Creating MainWindow...";
    
    UI::MainWindow *mainWindow = nullptr;
    std::unique_ptr<ModbusEngine::ModbusSession> modbusSession;
    std::unique_ptr<WebSocketServer::WebSocketServerManager> webSocketServer;
    std::unique_ptr<core::WebhookClientThread> webhookClient;
    try {
        qDebug() << "About to create MainWindow...";
        if (!store) {
            store = &DataStore::DataStore::instance();
        }
        modbusSession = std::make_unique<ModbusEngine::ModbusSession>();
        webSocketServer = std::make_unique<WebSocketServer::WebSocketServerManager>(18080);
        webhookClient = std::make_unique<core::WebhookClientThread>();

        const UI::MainWindowDependencies dependencies{
            *store,
            alarmManager,
            *modbusSession,
            *webSocketServer,
            *webhookClient
        };
        mainWindow = new UI::MainWindow(dependencies);
        qDebug() << "MainWindow created successfully";
    } catch (const std::exception &e) {
        qCritical() << "Exception creating MainWindow:" << e.what();
        QMessageBox::critical(nullptr, "错误", QString("创建主窗口失败: %1").arg(e.what()));
        return 1;
    } catch (...) {
        qCritical() << "Unknown exception creating MainWindow";
        QMessageBox::critical(nullptr, "错误", "创建主窗口时发生未知错误");
        return 1;
    }

    qDebug() << "Showing MainWindow...";
    mainWindow->show();
    qDebug() << "MainWindow shown";

    QTimer cleanupTimer;
    QObject::connect(&cleanupTimer, &QTimer::timeout, [&]() {
        if (store && dbInitOk) {
            store->cleanupOldData(30, 90);
        }
    });
    cleanupTimer.start(24 * 60 * 60 * 1000);
    qDebug() << "Cleanup timer started - will run every 24 hours";

    qDebug() << "Entering event loop...";

    int result = app.exec();
    
    qDebug() << "Application exiting with code:" << result;
    cleanupTimer.stop();
    delete mainWindow;
    return result;
}
