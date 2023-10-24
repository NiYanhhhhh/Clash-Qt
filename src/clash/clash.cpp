#include "clash.h"
#include <qapplication.h>
#include <qprocess.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>


Clash::Clash(QString program, QString clash_dir, QObject *parent) : QObject(parent) {
    process = new QProcess(this);
    restfulApi = new RestfulApi(this);
    this->clash_program = std::move(program);
    this->clash_dir = std::move(clash_dir);
    process->setStandardOutputFile("/tmp/clash_output.txt");

    if (clash_program.isEmpty()) {
        clash_program = QStandardPaths::findExecutable("clash");
        if (clash_program.isEmpty()) {
            clash_program = QCoreApplication::applicationDirPath() + "/clash";
        }
    }

    connect(qApp, &QApplication::aboutToQuit, this, &Clash::stop);
}
void Clash::start() {
    qDebug() << "Starting Clash Process:" + clash_program;
    if (!checkFiles()) {
        qDebug() << "Failed to check files";
        return;
    }
    auto parm = clash_dir.isEmpty() ? QStringList() : QStringList() << "-d" << clash_dir;
    qDebug() << parm;
    process->start(clash_program, clash_dir.isEmpty() ? QStringList() : QStringList() << "-d" << clash_dir);
    process->waitForStarted(1000);
    QTimer::singleShot(200, this, [this] {
        restfulApi->testConnection();
        qDebug() << process->processId();
    });
}
void Clash::stop() {
    restfulApi->stopTimer();
    process->terminate();
    while (!process->waitForFinished(1000)) {
        process->kill();
    }
    process->close();
}
void Clash::switchProfile(QString name) {}
Clash::RestfulApi *Clash::api() { return restfulApi; }
QProcess *Clash::getProcess() { return process; }
bool Clash::checkFiles() {
    if (clash_dir.isEmpty()) {
        clash_dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash";
    }
    QDir clash(clash_dir);
    if (!clash.exists()) {
        qDebug() << "Clash Dir not exist.";
        if (!clash.mkpath(clash_dir)) {
            qDebug() << "Failed to create folder " + clash_dir;
            return false;
        }
    }

    QFile clash_config(clash_dir + "/config.yaml");
    if (!clash_config.exists()) {
        qDebug() << "Clash Config file not exist.";
        QString config_file_path = QStandardPaths::locate(QStandardPaths::AppDataLocation, "config.yaml", QStandardPaths::LocateFile);
        if (config_file_path.isEmpty()) {
            config_file_path = QCoreApplication::applicationDirPath() + "/res/config.yaml";
            if (!QFile::exists(config_file_path)) {
                qDebug() << "Failed to find default config.yaml.";
                return false;
            }
        }
        if (!QFile::copy(config_file_path, clash_dir + "/config.yaml")) {
            qDebug() << "Failed to copy default config file.";
            return false;
        }
    }

    QFile geo_file(clash_dir + "/Country.mmdb");
    if (!geo_file.exists()) {
        qDebug() << "Clash Country.mmdb file not exist.";
        QString geo_file_path = QStandardPaths::locate(QStandardPaths::AppDataLocation, "Country.mmdb", QStandardPaths::LocateFile);
        if (geo_file_path.isEmpty()) {
            geo_file_path = QCoreApplication::applicationDirPath() + "/res/Country.mmdb";
            qDebug() << "Failed to find default Country.mmdb.";
            return false;
        }
        if (!QFile::copy(geo_file_path, clash_dir + "/Country.mmdb")) {
            qDebug() << "Failed to copy default GEO file.";
            return false;
        }
    }
    return true;
}

#define CONFIG_URL url + "configs"
#define TRAFFIC_URL url + "traffic"
#define LOG_URL url + "logs"
#define PROXY_URL url + "proxies"
#define CONNECTION_URL url + "connections"

Clash::RestfulApi::RestfulApi(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    log = traffic = connection = nullptr;
    url = "http://127.0.0.1:9090/";

    proxyTimer = new QTimer;
    connect(proxyTimer, &QTimer::timeout, this, &RestfulApi::updateProxy);
    connectionTimer = new QTimer;
    connect(connectionTimer, &QTimer::timeout, this, &RestfulApi::updateConnection);
    configTimer = new QTimer;
    connect(configTimer, &QTimer::timeout, this, &RestfulApi::updateConfig);
    // connect(qApp, &QApplication::aboutToQuit, this, &RestfulApi::stopTimer);
}

void Clash::RestfulApi::testConnection() {
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(url);
    auto reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << reply->errorString();
            return;
        }
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &error);
        if (error.error != QJsonParseError::NoError) {
            qDebug() << error.errorString();
            return;
        }
        if (document.object()["hello"].toString("err") != "clash") {
            qDebug() << document.toJson();
            return;
        }
        emit connected();
    });
}

inline void checkReply(QNetworkReply *reply) {
    if (reply != nullptr) {
        reply->abort();
        reply->deleteLater();
        reply = nullptr;
    }
}

void checkReply(const QVector<QNetworkReply *> &repliesList) {
    for (auto reply : repliesList) {
        checkReply(reply);
    }
}

