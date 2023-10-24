#include "mainwindow.h"

#include <qapplication.h>
#include <yaml-cpp/yaml.h>

#include <QButtonGroup>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QtGui/QDesktopServices>

#include "clash/clash.h"
#include "ui/widget/netspeedlabel.h"
#include "ui/widget/proxygroupwidget.h"
#include "ui/widget/proxywidget.h"
#include "ui/window/connectiondialog.h"
#include "ui/window/logdialog.h"
#include "ui/window/profiledialog.h"
#include "ui/window/tray.h"
#include "ui_mainwindow.h"
#include "util/instance.h"
#include "util/qtyaml.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), clash(getInstance<Clash>()) {
    ui->setupUi(this);

    QValidator *portValidator = new QIntValidator(0, 65565, this);
    ui->mixedPort->setValidator(portValidator);
    ui->httpPort->setValidator(portValidator);
    ui->socksPort->setValidator(portValidator);

    connect(ui->mixedPortSend, &QPushButton::released, this, &MainWindow::sendMixedPort);
    connect(ui->mixedPort, &QLineEdit::returnPressed, this, &MainWindow::sendMixedPort);
    connect(ui->httpPortSend, &QPushButton::released, this, &MainWindow::sendHttpPort);
    connect(ui->httpPort, &QLineEdit::returnPressed, this, &MainWindow::sendHttpPort);
    connect(ui->socksPortSend, &QPushButton::released, this, &MainWindow::sendSocksPort);
    connect(ui->socksPort, &QLineEdit::returnPressed, this, &MainWindow::sendSocksPort);

    log_level_group = new QButtonGroup(this);
    log_level_group->setExclusive(true);
    log_level_group->addButton(ui->debug);
    log_level_group->addButton(ui->info);
    log_level_group->addButton(ui->warning);
    log_level_group->addButton(ui->error);
    log_level_group->addButton(ui->silent);
    connect(log_level_group, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onLogLevelClicked);

    model_group = new QButtonGroup(this);
    model_group->addButton(ui->global);
    model_group->addButton(ui->rule);
    model_group->addButton(ui->direct);
    connect(model_group, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onModeClicked);
    connect(clash.api(), &Clash::RestfulApi::configUpdate, this, &MainWindow::onConfigUpdate);

    connect(ui->allowLan, &QCheckBox::clicked, this, &MainWindow::setAllowLan);

    loadProfiles();
    connect(getInstance<Clash>().api(), &Clash::RestfulApi::proxyDataReceived, this, &MainWindow::updateProxies);
    connect(ui->profile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onProfileChanged);

    net_speed_label = new NetSpeedLabel(this);
    statusBar()->addPermanentWidget(net_speed_label);
    connect(clash.api(), &Clash::RestfulApi::trafficUpdate, net_speed_label, &NetSpeedLabel::setSpeed);

    connect(ui->profileEdit, &QPushButton::clicked, this, &MainWindow::showProfileDialog);

    connect(clash.api(), &Clash::RestfulApi::connected, this, &MainWindow::onClashStarted);
    connect(qApp, &QApplication::aboutToQuit, this, [this] { this->saveProfiles(); });

    connect(ui->action_clash_folder, &QAction::triggered, this, &MainWindow::openClashFolder);
    connect(ui->action_log, &QAction::triggered, this, &MainWindow::showLogDialog);
    connect(ui->action_connection, &QAction::triggered, this, &MainWindow::showConnectionDialog);
    connect(ui->action_quit, &QAction::triggered, qApp, &QApplication::quit);

    tray = new Tray(this);
    connect(tray->quit, &QAction::triggered, this, [] { qApp->quit(); });
    connect(tray->preference, &QAction::triggered, this, [this]{
            show();
            raise();
            activateWindow();
    });
    connect(tray, &QSystemTrayIcon::activated, this, &MainWindow::onActivate);
    tray->show();
}

MainWindow::~MainWindow() {
    qDebug() << "Destructing MainWindow";
    delete ui;
}

void MainWindow::onActivate(QSystemTrayIcon::ActivationReason reason) {
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
        {
            if (!isActiveWindow()) {
                if (isMinimized()) {
                    showNormal();
                }
                show();
                raise();
                activateWindow();
            } else {
                close();
            }
            break;
        }
        default:;
    }
}
void MainWindow::onLogLevelClicked(QAbstractButton *button) { clash.api()->patchConfig("log-level", button->objectName()); }
void MainWindow::onModeClicked(QAbstractButton *button) { clash.api()->patchConfig("mode", button->objectName()); }

