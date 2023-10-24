#include "profiledialog.h"
#include <qnetworkreply.h>
#include <QMessageBox>
#include <QNetworkReply>

#include <QAbstractTableModel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <cstddef>

#include "ui/window/progressdialog.h"
#include "ui_profiledialog.h"

class ProfileTableModel : public QAbstractTableModel {
public:
    explicit ProfileTableModel(Clash::ProfileList &profile_list, QObject *parent)
        : QAbstractTableModel(parent), profile_list(profile_list) {}
    QModelIndex parent(const QModelIndex &child) const override { return QModelIndex(); }
    int rowCount(const QModelIndex &parent) const override { return profile_list.getList().size(); }
    int columnCount(const QModelIndex &parent) const override { return 4; }  // name, update, interval, url
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            auto profile = profile_list.getList().at(index.row());
            switch (index.column()) {
                case 0:
                    return profile.name;
                case 1:
                    return profile.updatedTime.toString("yyyy-MM-dd hh:mm");
                case 2:
                    return profile.interval;
                case 3:
                    return profile.url;
                default:
                    return QVariant();
            }
        }
        return QVariant();
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
            switch (section) {
                case 0:
                    return tr("Name");
                case 1:
                    return tr("Last Update");
                case 2:
                    return tr("Interval");
                case 3:
                    return tr("Url");
                default:
                    return QVariant();
            }
        }
        return QVariant();
    }

    void addProfile(const Clash::Profile &profile) {
        beginInsertRows(QModelIndex(), profile_list.getList().size(), profile_list.getList().size());
        profile_list.getList().append(profile);
        if (profile_list.getIndex() < 0) {
            profile_list.setIndex(0);
        }
        endInsertRows();
    }
    void removeProfile(int index) {
        beginRemoveRows(QModelIndex(), index, index);
        if (index == profile_list.getIndex()) {
            profile_list.setIndex(index-1);
        }
        profile_list.remove(index);
        endRemoveRows();
    }
    void refreshProfile() {
    }

private:
    Clash::ProfileList &profile_list;
};

ProfileDialog::ProfileDialog(Clash::ProfileList &profiles, QWidget *parent)
    : QDialog(parent), ui(new Ui::ProfileDialog), profiles(profiles) {
    ui->setupUi(this);

    model = new ProfileTableModel(profiles, this);
    ui->profileTable->setModel(model);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    progress_dialog = new ProgressDialog(this);

    manager = new QNetworkAccessManager(this);
    connect(ui->download, &QPushButton::released, this, [this] {
        if (ui->url->text().isEmpty()) {
            int index = ui->profileTable->currentIndex().row();
            if (index >= 0) {
                refreshProfile(getProfile(index));
            } else {
                QMessageBox::information(NULL, "", "Please enter url or select a profile");
            }
        } else {
            downloadProfile();
        }
    });
    connect(ui->remove, &QPushButton::released, this, [this]() {
        int index = ui->profileTable->currentIndex().row();
        if (index >= 0) {
            deleteProfile(index);
        } else {
            QMessageBox::information(NULL, "", "Please select a profile");
        }
    });
    connect(ui->refresh, &QPushButton::released, this, &ProfileDialog::refreshAllProfile);
}

Clash::Profile &ProfileDialog::getProfile(int index) {
    return profiles.getList()[index];
}

void ProfileDialog::downloadProfile() {
    QUrl url(ui->url->text());
    if (!url.isValid()) {
        QMessageBox::critical(this, tr("Invalid Url"), url.errorString());
    }
    QNetworkReply *reply_download = manager->get(QNetworkRequest(url));
    connect(reply_download, &QNetworkReply::finished, this, [this, reply_download] {
        if (reply_download->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, tr("Download Profile Failed"), reply_download->errorString());
            return;
        }

        downloadFinished(reply_download);
        progress_dialog->updateProgress(reply_download, 0, 0);
    });
    connect(reply_download, &QNetworkReply::errorOccurred, this, [reply_download](QNetworkReply::NetworkError) {
        qDebug() << "Error: " << reply_download->errorString();
    });
    connect(reply_download, &QNetworkReply::downloadProgress, this,
        [this, reply_download](qint64 bytesReceived, qint64 bytesTotal) {
            progress_dialog->updateProgress(reply_download, bytesReceived, bytesTotal);
        });
    progress_dialog->show();
}

void ProfileDialog::deleteProfile(int index) {
    model->removeProfile(index);
}

void ProfileDialog::refreshAllProfile() {
    for (Clash::Profile &profile : profiles.getList()) {
        refreshProfile(profile);
    }
}

void ProfileDialog::refreshProfile(Clash::Profile &profile) {
    qDebug() << "Refresh profile: " << profile.name;
    QUrl url(profile.url);
    qDebug() << "url:" << url.toString();
    if (!url.isValid()) {
        QMessageBox::critical(this, tr("Invalid Url"), url.errorString());
    }
    QNetworkReply *reply_refresh = manager->get(QNetworkRequest(url));
    connect(reply_refresh, &QNetworkReply::finished, this, [this, reply_refresh, &profile] {
        if (reply_refresh->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, tr("Download Profile Failed"), reply_refresh->errorString());
            return;
        }

        Clash::Profile *current_update_profile = &profile;
        refreshFinished(reply_refresh, current_update_profile);
        progress_dialog->updateProgress(reply_refresh, 0, 0);
    });
    connect(reply_refresh, &QNetworkReply::errorOccurred, this, [reply_refresh](QNetworkReply::NetworkError) {
        qDebug() << "Error: " << reply_refresh->errorString();
    });
    connect(reply_refresh, &QNetworkReply::downloadProgress, this,
        [this, reply_refresh](qint64 bytesReceived, qint64 bytesTotal) {
            progress_dialog->updateProgress(reply_refresh, bytesReceived, bytesTotal);
        });
    if (!progress_dialog->isVisible()) {
        progress_dialog->show();
    }
}

void ProfileDialog::downloadFinished(QNetworkReply *reply_download) {
    qDebug() << "Download finish";
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile");
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }

    QString filename = QString::number(QDateTime::currentMSecsSinceEpoch()) + ".yaml";
    QFile file(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/" + filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    qint64 res = file.write(reply_download->readAll());
    if (res > 0) {
        Clash::Profile profile;
        profile.name = reply_download->url().host();
        profile.url = reply_download->url().toString();
        profile.file = filename;
        profile.updatedTime = QDateTime::currentDateTime();
        profile.interval = 0;
        model->addProfile(profile);
    }
    file.close();
    reply_download->deleteLater();
}

void ProfileDialog::refreshFinished(QNetworkReply *reply_refresh, Clash::Profile *current_update_profile) {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile");
    if (!dir.exists()) {
        dir.mkpath(dir.absolutePath());
    }

    QFile file_old(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/" + current_update_profile->file);
    if (file_old.exists()) {
        file_old.remove();
    } else {
        qDebug() << "Failed to delete" << current_update_profile->file << "File not exist";
    }

    QString filename = QString::number(QDateTime::currentMSecsSinceEpoch()) + ".yaml";
    QFile file(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/clash/profile/" + filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    qint64 res = file.write(reply_refresh->readAll());
    if (res > 0) {
        current_update_profile->name = reply_refresh->url().host();
        current_update_profile->file = filename;
        current_update_profile->updatedTime = QDateTime::currentDateTime();
        current_update_profile->interval = 0;
    }
    file.close();
    reply_refresh->deleteLater();
}

ProfileDialog::~ProfileDialog() {
    delete ui;
    delete manager;
    delete model;
    delete progress_dialog;
}
