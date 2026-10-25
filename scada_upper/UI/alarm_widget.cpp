#include "alarm_widget.h"

#include <QHeaderView>
#include <QContextMenuEvent>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QUrl>
#include <QLabel>

namespace UI {

AlarmWidget::AlarmWidget(QWidget *parent)
    : QWidget(parent)
    , m_unackedView(nullptr)
    , m_historyView(nullptr)
    , m_alarmSound(nullptr)
    , m_soundEnabled(true)
    , m_tabs(nullptr)
{
    initViews();

    m_alarmSound = new QSoundEffect(this);
    m_alarmSound->setSource(QUrl("qrc:/sounds/alarm.wav"));
    m_alarmSound->setVolume(0.8);
    m_alarmSound->setLoopCount(1);
}

AlarmWidget::~AlarmWidget()
{
    delete m_alarmSound;
    delete m_unackedView;
    delete m_historyView;
}

void AlarmWidget::setSoundEnabled(bool enabled)
{
    m_soundEnabled = enabled;
    if (!enabled && m_alarmSound) {
        m_alarmSound->stop();
    }
}

void AlarmWidget::initModels()
{
}

void AlarmWidget::initViews()
{
    QString tableStyle = R"(
        QTableView {
            background: #132f4c;
            border: 1px solid #1e3a5f;
            font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #e0e0e0;
            gridline-color: #1e3a5f;
        }
        QTableView::item {
            padding: 8px 12px;
            border-bottom: 1px solid #1e3a5f;
        }
        QTableView::item:selected {
            background: #1e3a5f;
            color: #60a5fa;
        }
        QHeaderView::section {
            background: #0f2540;
            color: #60a5fa;
            padding: 10px 12px;
            border: none;
            border-bottom: 2px solid #1e3a5f;
            font-weight: bold;
            font-size: 12px;
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
    )";

    m_tabs = new QTabWidget(this);
    m_tabs->setStyleSheet(R"(
        QTabWidget {
            background: transparent;
            border: none;
        }
        QTabBar::tab {
            background: #0f2540;
            border: 1px solid #1e3a5f;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 10px 24px;
            margin-right: 3px;
            font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;
            font-size: 12px;
            color: #94a3b8;
        }
        QTabBar::tab:selected {
            background: #1e3a5f;
            color: #60a5fa;
            font-weight: bold;
        }
    )");

    QWidget *unackedPage = new QWidget();
    QVBoxLayout *unackedLayout = new QVBoxLayout(unackedPage);
    unackedLayout->setContentsMargins(4, 8, 4, 4);
    
    QLabel *noDataLabel1 = new QLabel("\u6682\u65E0\u672A\u786E\u8BA4\u62A5\u8B66", unackedPage);
    noDataLabel1->setAlignment(Qt::AlignCenter);
    noDataLabel1->setStyleSheet("color: #94a3b8; font-size: 14px; padding: 30px;");
    unackedLayout->addWidget(noDataLabel1);
    m_tabs->addTab(unackedPage, "\u672A\u786E\u8BA4\u62A5\u8B66");

    QWidget *historyPage = new QWidget();
    QVBoxLayout *historyLayout = new QVBoxLayout(historyPage);
    historyLayout->setContentsMargins(4, 8, 4, 4);
    
    QLabel *noDataLabel2 = new QLabel("\u6682\u65E0\u5386\u53F2\u62A5\u8B66", historyPage);
    noDataLabel2->setAlignment(Qt::AlignCenter);
    noDataLabel2->setStyleSheet("color: #94a3b8; font-size: 14px; padding: 30px;");
    historyLayout->addWidget(noDataLabel2);
    m_tabs->addTab(historyPage, "\u5386\u53F2\u62A5\u8B66");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 8, 0, 0);
    mainLayout->setSpacing(6);
    
    QLabel *titleLabel = new QLabel("\u62A5\u8B66\u4FE1\u606F");
    titleLabel->setStyleSheet("color: #ef4444; font-size: 13px; font-weight: bold; padding: 6px 12px;");
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(4);
    mainLayout->addWidget(m_tabs, 1);
    setLayout(mainLayout);
}

void AlarmWidget::onUnackedContextMenuRequested(const QPoint &pos)
{
}

void AlarmWidget::ackAlarmByForIndex(const QModelIndex &index)
{
}

void AlarmWidget::refreshAll()
{
}

void AlarmWidget::playAlarmSoundIfNeeded()
{
}

} // namespace UI
