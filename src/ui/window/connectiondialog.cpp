#include "connectiondialog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "clash/clash.h"
#include "ui_connectiondialog.h"
#include "util/instance.h"

class ConnectionModel : public QAbstractTableModel {
public:
    QModelIndex parent(const QModelIndex &child) const override { return QModelIndex(); }

    int rowCount(const QModelIndex &parent) const override { return connections.count(); }

    int columnCount(const QModelIndex &parent) const override {
        return 5;  // start_time, type, source, host, proxy
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Orientation::Horizontal) {
            switch (section) {
                case 0:
                    return tr("Start Time");
                case 1:
                    return tr("Type");
                case 2:
                    return tr("Source");
                case 3:
                    return tr("Destination");
                case 4:
                    return tr("Proxy");
                default:
                    return QVariant();
            }
        }
        return QVariant();
    }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case 0:  // Start Time
                {
                    QString timeStr = connections.at(index.row())["start"].toString("");
                    return QDateTime::fromString(timeStr, Qt::ISODateWithMs).toString("hh:mm:ss");
                }
                case 1:
                    return connections.at(index.row())["metadata"]["network"].toString("");
                case 2:
                    return connections.at(index.row())["metadata"]["sourceIP"].toString("") + ":" +
                           connections.at(index.row())["metadata"]["sourcePort"].toString("");
                case 3:
                    return connections.at(index.row())["metadata"]["host"].toString("") + "(" +
                           connections.at(index.row())["metadata"]["destinationIP"].toString("") + ":" +
                           connections.at(index.row())["metadata"]["destinationPort"].toString("") + ")";
                case 4:
                    return connections.at(index.row())["chains"].toArray().at(0).toString("");
            }
        }
        return QVariant();
    }
    QString getId(int row) {
        if (row < 0 || row >= connections.count()) {
            return QString();
        }
        return connections.at(row)["id"].toString();
    }

    int getRow(QString &id) {
        for (int i = 0; i < connections.count(); ++i) {
            if (id == connections.at(i)["id"].toString()) {
                return i;
            }
        }
        return -1;
    }

    void updateData(QJsonArray data) {
        beginResetModel();
        connections = std::move(data);
        endResetModel();
    }

private:
    QJsonArray connections;
    const static QStringList header;
};

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent), ui(new Ui::ConnectionDialog) {
    ui->setupUi(this);

    model = new ConnectionModel;
    ui->connectionTabel->setModel(model);
    ui->connectionTabel->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    // connect(ui->disconnectAll, &QPushButton::released, this.dis);

    timer = new QTimer(this);
    timer->start(1000);

    connect(timer, &QTimer::timeout, getInstance<Clash>().api(), &Clash::RestfulApi::updateConnection);
    connect(getInstance<Clash>().api(), &Clash::RestfulApi::connectionDataReceived, this, &ConnectionDialog::updateConnection);
}

ConnectionDialog::~ConnectionDialog() {
    delete model;
    delete ui;
}
void ConnectionDialog::updateConnection(QByteArray json) {
    QJsonParseError error{};
    QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error == QJsonParseError::NoError) {
        auto obj = document.object();
        auto connectionArray = obj["connections"];
        if (connectionArray.isArray()) {
            QItemSelection selection = ui->connectionTabel->selectionModel()->selection();
            auto selected = !selection.indexes().empty();
            QString id = selected ? model->getId(selection.indexes().at(0).row()) : "";
            model->updateData(connectionArray.toArray());
            if (selected) {
                int row = model->getRow(id);
                if (row != -1) {
                    ui->connectionTabel->selectRow(row);
                }
            }
        }
    } else {
        qDebug() << "Fail to parse Connection Data [" + error.errorString() + "]: " + json;
    }
}
