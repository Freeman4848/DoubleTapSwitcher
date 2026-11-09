#include "../native_common/protected_input.h"
#include "../native_common/tray_presentation.h"
#include "../native_common/bug_report.h"
#include "protocol.h"
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QClipboard>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QLockFile>
#include <QLocalSocket>
#include <QLocalServer>
#include <QTimer>
#include <QSocketNotifier>
#include <QElapsedTimer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QMessageBox>
#include <QDateTime>
#include <QTextStream>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

struct Source { QString id, label; unsigned index; };
class Backend {
    Display *display = nullptr;
    bool kde = false, simulation;
    QString check(const QDBusMessage &reply) const {
        if (reply.type() == QDBusMessage::ErrorMessage) return reply.errorMessage();
        return reply.arguments().isEmpty() ? QStringLiteral("Empty KDE response") : QString();
    }
public:
    QString description;
    std::function<bool()> safeToErase;
    QString currentId() {
        if(simulation)return "us";
        const auto list=sources();XkbStateRec current{};
        if(!display || XkbGetState(display,XkbUseCoreKbd,&current)!=Success)
            throw std::runtime_error("Cannot read active XKB layout.");
        for(const auto &source:list)if(source.index==current.group)return source.id;
        throw std::runtime_error("Unknown XKB group.");
    }
    Display *xdisplay() const { return display; }
    unsigned enabledTriggers(const QList<QString> &targets) {
        const auto list = sources();
        if (kde || !display) throw std::runtime_error("Trigger filtering requires X11.");
        XkbStateRec current{};
        if (XkbGetState(display, XkbUseCoreKbd, &current) != Success)
            throw std::runtime_error("Cannot read layout; triggers disabled.");
        unsigned mask = 0;
        for (int i = 0; i < targets.size(); ++i)
            for (const auto &source : list)
                if (source.id == targets[i] && source.index != current.group) mask |= 1u << i;
        return mask;
    }
    explicit Backend(bool test = false) : simulation(test) {}
    ~Backend() { if (display) XCloseDisplay(display); }
    QList<Source> sources() {
        if (simulation) return {{"us", "English US", 0}, {"ru", "Russian", 1}, {"ua", "Ukrainian", 2}};
        auto registered = QDBusConnection::sessionBus().interface()->isServiceRegistered("org.kde.keyboard");
        kde = registered.isValid() && registered.value();
        QList<Source> result;
        if (kde) {
            description = "KDE / KWin";
            QDBusInterface iface("org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts");
            iface.setTimeout(1000);
            auto reply = iface.call("getLayoutsList");
            auto error = check(reply);
            if (!error.isEmpty()) throw std::runtime_error(error.toStdString());
            const QDBusArgument argument = qvariant_cast<QDBusArgument>(reply.arguments().first());
            argument.beginArray();
            while (!argument.atEnd()) {
                QString id, label, longName;
                argument.beginStructure(); argument >> id >> label >> longName; argument.endStructure();
                unsigned index = static_cast<unsigned>(result.size());
                for (const auto &source : result) if (source.id == id) { id += "_" + QString::number(index); break; }
                result.append({id, longName.isEmpty() ? label : longName, index});
            }
            argument.endArray();
        } else {
            if (qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" ||
                qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("GNOME", Qt::CaseInsensitive))
                throw std::runtime_error("No native adapter for this desktop; keep the existing GNOME version.");
            description = "X11 / XKB (Mint Xfce target)";
            if (!display) display = XOpenDisplay(nullptr);
            if (!display) throw std::runtime_error("X11 unavailable; KDE Wayland needs org.kde.keyboard.");
            Atom property = XInternAtom(display, "_XKB_RULES_NAMES", False), actual;
            int format = 0; unsigned long count = 0, remaining = 0; unsigned char *raw = nullptr;
            if (XGetWindowProperty(display, DefaultRootWindow(display), property, 0, 4096,
                False, XA_STRING, &actual, &format, &count, &remaining, &raw) != Success || !raw)
                throw std::runtime_error("Cannot read the configured XKB layouts.");
            const QByteArray bytes(reinterpret_cast<char *>(raw), static_cast<int>(count));
            XFree(raw);
            const auto fields = bytes.split('\0');
            if (format != 8 || fields.size() < 4) throw std::runtime_error("Invalid XKB rules property.");
            const auto layouts = fields[2].split(','), variants = fields[3].split(',');
            for (int i = 0; i < layouts.size() && i < 4; ++i) {
                QString id = QString::fromUtf8(layouts[i]);
                if (id.isEmpty()) continue;
                if (i < variants.size() && !variants[i].isEmpty()) id += "+" + QString::fromUtf8(variants[i]);
                result.append({id, id, static_cast<unsigned>(i)});
            }
        }
        if (result.isEmpty()) throw std::runtime_error("Add keyboard layouts in the desktop's Keyboard settings first.");
        return result;
    }
    void activate(const QString &target, bool erase = false) {
        const auto list = sources();
        const Source *wanted = nullptr;
        for (const auto &source : list) if (source.id == target) wanted = &source;
        if (!wanted) throw std::runtime_error("Layout is not configured; add US/RU/UA in desktop Keyboard settings.");
        if (simulation) return;
        if (!kde) {
            XkbStateRec current{};
            if (XkbGetState(display, XkbUseCoreKbd, &current) != Success)
                throw std::runtime_error("Cannot read the active layout; deletion cancelled.");
            if (current.group == wanted->index) return;
        }
        if(erase && safeToErase && !safeToErase()) throw std::runtime_error("Protected input; deletion cancelled.");
        if(erase) {
            if(kde) throw std::runtime_error("This test build supports Xfce/X11 only.");
            const KeyCode backspace=XKeysymToKeycode(display,0xff08);
            for(int i=0;i<2;++i) {
                if(!XTestFakeKeyEvent(display,backspace,True,0) || !XTestFakeKeyEvent(display,backspace,False,0))
                    throw std::runtime_error("XTest deletion rejected.");
            }
            XSync(display,False);
        }
        if (kde) {
            QDBusInterface iface("org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts"); iface.setTimeout(1000);
            QDBusReply<bool> accepted = iface.call("setLayout", wanted->index);
            QDBusReply<unsigned> current = iface.call("getLayout");
            if (!accepted.isValid() || !accepted.value() || !current.isValid() || current.value() != wanted->index)
                throw std::runtime_error("KWin did not confirm the layout; deletion cancelled.");
        } else {
            XkbStateRec state{};
            if (!XkbLockGroup(display, XkbUseCoreKbd, wanted->index))
                throw std::runtime_error("XKB switch rejected; deletion cancelled.");
            XSync(display, False);
            if (XkbGetState(display, XkbUseCoreKbd, &state) != Success || state.group != wanted->index)
                throw std::runtime_error("XKB did not confirm the layout; deletion cancelled.");
        }
    }
};

