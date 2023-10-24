#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include <QNetworkReply>
#include <QDialog>

#include "clash/clash.h"
#include "ui/window/progressdialog.h"

namespace Ui {
class ProfileDialog;
}

class ProfileTableModel;

class ProfileDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProfileDialog(Clash::ProfileList &profiles, QWidget *parent = nullptr);
    ~ProfileDialog() override;

    void downloadProfile();
    void deleteProfile(int index);
    void refreshAllProfile();
    void refreshProfile(Clash::Profile &profile);
    void downloadFinished(QNetworkReply *);
    void refreshFinished(QNetworkReply *, Clash::Profile *profile);

private:
    Clash::Profile &getProfile(int index);
    Ui::ProfileDialog *ui;
    Clash::ProfileList &profiles;
    ProfileTableModel *model;
    QNetworkAccessManager *manager;
    ProgressDialog *progress_dialog;
};

#endif  // PROFILEMANAGER_H