void setText(QLineEdit *edit, const QString &text) {
    if (edit->hasFocus()) {
        return;
    }
    edit->setText(text);
}
void MainWindow::onConfigUpdate(const QByteArray &rawJson) {
    QJsonParseError parseError{};
    QJsonDocument document = QJsonDocument::fromJson(rawJson, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse config data" << parseError.errorString() << rawJson;
        return;
    }
    QJsonObject config = document.object();

    setText(ui->mixedPort, QString::number(config["mixed-port"].toInt(0)));
    setText(ui->httpPort, QString::number(config["port"].toInt(0)));
    setText(ui->socksPort, QString::number(config["socks-port"].toInt(0)));

    ui->allowLan->setChecked(config["allow-lan"].toBool(false));

    QString mode = config["mode"].toString();
    if (mode == "global") {
        ui->global->setChecked(true);
    } else if (mode == "rule") {
        ui->rule->setChecked(true);
    } else if (mode == "direct") {
        ui->direct->setChecked(true);
    } else {
        qDebug() << "Error Mode: " + mode;
    }

    QString level = config["log-level"].toString();
    if (level == "debug") {
        ui->debug->setChecked(true);
    } else if (level == "info") {
        ui->info->setChecked(true);
    } else if (level == "warning" || level == "warn") {
        ui->warning->setChecked(true);
    } else if (level == "error") {
        ui->error->setChecked(true);
    } else if (level == "silent") {
        ui->silent->setChecked(true);
    } else {
        qDebug() << "Error Level: " + level;
        ui->warning->setChecked(true);
    }
}


void MainWindow::updateProxies(const QByteArray &rawjson) {
    QJsonParseError error{};
    QJsonDocument document = QJsonDocument::fromJson(rawjson, &error);
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse proxy data" << error.errorString();
        qDebug() << rawjson;
        return;
    }
    QJsonObject obj = document.object();
    if (!obj.contains("proxies")) {
        qDebug() << "Error Proxy Data. No proxies found";
        return;
    }
    QJsonObject proxies = obj["proxies"].toObject();
    QStringList groupList = proxies.keys();
    if (profile_list.getIndex() >= 0 && profile_list.getCurrentProfile().selected.size() != groupList.size()) {
        profile_list.getCurrentProfile().selected.clear();
    }
    if (groups == groupList) {
        // group not change
        int i = 0;
        for (auto groupName : groupList) {
            auto item = group_items.value(groupName);
            if (item == nullptr) {
                continue;
            }
            updateGroup(proxies, groupName, item);
        }
    } else {
        // clear data if group changed
        ui->proxies->clear();
        group_items.clear();
        this->groups = groupList;
        int i = 0;
        for (const auto &groupName : groups) {
            auto proxyGroup = proxies[groupName].toObject();
            QString type = proxyGroup["type"].toString();
            if (type != "Selector") {
                continue;
            }
            auto *groupWidget = new ProxyGroupWidget(groupName, proxyGroup["now"].toString("NULL"), type, this);
            auto *item = new QTreeWidgetItem(ui->proxies);
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            group_items[groupName] = item;
            if (groupName == "Proxy") {
                ui->proxies->insertTopLevelItem(0, item);
            } else {
                ui->proxies->insertTopLevelItem(ui->proxies->topLevelItemCount(), item);
            }
            ui->proxies->setItemWidget(item, 0, groupWidget);
            updateGroup(proxies, groupName, item);
        }
    }
}

void MainWindow::updateGroup(const QJsonObject &proxies, QString group, QTreeWidgetItem *item) {
    auto array = proxies[group].toObject()["all"].toArray();
    QStringList list;
    for (auto it = array.begin(); it != array.end(); it++) {
        list.append((*it).toString("Error"));
    }
    if (group_proxies.value(group) == list) {
        // proxies in this group not changed.
    } else {
        group_proxies[group] = list;
        // remove previous data;
        for (int i = 0; i < item->childCount(); ++i) {
            auto child = item->child(i);
            delete ui->proxies->itemWidget(child, 0);
            item->removeChild(child);
            delete child;
        }
        // add new data
        for (const auto &proxy_name : list) {
            auto *widget = new ProxyWidget(group, proxy_name, "type", this);
            auto *child = new QTreeWidgetItem(item);
            item->addChild(child);
            ui->proxies->setItemWidget(child, 0, widget);
        }
    }
    QString selected = proxies[group].toObject()["now"].toString("NAN");
    static_cast<ProxyGroupWidget *>(ui->proxies->itemWidget(item, 0))->setSelected(selected);
    for (int i = 0; i < item->childCount(); ++i) {
        auto child_item = item->child(i);
        auto proxy_widget = static_cast<ProxyWidget *>(ui->proxies->itemWidget(child_item, 0));
        child_item->setSelected(proxy_widget->getName() == selected);
    }
    if (profile_list.getIndex() >= 0) {
        profile_list.getCurrentProfile().selected[group] = selected;
    }
}

