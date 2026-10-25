#ifndef ALARM_RECORD_WIDGET_H
#define ALARM_RECORD_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QDate>
#include <QTime>
#include <QComboBox>

namespace DataStore { class DataStore; }

namespace UI {

class AlarmRecordWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmRecordWidget(QWidget *parent = nullptr);
    AlarmRecordWidget(DataStore::DataStore& dataStore, QWidget *parent = nullptr);
    ~AlarmRecordWidget();

public slots:
    void onAlarmReceived(const QString &deviceId, const QString &alarmType, const QString &message, int severity);

private slots:
    void onQueryClicked();
    void onExportClicked();
    void onTimeRangeChanged(int index);

private:
    void initUI();
    void createDateTimePicker();
    void createAlarmTable();
    void loadAlarmRecords(const QDateTime &start, const QDateTime &end);
    void addAlarmToTable(const QString &deviceName, const QString &time, const QString &type, const QString &desc, const QString &severity);
    QString getDeviceName(const QString &deviceId);
    QString getSeverityText(int severity);
    QString getSeverityColor(int severity);

    QVBoxLayout *m_mainLayout;
    DataStore::DataStore& m_dataStore;
    
    QWidget *m_filterPanel;
    QComboBox *m_timeRangeSelector;
    QDateTimeEdit *m_startDateTime;
    QDateTimeEdit *m_endDateTime;
    QPushButton *m_queryBtn;
    QPushButton *m_exportBtn;
    
    QWidget *m_tablePanel;
    QTableWidget *m_alarmTable;
    
    QLabel *m_recordCountLabel;
};

}

#endif
