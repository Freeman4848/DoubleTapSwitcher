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
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QFileSystemWatcher>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

struct Source { QString id, label; unsigned index; };
class Backend {
    bool simulation;
    QString read() const {
        QFile file(path());
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read COSMIC keyboard configuration.");
        return QString::fromUtf8(file.readAll());
    }
    static QString field(const QString &text, const QString &name) {
        const auto match = QRegularExpression("\\b" + name + ":\\s*\"([^\"]*)\"").match(text);
        if (!match.hasMatch()) throw std::runtime_error("Unsupported COSMIC keyboard configuration.");
        return match.captured(1);
    }
public:
    QString description = "COSMIC / native configuration (legacy compositor)";
    explicit Backend(bool test = false): simulation(test) {}
    static QString path() { return QDir::homePath()+"/.config/cosmic/com.system76.CosmicComp/v1/xkb_config"; }
    QList<Source> sources() {
        if (simulation) return {{"us","us",0},{"ru","ru",1},{"ua","ua",2}};
        const auto text = read();
        const auto layouts = field(text,"layout").split(',');
        const auto variants = field(text,"variant").split(',');
        QList<Source> result;
        for (int i=0;i<layouts.size();++i) {
            QString id=layouts[i];
            if (!variants.value(i).isEmpty()) id += "+"+variants[i];
            result.append(Source{id,id,static_cast<unsigned>(i)});
        }
        return result;
    }
    unsigned enabledTriggers(const QList<QString> &targets) {
        const auto list=sources(); unsigned mask=0;
        for(int i=0;i<targets.size();++i) if(targets[i]!=list.first().id) mask|=1u<<i;
        return mask;
    }
    bool activate(const QString &target, bool = false) {
        if(simulation) return false;
        const auto list=sources(); int index=-1;
        for(int i=0;i<list.size();++i) if(list[i].id==target) index=i;
        if(index<0) throw std::runtime_error("Assigned layout is not configured.");
        if(index==0) return false;
        auto text=read(); auto layouts=field(text,"layout").split(',');
        auto variants=field(text,"variant").split(',');
        while(variants.size()<layouts.size()) variants.append(QString());
        layouts.swapItemsAt(0,index); variants.swapItemsAt(0,index);
        for(const auto &name : {QString("layout"),QString("variant")}) {
            const QRegularExpression re("\\b"+name+":\\s*\"[^\"]*\"");
            text.replace(re,name+": \""+(name=="layout"?layouts.join(','):variants.join(','))+"\"");
        }
        QSaveFile file(path());
        const auto bytes=text.toUtf8();
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())
            throw std::runtime_error("COSMIC configuration write failed.");
        return true;
    }
};

class PopWindow : public QMainWindow {
    Q_OBJECT
    ProtectedInputGuard inputGuard;
    Backend backend;
    bool smoke, wanted = false, ready = false;
    unsigned retries = 0;
    QLocalSocket socket;
    QTimer retry;
    QFileSystemWatcher layoutWatcher;
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
    QSettings settings{"Freeman4848", "DoubleTapPopTest"};
public slots:
    void layoutChanged(uint) { updateTriggers(); }
private:
    bool visuallyActive() const { return wanted && ready && !inputBlocked && !terminalBlocked; }
    void updateBadge() {
        QString code="??";
        try {
            const auto id=backend.sources().first().id.section('+',0,0);
            code=id=="us"?"EN":id=="ru"?"RU":id=="ua"?"UA":id.left(2).toUpper();
        } catch(const std::exception &) {}
        layoutTray.setIcon(layoutBadgeIcon(code,visuallyActive()));
        layoutTray.setToolTip("Keyboard layout: "+code);
    }
    void updateTriggers() {
        updateBadge();
        if (socket.state() != QLocalSocket::ConnectedState) return;
        if (!layoutWatcher.files().contains(Backend::path()))
            layoutWatcher.addPath(Backend::path());
        unsigned mask = 0;
        try { mask = backend.enabledTriggers(targets); inputBlocked=protect->isChecked() && !inputGuard.allows(); if(inputBlocked || terminalBlocked) mask=0; }
        catch (const std::exception &e) { log(e.what(), true); }
        const char command = static_cast<char>(0x80u | mask);
        socket.write(&command, 1); socket.flush();
        if(wanted && ready) state(terminalBlocked?"Protected terminal input":inputBlocked?"Protected or unverified input":"Active");
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
        tray.setIcon(icon(visuallyActive() ? "active" : "inactive")); tray.setToolTip("Double Tap Switcher: " + message); updateBadge();
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
            if(!found) throw std::runtime_error("Add the assigned layouts in KDE Keyboard Settings first.");
        }
        return c;
    }
    void readMessages() {
        incoming += socket.readAll();
        while (incoming.size() >= static_cast<int>(sizeof(dt::Message))) {
            dt::Message message{}; std::memcpy(&message, incoming.constData(), sizeof message);
            incoming.remove(0, sizeof message);
            if (message.type == dt::Ready) {
                ready = message.value > 0; state(ready ? "Active" : "Waiting for a physical keyboard"); updateTriggers();
            } else if (message.type == dt::Target && wanted && ready && message.value >= 0 && message.value < targets.size()) {
                if (qEnvironmentVariableIsSet("DT_TRACE"))
                    QTextStream(stderr) << "DT trigger=" << targets[message.value] << Qt::endl;
                try {
                    if(protect->isChecked() && !inputGuard.allows()) { socket.write("R",1); updateTriggers(); continue; }
                    latency.start();
                    const bool changed = backend.activate(targets[message.value], true);
                    if (changed) socket.write("D", 1);
                    updateTriggers(); retries=0; state(terminalBlocked?"Protected terminal input":inputBlocked?"Protected or unverified input":"Active");
                } catch (const std::exception &e) { log(QString::fromUtf8(e.what()), true); socket.write("R", 1); }
            } else if (message.type == dt::Erased) {
                retries = 0; state(terminalBlocked?"Protected terminal input":inputBlocked?"Protected or unverified input":"Active");
                log(QString("Switch + uinput acknowledgment: %1 ms (not editor redraw)").arg(latency.isValid() ? latency.elapsed() : 0));
            } else if (message.type == dt::Paused) { terminalBlocked=message.value!=0; updateTriggers();
            } else if (message.type == dt::Error) {
                const QString reason = message.value == dt::Stale ? "Deletion cancelled: stale/protected input" :
                    message.value == dt::SessionDenied ? "Reader requires an unlocked local graphical session" :
                    "Reader error: check uinput and the native reader systemd journal";
                log(reason, true);
            }
        }
    }
