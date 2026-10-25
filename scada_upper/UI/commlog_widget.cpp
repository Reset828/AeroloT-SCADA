#include "commlog_widget.h"
#include "../DataStore/datastore.h"

#include <QDebug>
#include <QDateTime>

namespace UI {

CommLogWidget::CommLogWidget(QWidget *parent)
    : CommLogWidget(DataStore::DataStore::instance(), parent)
{
}

CommLogWidget::CommLogWidget(DataStore::DataStore& dataStore, QWidget *parent)
    : QWidget(parent)
    , m_logCount(0)
    , m_dataStore(dataStore)
{
    initUI();
}

CommLogWidget::~CommLogWidget()
{
}

void CommLogWidget::loadHistoryLogs()
{
    m_logDisplay->clear();
    m_logCount = 0;
    
    const QVector<DataStore::CommLogEntry> records =
        m_dataStore.getCommLogs(0, 0, 500);
    int count = 0;
    for (const DataStore::CommLogEntry& record : records) {
        const qint64 timestamp = record.timestamp;
        const QString& logType = record.logType;
        const QString& direction = record.direction;
        const QString& deviceId = record.deviceId;
        const QString& content = record.content;
        
        QString timeStr = QDateTime::fromMSecsSinceEpoch(timestamp).toString("yyyy-MM-dd HH:mm:ss.zzz");
        appendLogEntry(timeStr, direction, deviceId, QString("[%1] %2").arg(logType).arg(content));
        count++;
    }
    
    if (count > 0) {
        m_logDisplay->append("<div style='color: #64748b; font-style: italic; margin-top: 10px;'>--- 以上是历史通信记录 ---</div>");
    } else {
        m_logDisplay->append("<div style='color: #64748b; font-style: italic;'>暂无通信记录</div>");
    }
}

void CommLogWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_mainLayout->setSpacing(15);

    createControlPanel();
    createLogDisplay();
}

void CommLogWidget::createControlPanel()
{
    m_controlPanel = new QWidget(this);
    m_controlPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    m_controlPanel->setFixedHeight(60);
    
    QHBoxLayout *controlLayout = new QHBoxLayout(m_controlPanel);
    controlLayout->setContentsMargins(15, 10, 15, 10);
    controlLayout->setSpacing(15);
    
    QLabel *filterLabel = new QLabel("日志级别:", this);
    filterLabel->setStyleSheet("color: #60a5fa; font-size: 13px; font-weight: bold;");
    controlLayout->addWidget(filterLabel);
    
    m_levelFilter = new QComboBox(this);
    m_levelFilter->addItem("全部");
    m_levelFilter->addItem("Modbus通信");
    m_levelFilter->addItem("WebSocket");
    m_levelFilter->addItem("Webhook");
    m_levelFilter->setStyleSheet(R"(
        QComboBox {
            background: #132f4c;
            border: 1px solid #1e3a5f;
            border-radius: 4px;
            color: #e0e0e0;
            font-size: 12px;
            padding: 6px 10px;
            min-width: 120px;
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
    connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CommLogWidget::onLevelChanged);
    controlLayout->addWidget(m_levelFilter);
    
    controlLayout->addStretch();
    
    m_logCountLabel = new QLabel("共 0 条日志", this);
    m_logCountLabel->setStyleSheet("color: #94a3b8; font-size: 12px;");
    controlLayout->addWidget(m_logCountLabel);
    
    m_clearBtn = new QPushButton("清空日志", this);
    m_clearBtn->setFixedWidth(90);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(R"(
        QPushButton {
            background: #dc2626;
            border: none;
            border-radius: 4px;
            color: white;
            font-size: 13px;
            font-weight: bold;
            padding: 8px 16px;
        }
        QPushButton:hover {
            background: #ef4444;
        }
        QPushButton:pressed {
            background: #b91c1c;
        }
    )");
    connect(m_clearBtn, &QPushButton::clicked, this, &CommLogWidget::onClearClicked);
    controlLayout->addWidget(m_clearBtn);
    
    m_mainLayout->addWidget(m_controlPanel);
}

void CommLogWidget::createLogDisplay()
{
    m_logPanel = new QWidget(this);
    m_logPanel->setStyleSheet("background: #0f2540; border: 1px solid #1e3a5f; border-radius: 8px;");
    
    QVBoxLayout *logLayout = new QVBoxLayout(m_logPanel);
    logLayout->setContentsMargins(10, 10, 10, 10);
    logLayout->setSpacing(10);
    
    QLabel *logTitle = new QLabel("通信日志", this);
    logTitle->setStyleSheet("color: #60a5fa; font-size: 14px; font-weight: bold;");
    logLayout->addWidget(logTitle);
    
    m_logDisplay = new QTextEdit(this);
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setStyleSheet(R"(
        QTextEdit {
            background: #0a1929;
            border: 1px solid #1e3a5f;
            border-radius: 4px;
            color: #e0e0e0;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            padding: 10px;
            line-height: 1.5;
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
    
    logLayout->addWidget(m_logDisplay, 1);
    
    m_mainLayout->addWidget(m_logPanel, 1);
}

void CommLogWidget::appendLog(const QString &direction, const QString &deviceId, const QString &content)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    appendLogEntry(timestamp, direction, deviceId, content);
}

void CommLogWidget::appendModbusLog(int slaveId, const QString &operation, const QString &data)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString deviceId = QString("从站%1").arg(slaveId);
    QString direction = operation.startsWith("读") ? "TX→RX" : "TX→";
    QString content = QString("[%1] %2").arg(operation).arg(data);
    appendLogEntry(timestamp, direction, deviceId, content);
}

void CommLogWidget::appendWebSocketLog(const QString &clientId, const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    appendLogEntry(timestamp, "WS", clientId, message);
}

void CommLogWidget::appendWebhookLog(const QString &url, int status, const QString &response)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString content = QString("[Webhook] URL: %1 | 状态: %2 | 响应: %3").arg(url).arg(status).arg(response);
    appendLogEntry(timestamp, "HTTP", "Webhook", content);
}

