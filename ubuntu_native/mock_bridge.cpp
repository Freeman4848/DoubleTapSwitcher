#include "mock_bridge.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QTimer>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); MockBridge bridge;
    auto bus=QDBusConnection::sessionBus();
    if(!bus.registerService("org.gnome.Shell") || !bus.registerObject("/org/freeman4848/DoubleTap",&bridge,QDBusConnection::ExportAllSlots)) return 1;
    QTimer::singleShot(10000,&app,&QCoreApplication::quit);
    return app.exec();
}
