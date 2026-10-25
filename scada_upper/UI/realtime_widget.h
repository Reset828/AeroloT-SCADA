#ifndef REALTIME_WIDGET_H
#define REALTIME_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QFrame>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QElapsedTimer>

#include "../core/device_data.h"
#include "../core/types.h"
#include "qcustomplot.h"

namespace UI {

class RealtimeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RealtimeWidget(QWidget *parent = nullptr);
    ~RealtimeWidget();

signals:
    void deviceChanged(const QString &deviceId);

public slots:
    void onDeviceDataUpdated(const QString &deviceId, const core::DeviceData &data);

private slots:
    void onDeviceSelected(int index);
    void onTimerTimeout();

private:
    void initUI();
    void createDeviceSelector();
    void createStatusPanel();
    void createParamsPanel();
    void createChartPanel();
    void updateDisplay(const QString &deviceId, const core::DeviceData &data);
    void updateChartData(const core::DeviceData &data);
    void setupChartForDevice(const QString &deviceId);
    void addDataPoint(double timeSec, const core::DeviceData &data);
    QString getDeviceName(const QString &deviceId);
    QString getStatusText(int status);
    QString getStatusColor(int status);

    QVBoxLayout *m_mainLayout;
    
    QWidget *m_selectorPanel;
    QComboBox *m_deviceSelector;
    
    QWidget *m_statusPanel;
    QLabel *m_deviceNameLabel;
    QLabel *m_statusLabel;
    QLabel *m_statusIcon;
    
    QWidget *m_paramsPanel;
    QGridLayout *m_paramsGrid;
    QVector<QLabel*> m_paramNameLabels;
    QVector<QLabel*> m_paramValueLabels;
    
    QWidget *m_chartPanel;
    QCustomPlot *m_plot;
    QVector<QCPGraph*> m_graphs;
    QVector<QCPAxis*> m_yAxes;
    QLabel *m_dataInfoLabel;
    
    QTimer *m_updateTimer;
    QElapsedTimer m_elapsedTimer;
    
    QMap<QString, core::DeviceData> m_currentData;
    QString m_currentDeviceId;
    QString m_currentDeviceType;
    
    static const int MAX_POINTS = 120;
    static const int TIME_WINDOW_SEC = 60;
    
    QVector<double> m_timeBuffer;
    QVector<QVector<double>> m_valueBuffers;
    double m_startTimeSec;
    
    struct ParamInfo {
        QString name;
        QString unit;
        double value;
        bool visible;
    };
    
    QVector<ParamInfo> m_params;
};

}

#endif
