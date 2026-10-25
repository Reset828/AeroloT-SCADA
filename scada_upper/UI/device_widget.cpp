#include "device_widget.h"

#include <QDebug>
#include <QPalette>
#include <QColor>
#include <QGraphicsDropShadowEffect>

namespace UI {

static const QString STATUS_RUNNING_ICON = "\u25CF";
static const QString STATUS_STOPPED_ICON = "\u25CB";
static const QString STATUS_FAULT_ICON = "\u2717";
static const QString STATUS_UNKNOWN_ICON = "\u2014";

DeviceWidget::DeviceWidget(const QString &deviceName, 
                           DeviceType deviceType, 
                           QWidget *parent)
    : QFrame(parent)
    , m_deviceName(deviceName)
    , m_deviceType(deviceType)
    , m_isHovered(false)
{
    switch (deviceType) {
    case DeviceType::Compressor:
        m_deviceId = QString("COM-%1").arg(deviceName);
        break;
    case DeviceType::Tank:
        m_deviceId = QString("TANK-%1").arg(deviceName);
        break;
    case DeviceType::Dryer:
        m_deviceId = QString("DRY-%1").arg(deviceName);
        break;
    default:
        m_deviceId = deviceName;
        break;
    }

    initUI();
    setObjectName("DeviceWidget");
    setStyleSheet(R"(
        #DeviceWidget {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e3a5f, stop:1 #0f2540);
            border-radius: 8px;
            border: 1px solid #2d5a8a;
        }
    )");
    
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(10);
    shadowEffect->setColor(QColor(0, 0, 0, 80));
    shadowEffect->setOffset(0, 2);
    setGraphicsEffect(shadowEffect);

    setFrameStyle(QFrame::NoFrame);
    setLineWidth(0);
    setMinimumSize(180, 140);
    setContentsMargins(12, 10, 12, 10);
}

DeviceWidget::~DeviceWidget()
{
}

void DeviceWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 8, 10, 8);
    m_mainLayout->setSpacing(6);

    m_nameLabel = new QLabel(m_deviceName, this);
    QFont nameFont = m_nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(13);
    nameFont.setFamily("Segoe UI");
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setStyleSheet(R"(
        color: #60a5fa;
        background: #0f2540;
        padding: 6px 10px;
        border-radius: 4px;
    )");
    m_mainLayout->addWidget(m_nameLabel);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(6);

    m_statusIcon = new QLabel(STATUS_UNKNOWN_ICON, this);
    m_statusIcon->setFont(QFont("Segoe UI", 16));
    m_statusIcon->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(m_statusIcon);

    m_statusText = new QLabel("\u672A\u77E5", this);
    m_statusText->setFont(QFont("Segoe UI", 11));
    m_statusText->setStyleSheet("color: #94a3b8;");
    statusLayout->addWidget(m_statusText);
    statusLayout->addStretch();
    m_mainLayout->addLayout(statusLayout);

    QHBoxLayout *dataLayout = new QHBoxLayout();
    dataLayout->setSpacing(12);

    QVBoxLayout *pressureLayout = new QVBoxLayout();
    pressureLayout->setSpacing(2);

    m_pressureLabel = new QLabel("\u538B\u529B", this);
    m_pressureLabel->setFont(QFont("Segoe UI", 9));
    m_pressureLabel->setStyleSheet("color: #00D9FF;");
    m_pressureLabel->setAlignment(Qt::AlignCenter);
    pressureLayout->addWidget(m_pressureLabel);

    m_pressureValue = new QLabel("-- bar", this);
    m_pressureValue->setFont(QFont("Segoe UI", 14, QFont::Bold));
    m_pressureValue->setStyleSheet("color: #e0e0e0;");
    m_pressureValue->setAlignment(Qt::AlignCenter);
    pressureLayout->addWidget(m_pressureValue);
    dataLayout->addLayout(pressureLayout);

    QFrame *separator = new QFrame(this);
    separator->setFixedWidth(1);
    separator->setStyleSheet("background: #2d5a8a;");
    dataLayout->addWidget(separator);

    QVBoxLayout *temperatureLayout = new QVBoxLayout();
    temperatureLayout->setSpacing(2);

    m_temperatureLabel = new QLabel("\u6E29\u5EA6", this);
    m_temperatureLabel->setFont(QFont("Segoe UI", 9));
    m_temperatureLabel->setStyleSheet("color: #FF6B6B;");
    m_temperatureLabel->setAlignment(Qt::AlignCenter);
    temperatureLayout->addWidget(m_temperatureLabel);

    m_temperatureValue = new QLabel("-- \u2103", this);
    m_temperatureValue->setFont(QFont("Segoe UI", 14, QFont::Bold));
    m_temperatureValue->setStyleSheet("color: #e0e0e0;");
    m_temperatureValue->setAlignment(Qt::AlignCenter);
    temperatureLayout->addWidget(m_temperatureValue);
    dataLayout->addLayout(temperatureLayout);

    m_mainLayout->addLayout(dataLayout);
    m_mainLayout->addStretch();

    updateStatusDisplay(RunStatus::Unknown);
}

