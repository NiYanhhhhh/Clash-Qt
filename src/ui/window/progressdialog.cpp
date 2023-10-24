#include "ui/window/progressdialog.h"

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Profile Download");
    finished = 0; total = 0;
    QVBoxLayout *layout_main = new QVBoxLayout(this);
    layout_progress = new QVBoxLayout(this);
    label_total_progress = new QLabel(QString(tr("Total: %1/%2")).arg(finished).arg(total), this);
    label_total_progress->setAlignment(Qt::AlignLeft);
    layout_progress->addWidget(label_total_progress);
    layout_main->addLayout(layout_progress);

    QHBoxLayout *layout_button = new QHBoxLayout(this);
    QPushButton *button_ok = new QPushButton(QString(tr("Ok")));
    QPushButton *button_cancel = new QPushButton(QString(tr("Cancel")));
    connect(button_ok, &QPushButton::released, this, [this] { close(); });
    connect(button_cancel, &QPushButton::released, this, [this] {
        if (progress_map.count() > 0) {
            // TODO
        }
    });
    layout_button->addWidget(button_ok);
    layout_button->addWidget(button_cancel);
    layout_main->addLayout(layout_button);

    setLayout(layout_main);
    setMinimumSize(250, 150);
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    connect(this, &ProgressDialog::allFinished, this, &ProgressDialog::onAllFinished);
}
ProgressDialog::~ProgressDialog() {
    delete layout_progress;
    delete label_total_progress;
}

void ProgressDialog::updateProgress(QNetworkReply *reply, qint64 bytesReceived, qint64 bytesTotal) {
    qDebug() << reply->url().toString() << "\t" << QString("%1/%2").arg(bytesReceived).arg(bytesTotal) << " bytes";
    if (!isVisible()) {
        return;
    }

    if (bytesReceived == bytesTotal && bytesTotal == 0) {
        removeProgress(reply);
        finishedIncrease();
        return;
    }
    if (progress_map.contains(reply)) {
        QProgressBar *progress = progress_map.value(reply);
        progress->setValue(bytesReceived);
        if (bytesTotal != -1) {
            progress->setFormat(QString("%1 /%2 bytes").arg(bytesReceived).arg(bytesTotal));
        } else if (bytesReceived != -1) {
            progress->setFormat(QString("%1 bytes").arg(bytesReceived));
        } else {
            progress->setFormat(QString("... bytes"));
        }
    } else {
        createProgress(reply, bytesReceived, bytesTotal);
    }
}

QProgressBar *ProgressDialog::createProgress(QNetworkReply *reply, qint64 bytesReceived, qint64 bytesTotal) {
    QProgressBar *progress_new = new QProgressBar(this);
    QLabel *label_new = new QLabel(this);
    progress_map.insert(reply, progress_new);
    progress_label_map.insert(reply, label_new);

    label_new->setText(QString(tr("Downloading %1 ......")).arg(reply->url().toString()));
    progress_new->setRange(0, bytesTotal);
    progress_new->setValue(bytesReceived);
    progress_new->setFormat(QString("%1/%2 bytes").arg(bytesReceived).arg(bytesTotal));
    layout_progress->addWidget(label_new);
    layout_progress->addWidget(progress_new);
    label_new->show();
    progress_new->show();
    totalIncrease();

    return progress_new;
}

void ProgressDialog::removeProgress(QNetworkReply *reply) {
    delete progress_label_map[reply];
    delete progress_map[reply];
    progress_label_map.remove(reply);
    progress_map.remove(reply);
}

void ProgressDialog::totalIncrease(int v) {
    total += 1;
    label_total_progress->setText(QString(tr("Total: %1/%2")).arg(finished).arg(total));
}

void ProgressDialog::finishedIncrease(int v) {
    finished += 1;
    if (finished > total) {
        qDebug() << "Why there are more finished download progress than total?";
    } else if (finished == total) {
        emit allFinished();
        return;
    }
    label_total_progress->setText(QString(tr("Total: %1/%2")).arg(finished).arg(total));
}

void ProgressDialog::onAllFinished() {
    qDebug() << "All finished!";
    if (progress_map.count() > 0) {
        qDebug() << "Not really all finished!";
        return;
    }
    label_total_progress->setText(QString(tr("Total: %1/%2 all downloaded!")).arg(finished).arg(total));
    setGeometry(geometry().x(), geometry().y(), minimumWidth(), minimumHeight());
    finished= 0, total = 0;
    //close();
}
