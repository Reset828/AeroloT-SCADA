#ifndef HISTORY_WIDGET_H
#define HISTORY_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QComboBox>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>

#include "qcustomplot.h"

namespace DataStore { class DataStore; }

namespace UI {

class HistoryDataWorker : public QObject {
    Q_OBJECT
public:
    explicit HistoryDataWorker(QObject *parent = nullptr) : QObject(parent) {}
    ~HistoryDataWorker() override;

public slots:
    void loadData(const QString &dbPath, int deviceId, qint64 startMs, qint64 endMs, const QString &deviceType, int maxPoints = 800);

signals:
    void dataReady(QVector<double> keys1, QVector<double> values1,
                   QVector<double> keys2, QVector<double> values2,
                   QVector<double> keys3, QVector<double> values3,
                   int pointCount, int sampledCount);
    void loadError(const QString &error);
};

class HistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWidget(QWidget *parent = nullptr);
    HistoryWidget(DataStore::DataStore& dataStore, QWidget *parent = nullptr);
    ~HistoryWidget();

private slots:
    void onQueryClicked();
    void onDeviceChanged(int index);
    void onTimeRangeChanged(int index);
    void onViewportChanged(const QCPRange &newRange);
    void onViewportTimerTimeout();
    void onDataReady(QVector<double> keys1, QVector<double> values1,
                     QVector<double> keys2, QVector<double> values2,
                     QVector<double> keys3, QVector<double> values3,
                     int pointCount, int sampledCount);
    void onLoadError(const QString &error);

private:
    void initUI();
    void createControlPanel();
    void createChartPanel();
    void loadViewportData(double startSec, double endSec);
    void loadViewportDataFull(double startSec, double endSec);
    void setupChartStyle();
    QString getDeviceName(const QString &deviceId);
    int getDeviceDbId(const QString &deviceId);

    QVBoxLayout *m_mainLayout;
    DataStore::DataStore& m_dataStore;
    
    QWidget *m_controlPanel;
    QComboBox *m_timeRangeSelector;
    QComboBox *m_deviceSelector;
    QDateTimeEdit *m_startDateTime;
    QDateTimeEdit *m_endDateTime;
    QPushButton *m_queryBtn;
    
    QWidget *m_chartPanel;
    QCustomPlot *m_plot;
    QCPGraph *m_graph1;
    QCPGraph *m_graph2;
    QCPGraph *m_graph3;
    
    QLabel *m_dataInfoLabel;
    QString m_currentDeviceId;
    
    QThread *m_workerThread;
    HistoryDataWorker *m_worker;
    qint64 m_queryStartMs;
    qint64 m_queryEndMs;
    int m_queryDeviceId;
    QString m_queryDeviceType;
    bool m_isLoading;
    bool m_needsFullLoad;
    QMutex m_loadMutex;
    QTimer *m_viewportTimer;
    double m_pendingViewStart;
    double m_pendingViewEnd;
};

}

#endif