class MintWindow : public QMainWindow {
    ProtectedInputGuard inputGuard;
    Backend backend;
    bool smoke, wanted = false, ready = false;
    unsigned retries = 0;
    QLocalSocket socket;
    QTimer retry;
    QSocketNotifier *layoutEvents = nullptr;
    QByteArray incoming;
    QElapsedTimer latency;
    QSystemTrayIcon tray, layoutTray;
    bool inputBlocked=true, terminalBlocked=false;
    QLabel *status, *adapter;
    QPushButton *toggle;
    QSpinBox *threshold;
    QCheckBox *diagnostics, *protect;
    QPlainTextEdit *console;
    struct Row { QComboBox *key, *layout; };
    QList<Row> rows;
    QList<QString> targets;
    QList<Source> installed;
    QSettings settings{"Freeman4848", "DoubleTapMintTest"};
    bool visuallyActive() const { return wanted && ready && !inputBlocked && !terminalBlocked; }
    void updateBadge() {
        QString code="??";
        try {
            const auto id=backend.currentId().section('+',0,0);
            code=id=="us"?"EN":id=="ru"?"RU":id=="ua"?"UA":id.left(2).toUpper();
        } catch(const std::exception &) {}
        layoutTray.setIcon(layoutBadgeIcon(code,visuallyActive()));
        layoutTray.setToolTip("Keyboard layout: "+code);
    }
    void updateTriggers() {
        updateBadge();
        if (socket.state() != QLocalSocket::ConnectedState) return;
        unsigned mask = 0;
        try { mask = backend.enabledTriggers(targets); inputBlocked=protect->isChecked() && !inputGuard.allows(); if(inputBlocked || terminalBlocked)mask=0; }
        catch (const std::exception &e) { log(e.what(), true); }
        const char command = static_cast<char>(0x80u | mask);
        socket.write(&command, 1); socket.flush();
        if(wanted && ready)state(terminalBlocked?"Protected terminal input":inputBlocked?"Protected or unverified input":"Active");
        if (qEnvironmentVariableIsSet("DT_TRACE"))
            QTextStream(stderr) << "DT enabled_trigger_mask=" << mask << Qt::endl;
    }
    const QList<unsigned> codes{16,17,18,19,20,30,31,32,33,34};
    const QStringList names{"Q","W","E","R","T","A","S","D","F","G"};
    QIcon icon(const QString &name) { return QIcon(QApplication::applicationDirPath() + "/icons/dt-" + name + ".png"); }
    void log(const QString &message, bool error = false) {
        if (error) console->setPlainText(message);
    }
    void state(const QString &message) {
        status->setText(message); toggle->setText(wanted ? "Stop" : "Start");
        tray.setIcon(icon(visuallyActive() ? "active" : "inactive")); tray.setToolTip("Double Tap Switcher: " + message);updateBadge();
        if(qEnvironmentVariableIsSet("DT_TRACE")) QTextStream(stderr)<<"DT visual active="<<visuallyActive()<<" reason="<<message<<Qt::endl;
    }
    void retryLater() {
        if (!wanted || retry.isActive()) return;

        int delay = 1000 << qMin(retries,4u); if(retries<4) ++retries;
        state(QString("Waiting for reader; retry in %1s").arg(delay / 1000)); retry.start(delay);
    }
    void refresh() {
        QStringList selected;
        for (const auto &row : rows) selected.append(row.layout->currentData().toString());
        installed = backend.sources(); adapter->setText(backend.description);
        for (int i = 0; i < rows.size(); ++i) {
            auto combo = rows[i].layout; combo->clear();
            for (const auto &source : installed) combo->addItem(source.label, source.id);
            int index = combo->findData(selected.value(i));
            if(index<0 && !selected[i].isEmpty()) {
                combo->addItem("Unavailable: "+selected[i],selected[i]); index=combo->count()-1;
            }
            if (index >= 0) combo->setCurrentIndex(index);
        }
    }
    dt::Config config() {
        dt::Config c; c.threshold = static_cast<unsigned>(threshold->value());
        c.count = 0; c.keys.fill(0); c.protect = protect->isChecked(); targets.clear();
        for (const auto &row : rows) {
            unsigned code = row.key->currentData().toUInt(); if (!code) continue;
            c.keys[c.count++] = code; targets.append(row.layout->currentData().toString());
        }
        if (!dt::valid(c)) throw std::runtime_error("Choose 1-5 distinct hotkeys and a valid interval.");
        for (const auto &target : targets) {
            bool found=false; for(const auto &source : installed) found|=source.id==target;
            if(!found) throw std::runtime_error("Add the assigned layouts in Mint Keyboard Settings first.");
        }
        return c;
    }
    void readMessages() {
        incoming += socket.readAll();
        while (incoming.size() >= static_cast<int>(sizeof(dt::Message))) {
            dt::Message message{}; std::memcpy(&message, incoming.constData(), sizeof message);
            incoming.remove(0, sizeof message);
            if (message.type == dt::Ready) {
                ready = message.value > 0; state(ready ? "Active" : "Waiting for a physical keyboard");updateTriggers();
            } else if (message.type == dt::Target && wanted && ready && message.value >= 0 && message.value < targets.size()) {
                if (qEnvironmentVariableIsSet("DT_TRACE"))
                    QTextStream(stderr) << "DT trigger=" << targets[message.value] << Qt::endl;
                try {
                    if(protect->isChecked() && (!inputGuard.allows() || terminalBlocked)){socket.write("R",1);updateTriggers();continue;}
                    latency.start(); backend.activate(targets[message.value], true); updateTriggers(); retries=0; state(terminalBlocked?"Protected terminal input":inputBlocked?"Protected or unverified input":"Active");
                } catch (const std::exception &e) { log(QString::fromUtf8(e.what()), true); socket.write("R", 1); }
            } else if (message.type == dt::Erased) {
                retries = 0; state(terminalBlocked?"Protected terminal input":inputBlocked?"Protected or unverified input":"Active");
                log(QString("Switch + uinput acknowledgment: %1 ms (not editor redraw)").arg(latency.isValid() ? latency.elapsed() : 0));
            } else if (message.type == dt::Paused) {terminalBlocked=message.value!=0;updateTriggers();
            } else if (message.type == dt::Error) {
                const QString reason = message.value == dt::Stale ? "Deletion cancelled: stale/protected input" :
                    message.value == dt::SessionDenied ? "Reader requires an unlocked local graphical session" :
                    "Reader error: check uinput and the native reader systemd journal";
                log(reason, true);
            }
        }
    }
public:
    explicit MintWindow(bool test = false) : backend(test), smoke(test) {
        setWindowTitle("Double Tap Switcher Mint 1.38 TEST (experimental)"); resize(670, 720);
        setWindowIcon(icon("app")); auto body = new QWidget(this); setCentralWidget(body);
        auto layout = new QVBoxLayout(body); auto header = new QHBoxLayout;
        status = new QLabel("Stopped"); toggle = new QPushButton("Start");
        auto restart = new QPushButton("Restart Hook");
        header->addWidget(status, 1); header->addWidget(toggle); header->addWidget(restart); layout->addLayout(header);
        adapter = new QLabel("Backend not connected"); layout->addWidget(adapter);
        auto warning = new QLabel("Password guard uses field metadata only; unverified inputs are blocked.");
        warning->setWordWrap(true); layout->addWidget(warning);
        auto speed = new QHBoxLayout; speed->addWidget(new QLabel("Double-tap interval (ms)"));
        threshold = new QSpinBox; threshold->setRange(100,1000); threshold->setValue(settings.value("threshold",250).toInt());
        speed->addWidget(threshold); speed->addStretch(); layout->addLayout(speed);
        diagnostics = new QCheckBox("Diagnostics (off by default; no typed text)");
        diagnostics->setChecked(false); diagnostics->hide();
        protect = new QCheckBox("Protect password fields and hidden terminal input");
        protect->setChecked(settings.value("protect",true).toBool()); layout->addWidget(protect);
        auto grid = new QGridLayout; const QStringList defaultKeys{"Q","W","F","",""};
        const QStringList defaultLayouts{"us","ru","ua","",""};
        for (int i = 0; i < 5; ++i) {
            auto key = new QComboBox; key->addItem("Disabled", 0u);
            for (int k = 0; k < names.size(); ++k) key->addItem(names[k], codes[k]);
            int selected = key->findText(settings.value(QString("key%1").arg(i),defaultKeys[i]).toString());
            key->setCurrentIndex(selected >= 0 ? selected : 0);
            auto combo = new QComboBox;
            QString target = settings.value(QString("layout%1").arg(i),defaultLayouts[i]).toString();
            combo->addItem(target, target);
            rows.append(Row{key, combo});
            grid->addWidget(new QLabel(QString("Hotkey %1").arg(i+1)),i,0); grid->addWidget(key,i,1);
            grid->addWidget(new QLabel("Layout"),i,2); grid->addWidget(combo,i,3);
        }
        layout->addLayout(grid); auto actions = new QHBoxLayout;
        auto save = new QPushButton("Save Settings"), refreshButton = new QPushButton("Refresh Layouts");
        actions->addWidget(save); actions->addWidget(refreshButton); layout->addLayout(actions);
 layout->addWidget(new QLabel("Errors / optional diagnostics"));
        console = new QPlainTextEdit; console->setReadOnly(true); console->setMaximumBlockCount(2000); layout->addWidget(console,1);
        auto footer = new QHBoxLayout; auto copy = new QPushButton("Copy All"), clear = new QPushButton("Clear");
        auto about = new QPushButton("About"), exit = new QPushButton("Exit");
        auto report = new QPushButton("Report a bug");
        for (auto button : {copy, clear, about, report, exit}) footer->addWidget(button);
        layout->addLayout(footer);
        connect(copy,&QPushButton::clicked,this,[this]{ QApplication::clipboard()->setText(console->toPlainText()); });
        connect(clear,&QPushButton::clicked,console,&QPlainTextEdit::clear);
        connect(exit,&QPushButton::clicked,qApp,&QApplication::quit);
        connect(report,&QPushButton::clicked,this,[this]{
            if (!QDesktopServices::openUrl(dtBugReportUrl(QStringLiteral("Mint Xfce"))))
                QMessageBox::information(this,"Report a bug",
                    "No email handler available. Write to freeman4848.dev@gmail.com.");
        });
        connect(about,&QPushButton::clicked,this,[this]{ QMessageBox::about(this,"About Double Tap Switcher",
            "Double Tap Switcher Mint 1.38 TEST\nFreeman4848\nC++ / Qt 6\nMint Xfce / X11 test adapter\nExperimental, not certified for password fields."); });
        connect(toggle,&QPushButton::clicked,this,[this]{ if (wanted) stop(); else start(); });
        connect(restart,&QPushButton::clicked,this,[this]{ stop(); start(); });
        connect(refreshButton,&QPushButton::clicked,this,[this]{ try { refresh(); } catch(const std::exception &e){ log(e.what(),true); } });
        connect(save,&QPushButton::clicked,this,[this] {
            try {
                config(); settings.setValue("threshold",threshold->value()); settings.setValue("protect",protect->isChecked());
                settings.setValue("diagnostics",diagnostics->isChecked());
                for (int i=0;i<rows.size();++i) {
                    settings.setValue(QString("key%1").arg(i), rows[i].key->currentIndex() ? rows[i].key->currentText() : "");
                    settings.setValue(QString("layout%1").arg(i), rows[i].layout->currentData());
                }
                settings.sync(); if (settings.status()!=QSettings::NoError) throw std::runtime_error("Settings write failed.");
                if (wanted) { stop(); start(); }
            } catch(const std::exception &e) { log(e.what(),true); }
        });
        backend.safeToErase=[this]{return !protect->isChecked() || (!terminalBlocked && inputGuard.allows());};
        inputGuard.changed=[this]{QTimer::singleShot(0,this,[this]{updateTriggers();});};
        retry.setSingleShot(true); connect(&retry,&QTimer::timeout,this,&MintWindow::start);
        connect(&socket,&QLocalSocket::readyRead,this,&MintWindow::readMessages);
        connect(&socket,&QLocalSocket::disconnected,this,[this]{ ready=false; if(wanted) retryLater(); });
        connect(&socket,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError){
            if(wanted) { ready=false; log("Reader unavailable: " + socket.errorString(),true); retryLater(); }
        });
        if (!smoke) {
            auto menu = new QMenu(this);
            menu->addAction("Start / Stop",this,[this]{ if(wanted) stop(); else start(); });
            menu->addAction("Settings...",this,[this]{ showNormal(); raise(); activateWindow(); });
            menu->addAction("Restart Hook",this,[this]{ stop(); start(); });
            menu->addSeparator(); menu->addAction("About",about,&QPushButton::click);
            menu->addAction("Report a bug",report,&QPushButton::click);
            menu->addAction("Exit",qApp,&QApplication::quit); tray.setContextMenu(menu);
            tray.setIcon(icon("inactive")); tray.show();updateBadge();layoutTray.show();
            connect(&tray,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason reason){
                if(reason==QSystemTrayIcon::Trigger){ showNormal(); raise(); activateWindow(); }
            });
            QTimer::singleShot(0,this,&MintWindow::start);
        }
        connect(qApp,&QApplication::aboutToQuit,this,&MintWindow::stop);
    }
    ~MintWindow() override { stop(); }
    void start() {
        if (smoke) return;
        wanted = true;
        if (socket.state()==QLocalSocket::ConnectedState) return;
        try {
            refresh(); auto c = config(); incoming.clear(); ready=false; state("Starting reader");
            socket.connectToServer("/run/double-tap-switcher-mint.sock");
            if (!socket.waitForConnected(1000)) { retryLater(); return; }
            if (socket.write(reinterpret_cast<const char *>(&c),sizeof c) != sizeof c)
                throw std::runtime_error("Configuration send failed.");
            if (!layoutEvents) {
                auto display = backend.xdisplay();
                if (!display || !XkbSelectEvents(display, XkbUseCoreKbd,
                    XkbStateNotifyMask | XkbMapNotifyMask | XkbNewKeyboardNotifyMask,
                    XkbStateNotifyMask | XkbMapNotifyMask | XkbNewKeyboardNotifyMask))
                    throw std::runtime_error("Cannot observe XKB layout changes.");
                XFlush(display);
                layoutEvents = new QSocketNotifier(ConnectionNumber(display), QSocketNotifier::Read, this);
                connect(layoutEvents, &QSocketNotifier::activated, this, [this, display] {
                    while (XPending(display)) { XEvent event{}; XNextEvent(display, &event); }
                    updateTriggers();
                });
            }
            updateTriggers();
        } catch(const std::exception &e) { log(e.what(),true); stop(); }
    }
    void stop() { wanted=false; ready=false; retries=0; retry.stop(); socket.abort(); incoming.clear(); state("Stopped"); }
    void reopen() { showNormal(); raise(); activateWindow(); }
