#ifndef NAVIGATION_BAR_H
#define NAVIGATION_BAR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

namespace UI {

class NavigationBar : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationBar(QWidget *parent = nullptr);
    ~NavigationBar();

    void setCurrentIndex(int index);
    int currentIndex() const;

signals:
    void navigationChanged(int index);

private slots:
    void onButtonClicked();

private:
    void initUI();
    void updateButtonStyles();

    QVBoxLayout *m_mainLayout;
    QLabel *m_titleLabel;
    QVector<QPushButton*> m_navButtons;
    int m_currentIndex;

    static const int NAV_REALTIME = 0;
    static const int NAV_ALARM = 1;
    static const int NAV_HISTORY = 2;
    static const int NAV_COMMLOG = 3;
};

}

#endif
