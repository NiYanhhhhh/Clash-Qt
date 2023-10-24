#ifndef CLASH_QT_TRAY_H
#define CLASH_QT_TRAY_H

#include <QAction>
#include <QSystemTrayIcon>

class Tray : public QSystemTrayIcon {
public:
    Tray(QObject *parent = nullptr);
    QAction* quit;
    QAction* preference;

public slots:
    void onReceiveError(QString content);

private:
    QMenu *menu;
};


#endif  // CLASH_QT_TRAY_H
