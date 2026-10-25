#ifndef COMMLOG_WIDGET_H
#define COMMLOG_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QDateTime>

namespace DataStore { class DataStore; }

namespace UI {

class CommLogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommLogWidget(QWidget *parent = nullptr);
    CommLogWidget(DataStore::DataStore& dataStore, QWidget *parent = nullptr);
    ~CommLogWidget();

public slots:
    void appendLog(const QString &direction, const QString &deviceId, const QString &content);
    void appendModbusLog(int slaveId, const QString &operation, const QString &data);
    void appendWebSocketLog(const QString &clientId, const QString &message);
    void appendWebhookLog(const QString &url, int status, const QString &response);
    void loadHistoryLogs();

private slots:
    void onClearClicked();
    void onLevelChanged(int index);

private:
    void initUI();
    void createControlPanel();
    void createLogDisplay();
    void appendLogEntry(const QString &timestamp, const QString &direction, const QString &deviceId, const QString &content);
    QString getDirectionColor(const QString &direction);
    QString getDirectionIcon(const QString &direction);

    QVBoxLayout *m_mainLayout;
    
    QWidget *m_controlPanel;
    QComboBox *m_levelFilter;
    QPushButton *m_clearBtn;
    QLabel *m_logCountLabel;
    
    QWidget *m_logPanel;
    QTextEdit *m_logDisplay;
    
    int m_logCount;
    DataStore::DataStore& m_dataStore;
};

}

#endif
