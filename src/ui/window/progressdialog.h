#ifndef PROGRESSMANAGER_H
#define PROGRESSMANAGER_H

#include <QMap>
#include <QDialog>
#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkReply>
#include <QSizePolicy>
#include <QPushButton>

class ProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = nullptr);
    ~ProgressDialog();

    void updateProgress(QNetworkReply *reply, qint64 bytesReceived, qint64 bytesTotal);

    QLabel *label_total_progress;
    QVBoxLayout *layout_progress;

private:
    int total, finished = 0;
    QMap<QNetworkReply*, QProgressBar*> progress_map;
    QMap<QNetworkReply*, QLabel*> progress_label_map;
    QProgressBar *createProgress(QNetworkReply* reply, qint64 bytesReceived, qint64 bytesTotal);
    void removeProgress(QNetworkReply* reply);
    void totalIncrease(int v = 1);
    void finishedIncrease(int v = 1);

signals:
    void allFinished();

public slots:
    void onAllFinished();
};


#endif  // PROGRESSMANAGER_H
