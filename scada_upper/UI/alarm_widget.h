 #ifndef ALARM_WIDGET_H
 #define ALARM_WIDGET_H

 #include <QWidget>
 #include <QTableView>
 #include <QTabWidget>
 #include <QVBoxLayout>
 #include <QMenu>
 #include <QSoundEffect>
 #include <QModelIndex>
 #include <QDateTime>

 namespace UI {
 
 class AlarmWidget : public QWidget
 {
     Q_OBJECT
 public:
     explicit AlarmWidget(QWidget *parent = nullptr);
     ~AlarmWidget();

     void setSoundEnabled(bool enabled);
 
 private slots:
     void onUnackedContextMenuRequested(const QPoint &pos);
     void ackAlarmByForIndex(const QModelIndex &index);
 
 private:
     void initModels();
     void initViews();
     void refreshAll();
     void playAlarmSoundIfNeeded();

     QTabWidget *m_tabs;
     QTableView *m_unackedView;
     QTableView *m_historyView;
     QSoundEffect *m_alarmSound;
     bool m_soundEnabled;
 };

 } // namespace UI

 #endif // ALARM_WIDGET_H
