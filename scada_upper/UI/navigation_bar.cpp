#include "navigation_bar.h"

#include <QHBoxLayout>

namespace UI {

NavigationBar::NavigationBar(QWidget *parent)
    : QWidget(parent)
    , m_currentIndex(0)
{
    initUI();
}

NavigationBar::~NavigationBar()
{
}

void NavigationBar::initUI()
{
    setFixedWidth(200);
    setStyleSheet("background: #0f2540; border-right: 1px solid #1e3a5f;");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 20, 10, 10);
    m_mainLayout->setSpacing(8);

    m_titleLabel = new QLabel("设备监控", this);
    m_titleLabel->setStyleSheet("color: #60a5fa; font-size: 16px; font-weight: bold; padding: 12px 8px; border-bottom: 2px solid #1e3a5f;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(m_titleLabel);

    m_mainLayout->addSpacing(15);

    QStringList buttonTexts = {"实时监测", "报警记录", "历史查询", "通信日志"};
    QStringList buttonIcons = {"📊", "⚠️", "📈", "📝"};

    for (int i = 0; i < buttonTexts.size(); ++i) {
        QPushButton *btn = new QPushButton(buttonIcons[i] + "  " + buttonTexts[i], this);
        btn->setFixedHeight(50);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("navIndex", i);
        btn->setStyleSheet(R"(
            QPushButton {
                background: transparent;
                border: none;
                border-radius: 6px;
                color: #94a3b8;
                font-size: 14px;
                font-weight: 500;
                padding: 10px 16px;
                text-align: left;
            }
            QPushButton:hover {
                background: rgba(30, 58, 95, 0.6);
                color: #e0e0e0;
            }
        )");
        connect(btn, &QPushButton::clicked, this, &NavigationBar::onButtonClicked);
        m_navButtons.append(btn);
        m_mainLayout->addWidget(btn);
    }

    m_mainLayout->addStretch();

    QLabel *versionLabel = new QLabel("v1.0.0", this);
    versionLabel->setStyleSheet("color: #64748b; font-size: 11px; padding: 8px;");
    versionLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(versionLabel);

    updateButtonStyles();
}

void NavigationBar::updateButtonStyles()
{
    for (int i = 0; i < m_navButtons.size(); ++i) {
        if (i == m_currentIndex) {
            m_navButtons[i]->setStyleSheet(R"(
                QPushButton {
                    background: #1e3a5f;
                    border: none;
                    border-radius: 6px;
                    color: #60a5fa;
                    font-size: 14px;
                    font-weight: bold;
                    padding: 10px 16px;
                    text-align: left;
                }
                QPushButton:hover {
                    background: #244a7a;
                    color: #60a5fa;
                }
            )");
        } else {
            m_navButtons[i]->setStyleSheet(R"(
                QPushButton {
                    background: transparent;
                    border: none;
                    border-radius: 6px;
                    color: #94a3b8;
                    font-size: 14px;
                    font-weight: 500;
                    padding: 10px 16px;
                    text-align: left;
                }
                QPushButton:hover {
                    background: rgba(30, 58, 95, 0.6);
                    color: #e0e0e0;
                }
            )");
        }
    }
}

void NavigationBar::setCurrentIndex(int index)
{
    if (index >= 0 && index < m_navButtons.size() && index != m_currentIndex) {
        m_currentIndex = index;
        updateButtonStyles();
        emit navigationChanged(index);
    }
}

int NavigationBar::currentIndex() const
{
    return m_currentIndex;
}

void NavigationBar::onButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        int index = btn->property("navIndex").toInt();
        setCurrentIndex(index);
    }
}

}