void Clash::RestfulApi::updateProxySelector(QString group, QString name, bool update) {
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(QUrl(PROXY_URL + "/" + group.toUtf8()));
    QJsonObject data;
    data["name"] = name;
    auto reply = manager->put(request, QJsonDocument(data).toJson(QJsonDocument::JsonFormat::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, update, group, name] {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Failed to change proxy selector: " << group << name << reply->readAll();
        } else if (update) {
            updateProxy();
        }
        reply->deleteLater();
    });
}

void Clash::RestfulApi::updateConfig() {
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(CONFIG_URL);
    auto reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        emit configUpdate(reply->readAll());
        reply->deleteLater();
    });
}

void Clash::RestfulApi::updateProxy() {
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(QUrl(PROXY_URL));
    auto reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        emit proxyDataReceived(reply->readAll());
        reply->deleteLater();
    });
}

void Clash::RestfulApi::updateConnection() {
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(CONNECTION_URL);
    auto reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        emit connectionDataReceived(reply->readAll());
        reply->deleteLater();
    });
}

void Clash::RestfulApi::listenTraffic() {
    if (connection != nullptr && traffic->isRunning()) {
        return;
    }
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(QUrl(TRAFFIC_URL));

    traffic = manager->get(request);
    connect(traffic, &QNetworkReply::readyRead, this, [this] {
        while (traffic->canReadLine()) {
            QJsonParseError error;
            QByteArray line = traffic->readLine();
            auto document = QJsonDocument::fromJson(line, &error);
            if (error.error == QJsonParseError::NoError) {
                if (document.isObject()) {
                    auto obj = document.object();
                    emit trafficUpdate(obj["up"].toInt(0), obj["down"].toInt(0));
                }
            } else {
                qDebug() << "Failed to parse traffic data:\n    Data:" << line << "\n  Reason:" << error.errorString();
            }
        }
    });
}

void Clash::RestfulApi::stopListenTraffic() { checkReply(traffic); }

void Clash::RestfulApi::listenLog() {
    if (connection != nullptr && log->isRunning()) {
        return;
    }
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(LOG_URL);
    log = manager->get(request);
    connect(log, &QNetworkReply::readyRead, this, [this] {
        while (log->canReadLine()) {
            QJsonParseError error;
            QByteArray line = log->readLine();
            auto document = QJsonDocument::fromJson(line, &error);
            if (error.error == QJsonParseError::NoError) {
                if (document.isObject()) {
                    auto obj = document.object();
                    emit logReceived(obj["type"].toString(), obj["payload"].toString());

                    if (obj["type"].toString() == "error") {
                        emit errorHappened(obj["payload"].toString());
                    }
                }
            } else {
                qDebug() << "Failed to parse log data:\n    Data:" << line << "\n  Reason:" << error.errorString();
            }
        }
    });
}

void Clash::RestfulApi::stopListenLog() { checkReply(log); }

void enableTimer(QTimer *timer, bool enable, int interval_ms) {
    if (enable) {
        timer->setInterval(interval_ms);
        if (!timer->isActive()) {
            timer->start();
        }
    } else {
        timer->stop();
    }
}

void Clash::RestfulApi::autoUpdateProxy(bool enable, int interval_ms) { enableTimer(proxyTimer, enable, interval_ms); }

void Clash::RestfulApi::autoUpdateConnection(bool enable, int interval_ms) { enableTimer(connectionTimer, enable, interval_ms); }

void Clash::RestfulApi::autoUpdateConfig(bool enable, int interval_ms) { enableTimer(configTimer, enable, interval_ms); }

void Clash::RestfulApi::stopTimer() {
    if (proxyTimer->isActive())
        proxyTimer->stop();
    if (connectionTimer->isActive())
        connectionTimer->stop();
    if (configTimer->isActive())
        configTimer->stop();
}

void Clash::RestfulApi::updateProfile(const Profile &profile) {
    QString fullPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/" + profile.file;
    QFile file(fullPath);
    if (!file.exists()) {
        qDebug() << "Profile " + profile.file + "not exist.";
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open " + profile.file;
        return;
    }
    QString payload = file.readAll();
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(CONFIG_URL);

    QJsonObject obj;
    obj["payload"] = payload;

    auto reply = manager->put(request, QJsonDocument(obj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, this, &profile] {
        if (reply->error() != QNetworkReply::NoError) {
            QJsonParseError error{};
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &error);
            if (error.error != QJsonParseError::NoError) {
                emit failedChangeProfile(document.object().value("message").toString("NO MESSAGE."));
            }
        } else {
            emit successChangeProfile();
            for (auto it = profile.selected.constBegin(); it != profile.selected.constEnd(); ++it) {
                updateProxySelector(it.key(), it.value());
            }
            updateProxy();
        }
    });
}
void Clash::RestfulApi::patchConfig(QString key, QVariant value) {
    QNetworkRequest request;
    request.setRawHeader("Content-Type", "application/json");
    request.setUrl(CONFIG_URL);
    QJsonObject obj;
    obj[key] = value.toJsonValue();

    auto reply = manager->sendCustomRequest(request, "PATCH", QJsonDocument(obj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, this] {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << reply->readAll();
            return;
        }
        updateConfig();
    });
}