public:
    explicit PopWindow(bool test = false) : backend(test), smoke(test) {
        setWindowTitle("Double Tap Switcher Pop!_OS 1.36 TEST (experimental)"); resize(670, 720);
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
            if (!QDesktopServices::openUrl(dtBugReportUrl(QStringLiteral("Pop!_OS COSMIC"))))
                QMessageBox::information(this,"Report a bug",
                    "No email handler available. Write to freeman4848.dev@gmail.com.");
        });
        connect(about,&QPushButton::clicked,this,[this]{ QMessageBox::about(this,"About Double Tap Switcher",
            "Double Tap Switcher Pop!_OS 1.36 TEST\nFreeman4848\nC++ / Qt 6\nPop!_OS COSMIC / Wayland test adapter\nExperimental, not certified for password fields."); });
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
        inputGuard.changed=[this] { QTimer::singleShot(0,this,[this]{updateTriggers();}); };
        retry.setSingleShot(true); connect(&retry,&QTimer::timeout,this,&PopWindow::start);
        connect(&socket,&QLocalSocket::readyRead,this,&PopWindow::readMessages);
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
            tray.setIcon(icon("inactive")); tray.show(); updateBadge(); layoutTray.show();
            connect(&tray,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason reason){
                if(reason==QSystemTrayIcon::Trigger){ showNormal(); raise(); activateWindow(); }
            });
            QTimer::singleShot(0,this,&PopWindow::start);
        }
        connect(qApp,&QApplication::aboutToQuit,this,&PopWindow::stop);
    }
    ~PopWindow() override { stop(); }
    void start() {
        if (smoke) return;
        wanted = true;
        if (socket.state()==QLocalSocket::ConnectedState) return;
        try {
            refresh(); auto c = config(); incoming.clear(); ready=false; state("Starting reader");
            socket.connectToServer("/run/double-tap-switcher-pop.sock");
            if (!socket.waitForConnected(1000)) { retryLater(); return; }
            if (socket.write(reinterpret_cast<const char *>(&c),sizeof c) != sizeof c)
                throw std::runtime_error("Configuration send failed.");
            if (layoutWatcher.directories().isEmpty()) {
                layoutWatcher.addPath(QFileInfo(Backend::path()).absolutePath());
                connect(&layoutWatcher, &QFileSystemWatcher::directoryChanged, this,
                    [this] { updateTriggers(); });
                connect(&layoutWatcher, &QFileSystemWatcher::fileChanged, this,
                    [this] { updateTriggers(); });
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
    QApplication::setApplicationName("DoubleTapPopTest");
    bool smoke = app.arguments().contains("--smoke-test");
    if (smoke) { PopWindow window(true); window.show(); QTimer::singleShot(150,&app,&QApplication::quit); return app.exec(); }
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) { QTextStream(stderr)<<"No private runtime directory\n"; return 1; }
    QLockFile lock(runtime+"/double-tap-pop-test.lock");
    const QString path=runtime+"/double-tap-pop-test-control";
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
    PopWindow window;
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

#include "main.moc"
