#ifndef DEVICE_WIDGET_H
#define DEVICE_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>

namespace UI {

enum class DeviceType {
    Compressor,
    Tank,
    Dryer
};

enum class RunStatus {
    Unknown,
    Running,
    Stopped,
    Fault
};

class DeviceWidget : public QFrame
{
    Q_OBJECT

public:
    explicit DeviceWidget(const QString &deviceName, 
                          DeviceType deviceType, 
                          QWidget *parent = nullptr);
    ~DeviceWidget();

    void updateData(double pressure, double temperature, RunStatus status);
    void setDeviceId(const QString &id);
    QString deviceId() const;
    DeviceType deviceType() const;

 signals:
    void deviceClicked(const QString &deviceId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void initUI();
    void updateStatusDisplay(RunStatus status);

    QString m_deviceId;
    QString m_deviceName;
    DeviceType m_deviceType;

    QVBoxLayout *m_mainLayout;
    QLabel *m_nameLabel;
    QLabel *m_statusIcon;
    QLabel *m_statusText;
    QLabel *m_pressureLabel;
    QLabel *m_temperatureLabel;
    QLabel *m_pressureValue;
    QLabel *m_temperatureValue;

    bool m_isHovered;
};

} // namespace UI

#endif // DEVICE_WIDGET_H