void CommLogWidget::appendLogEntry(const QString &timestamp, const QString &direction, const QString &deviceId, const QString &content)
{
    QString directionColor = getDirectionColor(direction);
    QString directionIcon = getDirectionIcon(direction);
    
    QString html = QString("<div style='margin: 2px 0; line-height: 1.6;'>"
                          "<span style='color: #64748b;'>[%1]</span> "
                          "<span style='color: %2; font-weight: bold;'>%3</span> "
                          "<span style='color: #60a5fa;'>[%4]</span> "
                          "<span style='color: #e0e0e0;'>%5</span>"
                          "</div>")
                      .arg(timestamp)
                      .arg(directionColor)
                      .arg(directionIcon + " " + direction)
                      .arg(deviceId)
                      .arg(content);
    
    m_logDisplay->append(html);
    
    m_logCount++;
    m_logCountLabel->setText(QString("共 %1 条日志").arg(m_logCount));
    
    QTextCursor cursor = m_logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logDisplay->setTextCursor(cursor);
    
    if (m_logCount > 10000) {
        m_logDisplay->clear();
        m_logCount = 0;
        m_logDisplay->append("<div style='color: #64748b; font-style: italic;'>日志已自动清空</div>");
    }
}

void CommLogWidget::onClearClicked()
{
    m_logDisplay->clear();
    m_logCount = 0;
    m_logCountLabel->setText("共 0 条日志");
    m_logDisplay->append("<div style='color: #64748b; font-style: italic;'>日志已清空</div>");
}

void CommLogWidget::onLevelChanged(int index)
{
}

QString CommLogWidget::getDirectionColor(const QString &direction)
{
    if (direction.contains("TX")) return "#4ade80";
    if (direction.contains("RX")) return "#60a5fa";
    if (direction == "WS") return "#fbbf24";
    if (direction == "HTTP") return "#f472b6";
    return "#94a3b8";
}

QString CommLogWidget::getDirectionIcon(const QString &direction)
{
    if (direction.contains("TX→RX")) return "⇄";
    if (direction.contains("TX")) return "→";
    if (direction.contains("RX")) return "←";
    if (direction == "WS") return "🔌";
    if (direction == "HTTP") return "🌐";
    return "•";
}

}
