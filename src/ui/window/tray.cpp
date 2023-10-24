//
// Created by cyril on 2020/11/3.
//

#include "tray.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QMenu>

#include "clash/clash.h"
#include "util/instance.h"

Tray::Tray(QObject *parent) : QSystemTrayIcon(parent) {
    setIcon(QIcon(":/icon/clash"));
    setToolTip(QApplication::applicationName());


    menu = new QMenu;
    quit = new QAction(QIcon::fromTheme("application-exit"), tr("Quit"));
    preference = new QAction(QIcon::fromTheme("configure"), tr("Preference"));

    menu->addAction(preference);
    menu->addSeparator();
    menu->addAction(quit);
    setContextMenu(menu);

    connect(getInstance<Clash>().api(), &Clash::RestfulApi::errorHappened, this, &Tray::onReceiveError);
}
void Tray::onReceiveError(QString content) { this->showMessage("Error Happened", content, MessageIcon::Warning); }