template <>
struct YAML::convert<Clash::Profile> {
    static Node encode(const Clash::Profile &rhs) {
        Node node;
        node["name"] = rhs.name.toStdString();
        node["url"] = rhs.url.toStdString();
        node["file"] = rhs.file.toStdString();
        node["time"] = rhs.updatedTime.toString(Qt::ISODate).toStdString();
        node["interval"] = rhs.interval;
        node["selected"] = rhs.selected;
        return node;
    }

    static bool decode(const Node &node, Clash::Profile &rhs) {
        if (!node.IsMap()) return false;
        rhs.name = QString::fromStdString(node["name"].as<std::string>("ERROR"));
        rhs.url = QString::fromStdString(node["url"].as<std::string>("ERROR"));
        rhs.file = QString::fromStdString(node["file"].as<std::string>("ERROR"));
        rhs.updatedTime = QDateTime::fromString(QString::fromStdString(node["time"].as<std::string>("2017-07-24T15:46:29")), Qt::ISODate);
        rhs.interval = node["interval"].as<int>(-1);
        rhs.selected = node["selected"].as<QMap<QString, QString>>();
        return true;
    }
};

void MainWindow::useProfiles() {
    ui->profile->clear();
    for (auto profile : profile_list.getList()) {
        ui->profile->addItem(profile.name);
    }
    ui->profile->setCurrentIndex(profile_list.getIndex());
}
void MainWindow::loadProfiles() {
    QString filename = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/profiles.yaml";
    if (!QFile::exists(filename)) {
        return;
    }
    YAML::Node profileConfig = YAML::LoadFile(filename.toStdString());
    YAML::Node profileListNode = profileConfig["profiles"];
    if (!profileListNode.IsSequence()) {
        qDebug() << "Profile Config Error";
    } else {
        for (YAML::Node node : profileListNode) {
            profile_list.getList().append(node.as<Clash::Profile>(Clash::Profile()));
        }
    }
    if (profile_list.getList().isEmpty()) {
        qDebug() << "No profile";
        return;
    }
    profile_list.setIndex(profileConfig["selected"].as<int>(-1));
    useProfiles();
}
void MainWindow::saveProfiles() {
    YAML::Node profiles;
    for (auto profile : profile_list.getList()) {
        profiles.push_back(profile);
    }
    YAML::Node node;
    node["profiles"] = profiles;
    node["selected"] = profile_list.getIndex();

    YAML::Emitter out;
    out << node;
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile");
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }
    QFile file(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/profiles.yaml");
    file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    QTextStream textStream(&file);
    textStream << QString(out.c_str());  // do not write out.c_str() directly which will lead to mojibake;
    textStream.flush();
    file.close();
}
void MainWindow::onProfileChanged(int index) {
    profile_list.setIndex(index);
    if (profile_list.getIndex() >= 0) {
        clash.api()->updateProfile(profile_list.getConstCurrentProfile());
    }
}
void MainWindow::onClashStarted() {
    clash.api()->listenTraffic();
    clash.api()->autoUpdateProxy();
    clash.api()->autoUpdateConfig();
    // switch profile
    onProfileChanged(ui->profile->currentIndex());
}

int getPort(QLineEdit *edit) {
    int port = edit->text().toInt();
    edit->clearFocus();
    return port;
}
void MainWindow::sendMixedPort() { clash.api()->patchConfig("mixed-port", getPort(ui->mixedPort)); }
void MainWindow::sendHttpPort() { clash.api()->patchConfig("port", getPort(ui->httpPort)); }
void MainWindow::sendSocksPort() { clash.api()->patchConfig("socks-port", getPort(ui->socksPort)); }
void MainWindow::setAllowLan(bool checked) { clash.api()->patchConfig("allow-lan", checked); }

void MainWindow::showProfileDialog() {
    ProfileDialog manager(profile_list, this);
    manager.exec();
    useProfiles();
}
void MainWindow::showLogDialog() { (new LogDialog)->show(); }
void MainWindow::showConnectionDialog() { (new ConnectionDialog)->show(); }
void MainWindow::openClashFolder() {
    QDesktopServices::openUrl(QUrl(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash", QUrl::TolerantMode));
}