void DeviceWidget::updateData(double pressure, double temperature, RunStatus status)
{
    m_pressureValue->setText(QString::number(pressure, 'f', 1) + " bar");
    m_temperatureValue->setText(QString::number(temperature, 'f', 1) + " \u2103");
    updateStatusDisplay(status);
}

void DeviceWidget::setDeviceId(const QString &id)
{
    m_deviceId = id;
}

QString DeviceWidget::deviceId() const
{
    return m_deviceId;
}

DeviceType DeviceWidget::deviceType() const
{
    return m_deviceType;
}

void DeviceWidget::mousePressEvent(QMouseEvent *event)
{
    emit deviceClicked(m_deviceId);
    QFrame::mousePressEvent(event);
}

void DeviceWidget::enterEvent(QEnterEvent *event)
{
    m_isHovered = true;
    setCursor(Qt::PointingHandCursor);
    
    QGraphicsDropShadowEffect *shadowEffect = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (shadowEffect) {
        shadowEffect->setBlurRadius(15);
        shadowEffect->setColor(QColor(30, 96, 175, 120));
    }

    QFrame::enterEvent(event);
}

void DeviceWidget::leaveEvent(QEvent *event)
{
    m_isHovered = false;
    setCursor(Qt::ArrowCursor);
    
    QGraphicsDropShadowEffect *shadowEffect = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (shadowEffect) {
        shadowEffect->setBlurRadius(10);
        shadowEffect->setColor(QColor(0, 0, 0, 80));
    }

    QFrame::leaveEvent(event);
}

void DeviceWidget::updateStatusDisplay(RunStatus status)
{
    QColor iconColor;
    QString statusStr;
    QString iconHtml;

    switch (status) {
    case RunStatus::Running:
        iconColor = QColor("#22c55e");
        statusStr = "\u8FD0\u884C";
        iconHtml = QString("<span style=\"color:%1; font-size:18px;\">%2</span>")
                      .arg(iconColor.name()).arg(STATUS_RUNNING_ICON);
        break;
    case RunStatus::Stopped:
        iconColor = QColor("#94a3b8");
        statusStr = "\u505C\u6B62";
        iconHtml = QString("<span style=\"color:%1; font-size:18px;\">%2</span>")
                      .arg(iconColor.name()).arg(STATUS_STOPPED_ICON);
        break;
    case RunStatus::Fault:
        iconColor = QColor("#ef4444");
        statusStr = "\u6545\u969C";
        iconHtml = QString("<span style=\"color:%1; font-size:18px;\">%2</span>")
                      .arg(iconColor.name()).arg(STATUS_FAULT_ICON);
        break;
    case RunStatus::Unknown:
    default:
        iconColor = QColor("#94a3b8");
        statusStr = "\u672A\u77E5";
        iconHtml = QString("<span style=\"color:%1; font-size:18px;\">%2</span>")
                      .arg(iconColor.name()).arg(STATUS_UNKNOWN_ICON);
        break;
    }

    m_statusIcon->setText(iconHtml);
    m_statusText->setText(statusStr);

    QString styleSheet;
    switch (status) {
    case RunStatus::Running:
        styleSheet = R"(
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e3a5f, stop:1 #0f2540);
            border-radius: 8px;
            border: 1px solid #22c55e;
        )";
        break;
    case RunStatus::Fault:
        styleSheet = R"(
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e3a5f, stop:1 #0f2540);
            border-radius: 8px;
            border: 1px solid #ef4444;
        )";
        break;
    case RunStatus::Stopped:
        styleSheet = R"(
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e3a5f, stop:1 #0f2540);
            border-radius: 8px;
            border: 1px solid #94a3b8;
        )";
        break;
    default:
        styleSheet = R"(
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e3a5f, stop:1 #0f2540);
            border-radius: 8px;
            border: 1px solid #2d5a8a;
        )";
        break;
    }
    setStyleSheet(styleSheet);
}

} // namespace UI