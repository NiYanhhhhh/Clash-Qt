#ifndef PROXYPAGE_H
#define PROXYPAGE_H

#include <QWidget>

namespace Ui {
class ProxyPage;
}

class ProxyPage : public QWidget
{
    Q_OBJECT

public:
    explicit ProxyPage(QWidget *parent = nullptr);
    ~ProxyPage();

private:
    Ui::ProxyPage *ui;
};

#endif // PROXYPAGE_H
