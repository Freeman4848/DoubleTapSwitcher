#pragma once
#include <QObject>
#include <QDBusContext>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class MockBridge : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freeman4848.DoubleTap")
    bool active=false, guard=true, secret=false, debug=false;
    int erased=0;
    QString current="us";
public slots:
    bool TestOnly() const { return true; }
    void SetProtected(bool value) { secret=value; }
    void SetOptions(bool protect, bool diagnostics) { guard=protect; debug=diagnostics; }
    void SetRunning(bool value) { active=value; }
    QString Inspect() const {
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"active",active},{"guard",guard},{"debug",debug},{"erased",erased}}).toJson());
    }
    QString GetState() const {
        QJsonArray sources;
        for(const QString &id : {QString("us"),QString("ru"),QString("ua")})
            sources.append(QJsonObject{{"id",id},{"name",id}});
        return QString::fromUtf8(QJsonDocument(QJsonObject{{"version",3},{"current",current},{"sources",sources}}).toJson());
    }
    QString Switch(const QString &target, bool erase) {
        if(erase && (!active || (guard && secret))) return "blocked:protected-input";
        if(target!="us" && target!="ru" && target!="ua") {
            sendErrorReply("org.freeman4848.InvalidLayout","Unavailable layout"); return {};
        }
        if(erase) ++erased;
        current=target;
        return current;
    }
};
