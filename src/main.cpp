#include <QApplication>
#include <csignal>

#include "clash/clash.h"
#include "ui/window/mainwindow.h"
#include "ui/window/tray.h"
#include "util/instance.h"

void signalhandler(int sig) {
    if (sig == SIGINT) {
        qApp->quit();
    }
}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    signal(SIGINT, signalhandler);

    a.setApplicationName("clash-qt");
    a.setWindowIcon(QIcon(":/icon/clash"));
    a.setQuitOnLastWindowClosed(false);

    getInstance<MainWindow>();
    getInstance<Tray>().show();
    getInstance<Clash>().start();

    return a.exec();
}
