#ifndef CLASH_QT_CLASH_H
#define CLASH_QT_CLASH_H

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QObject>
#include <QStandardPaths>

#include "yaml-cpp/yaml.h"

namespace YAML {
class Node;
}

class QProcess;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;

class Clash : public QObject {
Q_OBJECT

public:
    struct Profile {
        QString name;
        QString url;
        QString file;
        QDateTime updatedTime;
        int interval;
        QMap<QString, QString> selected;
    };


    class ProfileList {
    public:
        QList<Clash::Profile> &getList() { return list; }
        const Clash::Profile &getConstCurrentProfile() const { return list.at(index); }
        int getIndex() const { return index; }
        Clash::Profile &getCurrentProfile() { return list[index]; }
        void setIndex(int new_index) {
            if (list.isEmpty() || new_index < 0 || new_index >= list.size()) {
                index = -1;
            } else {
                index = new_index;
            }
        }
        void remove(int index) {
            Profile profile = list.takeAt(index);
            QFile file(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/" + profile.file);
            if (file.exists()) {
                file.remove();
            } else {
                qDebug() << "Failed to delete" << profile.file << "File not exist";
            }
        }
        void update(int index, Profile profile_new) {
            Profile &profile = list[index];
            QFile file(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/" + profile.file);
            if (file.exists()) {
                file.remove();
            } else {
                qDebug() << "Failed to delete" << profile.file << "File not exist";
            }
            profile.name = profile_new.name;
            profile.file = profile_new.file;
            profile.updatedTime = profile_new.updatedTime;
        }

    private:
        QList<Clash::Profile> list;
        int index;
    };

    class RestfulApi;

public:
    explicit Clash(QString program = "", QString clash_dir = "", QObject *parent = nullptr);
    void start();
    bool checkFiles();
    RestfulApi *api();
    QProcess *getProcess();
    void switchProfile(QString name);

public slots:
    void stop();

private:
    QProcess *process;
    RestfulApi *restfulApi;
    QString clash_program, clash_dir;
};

class Clash::RestfulApi : public QObject {
    Q_OBJECT
public:
    RestfulApi(QObject *parent = nullptr);

    void testConnection();

    void listenTraffic();
    void stopListenTraffic();

    void listenLog();
    void stopListenLog();

    void updateConfig();
    void updateConnection();
    void updateProxy();
    void updateProxySelector(QString group, QString name, bool update = true);

    void autoUpdateProxy(bool enable = true, int interval_ms = 1000);
    void autoUpdateConnection(bool enable = true, int interval_ms = 1000);
    void autoUpdateConfig(bool enable = true, int interval_ms = 1000);
    void updateProfile(const Profile &profile);
    void patchConfig(QString key, QVariant value);
    void stopTimer();

signals:
    void connected();
    void configUpdate(QByteArray rawJson);
    void trafficUpdate(int up, int down);
    void logReceived(QString type, QString payload);
    void errorHappened(QString content);
    void proxyDataReceived(QByteArray rawJson);
    void connectionDataReceived(QByteArray rawJson);
    void successChangeProfile();
    void failedChangeProfile(QString reason);

private:
    QString url;
    QNetworkAccessManager *manager;
    QNetworkReply *traffic, *log, *connection;
    QTimer *proxyTimer, *connectionTimer, *configTimer;
};


#endif  // CLASH_QT_CLASH_H
