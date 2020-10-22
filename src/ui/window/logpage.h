//
// Created by cyril on 2020/10/20.
//

#ifndef CLASH_QT_LOGPAGE_H
#define CLASH_QT_LOGPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QTableView>

class LogTableModel;

class LogPage : public QWidget{
public:
    LogPage(QWidget *parent = nullptr);

private:
    QPushButton *clearBtn, *stopBtn;
    QTableView *table;
    LogTableModel *model;
};


#endif //CLASH_QT_LOGPAGE_H
