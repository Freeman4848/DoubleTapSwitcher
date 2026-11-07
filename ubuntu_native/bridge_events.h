#pragma once
#include <QObject>
#include <QString>
#include <functional>

class BridgeEvents : public QObject {
    Q_OBJECT
public:
    std::function<void(const QString &)> action;
    std::function<void()> context;
    std::function<void(bool)> sleep;
public slots:
    void onAction(const QString &value) { if (action) action(value); }
    void onContext() { if (context) context(); }
    void onSleep(bool preparing) { if (sleep) sleep(preparing); }

};
