#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();
    void connectRequested();
    void disconnectRequested();

public slots:
    void setConnectionState(bool connected);

private slots:
    void onMinimizeClicked();
    void onMaximizeClicked();
    void onCloseClicked();
    void onConnectClicked();

private:
    void initUI();
    QHBoxLayout *m_layout;
    QLabel *m_titleLabel;
    QPushButton *m_connectBtn;
    QToolButton *m_minBtn;
    QToolButton *m_maxBtn;
    QToolButton *m_closeBtn;
    bool m_isMaximized;
    bool m_isConnected;
};

#endif // TITLEBAR_H