protected:
    void closeEvent(QCloseEvent *event) override {
        if(!smoke && QSystemTrayIcon::isSystemTrayAvailable()) { hide(); event->ignore(); }
        else { stop(); event->accept(); }
    }
};

int main(int argc, char **argv) {
    QApplication app(argc,argv); QApplication::setOrganizationName("Freeman4848");
    QApplication::setApplicationName("DoubleTapMintTest");
    bool smoke = app.arguments().contains("--smoke-test");
    if (smoke) { MintWindow window(true); window.show(); QTimer::singleShot(150,&app,&QApplication::quit); return app.exec(); }
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) { QTextStream(stderr)<<"No private runtime directory\n"; return 1; }
    QLockFile lock(runtime+"/double-tap-mint-test.lock");
    const QString path=runtime+"/double-tap-mint-test-control";
    if(!lock.tryLock(0)) {
        QLocalSocket notify; notify.connectToServer(path);
        if(notify.waitForConnected(1000)){
            notify.write(app.arguments().contains("--quit") ? "quit" : app.arguments().contains("--stop") ? "stop" : app.arguments().contains("--start") ? "start" : "show");
            notify.waitForBytesWritten(1000);
        }
        return 0;
    }
    QLocalServer server; QLocalServer::removeServer(path); server.setSocketOptions(QLocalServer::UserAccessOption);
    if(!server.listen(path)) return 1;
    if(app.arguments().contains("--quit") || app.arguments().contains("--stop")) return 0;
    MintWindow window;
    QObject::connect(&server,&QLocalServer::newConnection,&window,[&]{
        while(auto peer=server.nextPendingConnection()) {
            QObject::connect(peer,&QLocalSocket::readyRead,&window,[&,peer]{
                const auto command=peer->readAll();
                if(command=="quit") app.quit();
                else if(command=="stop") window.stop();
                else if(command=="start") window.start();
                else window.reopen();
                peer->disconnectFromServer();peer->deleteLater();
            });
            if(peer->bytesAvailable()) QMetaObject::invokeMethod(peer,"readyRead",Qt::QueuedConnection);
        }
    });
    window.show(); return app.exec();
}
