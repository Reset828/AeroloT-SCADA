#include "titlebar.h"

#include <QStyle>
#include <QMessageBox>
#include <QApplication>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , m_isMaximized(false)
    , m_isConnected(false)
{
    initUI();
}

void TitleBar::initUI()
{
    setFixedHeight(40);
    setStyleSheet("TitleBar { background: #0d1b2a; border-bottom: 1px solid #1e3a5f; }");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(15, 0, 10, 0);
    m_layout->setSpacing(0);

    m_titleLabel = new QLabel("\u7A7A\u538B\u7AD9\u7269\u8054\u63A7SCADA\u7CFB\u7EDF", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet("color: #60a5fa; letter-spacing: 1px;");
    m_layout->addWidget(m_titleLabel);
    m_layout->addStretch();

    m_connectBtn = new QPushButton("\u8FDE\u63A5\u4E0B\u4F4D\u673A", this);
    m_connectBtn->setFixedSize(120, 32);
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    m_connectBtn->setStyleSheet(R"(
        QPushButton {
            color: #4ade80;
            border: 1px solid #4ade80;
            border-radius: 4px;
            background: transparent;
            font-size: 12px;
            font-weight: bold;
            padding: 4px 12px;
        }
        QPushButton:hover {
            background: #4ade80;
            color: #0d1b2a;
        }
        QPushButton:pressed {
            background: #22c55e;
            border-color: #22c55e;
        }
    )");
    connect(m_connectBtn, &QPushButton::clicked, this, &TitleBar::onConnectClicked);
    m_layout->addWidget(m_connectBtn);

    QWidget *buttonContainer = new QWidget(this);
    buttonContainer->setStyleSheet("background: transparent;");
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    m_minBtn = new QToolButton(this);
    m_minBtn->setText("\u2014");
    m_minBtn->setToolTip("\u6700\u5C0F\u5316");
    m_minBtn->setFixedSize(46, 40);
    m_minBtn->setStyleSheet(R"(
        QToolButton {
            color: #e0e0e0;
            border: none;
            background: transparent;
            font-size: 14px;
        }
        QToolButton:hover {
            background: #1e3a5f;
        }
    )");
    connect(m_minBtn, &QToolButton::clicked, this, &TitleBar::onMinimizeClicked);
    buttonLayout->addWidget(m_minBtn);

    m_maxBtn = new QToolButton(this);
    m_maxBtn->setText("\u25A1");
    m_maxBtn->setToolTip("\u6700\u5927\u5316");
    m_maxBtn->setFixedSize(46, 40);
    m_maxBtn->setStyleSheet(R"(
        QToolButton {
            color: #e0e0e0;
            border: none;
            background: transparent;
            font-size: 14px;
        }
        QToolButton:hover {
            background: #1e3a5f;
        }
    )");
    connect(m_maxBtn, &QToolButton::clicked, this, &TitleBar::onMaximizeClicked);
    buttonLayout->addWidget(m_maxBtn);

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setText("\u00D7");
    m_closeBtn->setToolTip("\u5173\u95ED");
    m_closeBtn->setFixedSize(46, 40);
    m_closeBtn->setStyleSheet(R"(
        QToolButton {
            color: #e0e0e0;
            border: none;
            background: transparent;
            font-size: 16px;
        }
        QToolButton:hover {
            background: #dc2626;
            color: white;
        }
    )");
    connect(m_closeBtn, &QToolButton::clicked, this, &TitleBar::onCloseClicked);
    buttonLayout->addWidget(m_closeBtn);

    m_layout->addWidget(buttonContainer);
}

void TitleBar::onMinimizeClicked()
{
    emit minimizeRequested();
}

void TitleBar::onMaximizeClicked()
{
    m_isMaximized = !m_isMaximized;
    if (m_isMaximized) {
        m_maxBtn->setText("\u25A3");
    } else {
        m_maxBtn->setText("\u25A1");
    }
    emit maximizeRequested();
}

void TitleBar::onCloseClicked()
{
    emit closeRequested();
}

void TitleBar::onConnectClicked()
{
    if (m_isConnected) {
        emit disconnectRequested();
    } else {
        emit connectRequested();
    }
}

void TitleBar::setConnectionState(bool connected)
{
    m_isConnected = connected;
    if (connected) {
        m_connectBtn->setText("\u65AD\u5F00\u8FDE\u63A5");
        m_connectBtn->setStyleSheet(R"(
            QPushButton {
                color: #ef4444;
                border: 1px solid #ef4444;
                border-radius: 4px;
                background: transparent;
                font-size: 12px;
                font-weight: bold;
                padding: 4px 12px;
            }
            QPushButton:hover {
                background: #ef4444;
                color: #0d1b2a;
            }
            QPushButton:pressed {
                background: #dc2626;
                border-color: #dc2626;
            }
        )");
    } else {
        m_connectBtn->setText("\u8FDE\u63A5\u4E0B\u4F4D\u673A");
        m_connectBtn->setStyleSheet(R"(
            QPushButton {
                color: #4ade80;
                border: 1px solid #4ade80;
                border-radius: 4px;
                background: transparent;
                font-size: 12px;
                font-weight: bold;
                padding: 4px 12px;
            }
            QPushButton:hover {
                background: #4ade80;
                color: #0d1b2a;
            }
            QPushButton:pressed {
                background: #22c55e;
                border-color: #22c55e;
            }
        )");
    }
}