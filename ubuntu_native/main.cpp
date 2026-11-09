#include "protocol.h"
#include "bridge_events.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QMenu>
#include <QCloseEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QLockFile>
#include <QLocalSocket>
#include <QLocalServer>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QMessageBox>
#include <QDateTime>
#include <QTextStream>
#include <QFrame>
#include <QFontDatabase>
#include <QScreen>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrlQuery>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

static QUrl bugReportUrl() {
    QUrl url("mailto:freeman4848.dev@gmail.com");
    QUrlQuery query;
    query.addQueryItem("subject", "Double Tap Switcher 1.39 - Bug report");
    query.addQueryItem("body", "Double Tap Switcher 1.39 / Ubuntu GNOME 50\n\n"
        "What happened:\n\nSteps to reproduce:\n\nExpected result:\n");
    url.setQuery(query);
    return url;
}

struct Source { QString id, label; unsigned index; };
class Backend {
    bool simulation;
    QDBusMessage request(const QString &method, const QVariantList &arguments = {}) {
        QDBusInterface iface("org.gnome.Shell", "/org/freeman4848/DoubleTap", "org.freeman4848.DoubleTap");
        iface.setTimeout(1000);
        auto reply = iface.callWithArgumentList(QDBus::Block, method, arguments);
        if (reply.type() == QDBusMessage::ErrorMessage)
            throw std::runtime_error(("GNOME bridge: " + reply.errorMessage() + "; log out/in after an extension update").toStdString());
        return reply;
    }
public:
    QString description = "Ubuntu / GNOME Shell";
    explicit Backend(bool test = false) : simulation(test) {}
    QList<Source> sources() {
        if (simulation) return {{"us", "English US", 0}, {"ru", "Russian", 1}, {"ua", "Ukrainian", 2}};
        QDBusMessage reply;
        try { reply = request("GetState"); }
        catch (const std::exception &) {
            QDBusInterface extensions("org.gnome.Shell.Extensions", "/org/gnome/Shell/Extensions", "org.gnome.Shell.Extensions");
            extensions.setTimeout(1500);
            QDBusReply<bool> enabled = extensions.call("EnableExtension", QString("double-tap@freeman4848.local"));
            if (!enabled.isValid() || !enabled.value()) throw std::runtime_error("GNOME extension unavailable; log out/in after installation.");
            reply = request("GetState");
        }
        if (reply.arguments().isEmpty()) throw std::runtime_error("Empty GNOME state.");
        auto document = QJsonDocument::fromJson(reply.arguments().first().toString().toUtf8());
        auto object = document.object();
        if (object.value("version").toInt() != 3) throw std::runtime_error("Old GNOME extension loaded; log out/in after installation.");
        QList<Source> result;
        for (const auto &item : object.value("sources").toArray()) {
            auto source = item.toObject();
            if (!source.value("id").toString().isEmpty())
                result.append({source.value("id").toString(), source.value("name").toString(), static_cast<unsigned>(result.size())});
        }
        if (result.isEmpty()) throw std::runtime_error("No layouts: add them in Ubuntu Settings > Keyboard.");
        return result;
    }
    void options(bool protect, bool diagnostics) { if (!simulation) request("SetOptions", {protect, diagnostics}); }
    void running(bool active) { if (!simulation) request("SetRunning", {active}); }
    bool activate(const QString &target, bool erase = false) {
        if (simulation) return true;
        const auto list = sources();
        bool found = false;
        for (const auto &source : list) found |= source.id == target;
        if (!found) throw std::runtime_error("Layout is not configured in Ubuntu Keyboard settings.");
        auto reply = request("Switch", {target, erase});
        if (reply.arguments().isEmpty()) throw std::runtime_error("Empty switch response.");
        auto actual = reply.arguments().first().toString();
        if (actual.startsWith("blocked:")) return false;
        if (actual != target) throw std::runtime_error("GNOME did not confirm the requested layout.");
        return true;
    }
};

class Preferences {
    QJsonObject values;
    QString path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/double-tap-switcher/settings.json";
public:
    explicit Preferences(bool test = false) {
        if (test) return;
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            auto data = QJsonDocument::fromJson(file.readAll()).object();
            values["threshold"] = data.value("threshold_ms");
            values["protect"] = data.contains("protect_input") ? data.value("protect_input") : QJsonValue(true);
            values["theme"] = data.value("theme");
            auto bindings = data.value("bindings").toArray();
            for (int i=0;i<bindings.size() && i<5;++i) {
                values[QString("key%1").arg(i)] = bindings[i].toObject().value("key");
                values[QString("layout%1").arg(i)] = bindings[i].toObject().value("layout");
            }
        }
    }
    QVariant value(const QString &key, const QVariant &fallback) const {
        return values.contains(key) && !values[key].isNull() ? values[key].toVariant() : fallback;
    }
    void setValue(const QString &key, const QVariant &value) { values[key] = QJsonValue::fromVariant(value); }
    bool save() {
        QJsonObject data{{"threshold_ms", values["threshold"]}, {"diagnostics", false}, {"theme", values["theme"]}, {"protect_input", values["protect"]}};
        QJsonArray bindings;
        for (int i=0;i<5;++i) {
            auto key=values.value(QString("key%1").arg(i)).toString();
            if (!key.isEmpty()) bindings.append(QJsonObject{{"key",key},{"layout",values.value(QString("layout%1").arg(i))}});
        }
        data["bindings"]=bindings;
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) return false;
        if (file.write(QJsonDocument(data).toJson()) < 0) return false;
        return file.commit();
    }
};

class Window : public QMainWindow {
    friend struct WindowTests;
    Backend backend;
    bool smoke, wanted = false, ready = false, sleeping = false;
    unsigned retries = 0;
    QLocalSocket socket;
    QTimer retry, health;
    QByteArray incoming;
    BridgeEvents events;
    QLabel *status, *adapter;
    QPushButton *toggle;
    QSpinBox *threshold;
    QCheckBox *protect;
    QComboBox *theme;
    QLabel *errorLabel;
    struct Row { QComboBox *key, *layout; };
    QList<Row> rows;
    QList<QString> targets;
    QList<Source> installed;
    Preferences settings;
    const QList<unsigned> codes{16,17,18,19,20,30,31,32,33,34};
    const QStringList names{"Q","W","E","R","T","A","S","D","F","G"};
    QIcon icon(const QString &name) { return QIcon(QApplication::applicationDirPath() + "/icons/dt-" + name + ".png"); }
    void notice(const QString &message, bool error = false) {
        if (!error) return;
        errorLabel->setText(message);
        errorLabel->show();
    }
    void applyTheme() {
        const QString selected = theme->currentData().toString();
        QString sheet = QString::fromLatin1(R"(
            QWidget#canvas { background: #f1f4f1; color: #18322d; }
            QLabel { color: #29463e; }
            QLabel#title { font-size: 25px; font-weight: 700; color: #12382d; }
            QLabel#subtitle { font-size: 10px; font-weight: 700; color: #52766a; letter-spacing: 2px; }
            QLabel#badge { background: #dcebe3; color: #226248; border-radius: 8px; padding: 8px; font-size: 10px; }
            QLabel#status { font-size: 16px; font-weight: 600; }
            QLabel#hint { color: #668176; font-size: 11px; }
            QLabel#section { color: #52766a; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
            QFrame#card { background: #ffffff; border: 1px solid #dbe5df; border-radius: 14px; }
            QPushButton { color: #234b3c; background: #ffffff; border: 1px solid #ccdcd2; border-radius: 8px; padding: 8px 14px; }
            QPushButton:hover { background: #e2eee6; border-color: #83b59a; }
            QPushButton:pressed { background: #c8dfd1; }
            QPushButton#primary { background: #17684a; border-color: #17684a; color: #ffffff; font-weight: 600; }
            QPushButton#primary:hover { background: #21845d; }
            QComboBox, QSpinBox { color: #193e30; background: #ffffff; border: 1px solid #c8d9ce; border-radius: 6px; padding: 6px; min-height: 18px; }
            QComboBox QAbstractItemView { background: #ffffff; color: #193e30; selection-background-color: #cce8d7; selection-color: #193e30; }
            QCheckBox { color: #29463e; spacing: 8px; }
        )");
        if(selected!="light") {
            const QList<QPair<QString,QString>> colors{
                {"#f1f4f1","#10241f"},{"#18322d","#e0f0e7"},{"#29463e","#c9dfd3"},
                {"#12382d","#edfaf2"},{"#52766a","#96c4af"},{"#dcebe3","#214b3b"},
                {"#226248","#b5ebcd"},{"#668176","#a2bcaf"},{"#ffffff","#1c382d"},
                {"#dbe5df","#36594a"},{"#234b3c","#dcf1e4"},{"#ccdcd2","#4b705e"},
                {"#e2eee6","#2c5140"},{"#83b59a","#75b894"},{"#c8dfd1","#38634e"},
                {"#193e30","#dff2e5"},{"#c8d9ce","#4b705e"},{"#cce8d7","#345f48"}};
            for(const auto &color : colors) sheet.replace(color.first,color.second);
        }
        if(selected=="dark") {
            const QList<QPair<QString,QString>> neutral{
                {"#10241f","#0e1012"},{"#1c382d","#191c20"},{"#214b3b","#252a30"},
                {"#36594a","#3a4048"},{"#4b705e","#444c56"},{"#2c5140","#292f36"},
                {"#38634e","#333b44"},{"#345f48","#343e49"},{"#96c4af","#aeb8c5"},
                {"#a2bcaf","#9faab7"},{"#c9dfd3","#d4d9e0"},{"#dcf1e4","#e4e9f0"},
                {"#dff2e5","#e4e9f0"},{"#e0f0e7","#e4e9f0"},{"#edfaf2","#f1f3f6"},
                {"#b5ebcd","#cad4e1"}};
            for(const auto &color : neutral) sheet.replace(color.first,color.second);
        }
        // Primary action keeps the brand accent in all three themes.
        sheet += "QPushButton#primary { color: #ffffff; } QLabel#error { color: " +
            QString(selected!="light" ? "#ffb6a5" : "#a33026") + "; }";
        setStyleSheet(sheet);
    }
    void state(const QString &message) {
        status->setText(message); toggle->setText(wanted ? "Stop" : "Start");

    }
    void retryLater() {
        if (!wanted || sleeping || retry.isActive()) return;
        int delay = 1000 << qMin(retries, 4u);
        if (retries < 4) ++retries;
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
            if(index < 0 && !selected.value(i).isEmpty()) {
                combo->addItem("Unavailable: " + selected[i], selected[i]);
                index = combo->count()-1;
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
            bool found=false;
            for(const auto &source : installed) found |= source.id==target;
            if (!found) throw std::runtime_error("A hotkey points to an unavailable layout; add it in Ubuntu Keyboard settings or disable that hotkey.");
        }
        return c;
    }
    void readMessages() {
        incoming += socket.readAll();
        while (incoming.size() >= static_cast<int>(sizeof(dt::Message))) {
            dt::Message message{}; std::memcpy(&message, incoming.constData(), sizeof message);
            incoming.remove(0, sizeof message);
            if (message.type == dt::Ready) {
                ready = message.value > 0;
                try { backend.running(true); } catch(const std::exception &e) { notice(e.what(),true); socket.abort(); retryLater(); continue; }
                errorLabel->clear(); errorLabel->hide();
                state(ready ? "Active" : "Waiting for a physical keyboard");
            } else if (message.type == dt::Target && wanted && ready && message.value >= 0 && message.value < targets.size()) {
                try {
                    bool changed = backend.activate(targets[message.value], true);
                    state(changed ? "Active" : "Paused: protected input / unknown context");
                    if(changed) retries=0;
                } catch (const std::exception &e) { notice(QString::fromUtf8(e.what()), true); socket.write("R", 1); }

            } else if (message.type == dt::Paused) {
                state(message.value ? "Paused: protected terminal input" : "Active");
            } else if (message.type == dt::Error) {
                const QString reason = message.value == dt::Stale ? "Deletion cancelled: stale/protected input" :
                    message.value == dt::SessionDenied ? "Reader requires an unlocked local graphical session" :
                    "Keyboard reader unavailable; restart the hook or report a bug";
                notice(reason, true);
            }
        }
    }
public:
    explicit Window(bool test = false) : backend(test), smoke(test), settings(test) {
        setWindowTitle("Double Tap Switcher 1.39"); resize(740, 740);
        setWindowIcon(icon("app"));
        auto scroll = new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        auto body = new QWidget; scroll->setWidget(body); setCentralWidget(scroll);
        body->setObjectName("canvas");
        auto layout = new QVBoxLayout(body); layout->setContentsMargins(28,20,28,20); layout->setSpacing(9);
        auto brand = new QHBoxLayout;
        auto logo = new QLabel; logo->setPixmap(icon("app").pixmap(60,60));
        brand->addWidget(logo); auto titles = new QVBoxLayout;
        auto title = new QLabel("Double Tap Switcher"); title->setObjectName("title");
        auto subtitle = new QLabel("YOUR KEYS. YOUR LANGUAGES."); subtitle->setObjectName("subtitle");
        titles->addWidget(title); titles->addWidget(subtitle); brand->addLayout(titles); brand->addStretch();
        auto badge = new QLabel("UBUNTU  /  1.39"); badge->setObjectName("badge"); brand->addWidget(badge);
        layout->addLayout(brand);
        auto header = new QHBoxLayout;
        status = new QLabel("Stopped"); toggle = new QPushButton("Start");
        auto restart = new QPushButton("Restart Hook");
        header->addWidget(status, 1); header->addWidget(toggle); header->addWidget(restart); layout->addLayout(header);
        status->setObjectName("status"); toggle->setObjectName("primary");
        adapter = new QLabel("Ubuntu / GNOME Shell"); layout->addWidget(adapter);
        auto warning = new QLabel("Automatic at login. Recovers after sleep. Super + Space stays yours.");
        warning->setWordWrap(true); layout->addWidget(warning);
        auto speed = new QHBoxLayout; speed->addWidget(new QLabel("DOUBLE-TAP WINDOW"));
        threshold = new QSpinBox; threshold->setRange(100,1000); threshold->setValue(settings.value("threshold",250).toInt());
        threshold->setSuffix(" ms"); speed->addWidget(threshold); speed->addStretch(); layout->addLayout(speed);
        auto intervalHint = new QLabel("Time between taps, not a delay before switching.");
        intervalHint->setObjectName("hint"); layout->addWidget(intervalHint);
        auto appearance = new QHBoxLayout;
        appearance->addWidget(new QLabel("APPEARANCE"));
        theme = new QComboBox;
        theme->addItem("Green", "green"); theme->addItem("Dark", "dark"); theme->addItem("Light", "light");
        theme->setCurrentIndex(qMax(0,theme->findData(settings.value("theme","green"))));
        appearance->addWidget(theme); appearance->addStretch(); layout->addLayout(appearance);
        protect = new QCheckBox("Protect password fields and hidden terminal prompts (best effort)");
        protect->setChecked(settings.value("protect",true).toBool()); layout->addWidget(protect);
        auto mappingTitle = new QLabel("HOTKEYS & LAYOUTS"); mappingTitle->setObjectName("section"); layout->addWidget(mappingTitle);
        auto card = new QFrame; card->setObjectName("card"); auto grid = new QGridLayout(card);
        grid->setContentsMargins(16,12,16,12); grid->setVerticalSpacing(5); grid->setColumnStretch(3,1);
        const QStringList defaultKeys{"Q","W","F","",""};
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
        layout->addWidget(card); auto actions = new QHBoxLayout;
        auto save = new QPushButton("Save Settings"), refreshButton = new QPushButton("Refresh Layouts");
        save->setObjectName("primary");
        actions->addWidget(save); actions->addWidget(refreshButton); layout->addLayout(actions);

        errorLabel = new QLabel; errorLabel->setObjectName("error"); errorLabel->setWordWrap(true);
        errorLabel->setTextFormat(Qt::PlainText);
        errorLabel->hide(); layout->addWidget(errorLabel);
        auto privacy = new QLabel("No activity logs or keystrokes saved to disk. Bug reports are sent only by you.");
        privacy->setObjectName("hint"); privacy->setWordWrap(true); layout->addWidget(privacy);
        layout->addStretch();
        auto footer = new QHBoxLayout;
        auto report = new QPushButton("Report a bug");
        auto about = new QPushButton("About"), exit = new QPushButton("Exit");
        for (auto button : {report, about, exit}) footer->addWidget(button);
        layout->addLayout(footer);
        connect(report,&QPushButton::clicked,this,[]{ QDesktopServices::openUrl(bugReportUrl()); });
        applyTheme();
        connect(theme,&QComboBox::currentIndexChanged,this,[this] { applyTheme(); });
        connect(exit,&QPushButton::clicked,qApp,&QApplication::quit);
        connect(about,&QPushButton::clicked,this,&Window::showAbout);
        connect(toggle,&QPushButton::clicked,this,[this]{ if (wanted) stop(); else start(); });
        connect(restart,&QPushButton::clicked,this,[this]{ stop(); start(); });
        connect(refreshButton,&QPushButton::clicked,this,[this]{ try { refresh(); } catch(const std::exception &e){ notice(e.what(),true); } });
        connect(save,&QPushButton::clicked,this,[this] {
            try {
                config(); settings.setValue("threshold",threshold->value()); settings.setValue("protect",protect->isChecked());
                settings.setValue("theme",theme->currentData());
                for (int i=0;i<rows.size();++i) {
                    settings.setValue(QString("key%1").arg(i), rows[i].key->currentIndex() ? rows[i].key->currentText() : "");
                    settings.setValue(QString("layout%1").arg(i), rows[i].layout->currentData());
                }
                if (!settings.save()) throw std::runtime_error("Settings write failed.");
                if (wanted) { stop(); start(); }
            } catch(const std::exception &e) { notice(e.what(),true); }
        });
        retry.setSingleShot(true); connect(&retry,&QTimer::timeout,this,&Window::start);
        health.setInterval(5000);
        connect(&health,&QTimer::timeout,this,[this] {
            if (!wanted || sleeping || retry.isActive()) return;
            if (socket.state()!=QLocalSocket::ConnectedState) { start(); return; }
            try { backend.running(true); }
            catch(const std::exception &e) { notice(e.what(),true); socket.abort(); retryLater(); }
        });
        connect(&socket,&QLocalSocket::readyRead,this,&Window::readMessages);
        connect(&socket,&QLocalSocket::disconnected,this,[this]{ ready=false; if(wanted) retryLater(); });
        connect(&socket,&QLocalSocket::errorOccurred,this,[this](QLocalSocket::LocalSocketError){
            if(wanted) { ready=false; notice("Reader unavailable: " + socket.errorString(),true); retryLater(); }
        });
        if (!smoke) {
            auto bus = QDBusConnection::sessionBus();
            events.action = [this](const QString &action) {
                if (action=="toggle") { if(wanted) stop(); else start(); }
                else if(action=="restart") { stop(); start(); }
                else if(action=="exit") QApplication::quit();
            };
            events.context = [this] { if(socket.state()==QLocalSocket::ConnectedState) socket.write("R",1); };
            events.sleep = [this](bool preparing) { sleepChanged(preparing); };
            QDBusConnection::systemBus().connect("org.freedesktop.login1", "/org/freedesktop/login1",
                "org.freedesktop.login1.Manager", "PrepareForSleep", &events, SLOT(onSleep(bool)));
            bus.connect("org.gnome.Shell", "/org/freeman4848/DoubleTap", "org.freeman4848.DoubleTap", "Action", &events, SLOT(onAction(QString)));
            bus.connect("org.gnome.Shell", "/org/freeman4848/DoubleTap", "org.freeman4848.DoubleTap", "ContextChanged", &events, SLOT(onContext()));
            QTimer::singleShot(0,this,&Window::start);
            health.start();
        }
        connect(qApp,&QApplication::aboutToQuit,this,&Window::stop);
    }
    ~Window() override { stop(); }
    void setPreviewTheme(const QString &name) { theme->setCurrentIndex(theme->findData(name)); }
    void showAbout() {
        QMessageBox::about(this,"About Double Tap Switcher",
            "Double Tap Switcher 1.39\nFreeman4848\nC++ / Qt 6 + GNOME JavaScript\nOnly Ubuntu GNOME 50 is supported.\nProtected-field detection is best effort.");
    }
    void sleepChanged(bool preparing) {
        sleeping=preparing;
        if (preparing) {
            retry.stop(); ready=false; socket.abort(); incoming.clear();
            state(wanted ? "Sleeping; automatic recovery enabled" : "Stopped");
        } else if (wanted) {
            retries=0; notice("Resumed: reconnecting keyboard reader");
            retry.start(1000);
        }
    }
    void start() {
        wanted = true;
        if (sleeping) return;
        if (smoke) { ready=true; state("Active"); return; }
        if (socket.state()==QLocalSocket::ConnectedState) return;
        try {
            refresh(); auto c = config(); backend.options(protect->isChecked(),false); incoming.clear(); ready=false; state("Starting reader");
            socket.connectToServer("/run/double-tap-switcher.sock");
            if (!socket.waitForConnected(1000)) { retryLater(); return; }
            if (socket.write(reinterpret_cast<const char *>(&c),sizeof c) != sizeof c)
                throw std::runtime_error("Configuration send failed.");
        } catch(const std::exception &e) { notice(e.what(),true); socket.abort(); retryLater(); }
    }
    void stop() {
        bool wasRunning = wanted || ready;
        wanted=false; ready=false; retries=0; retry.stop(); socket.abort(); incoming.clear();
        if(wasRunning && !smoke) { try { backend.running(false); } catch(const std::exception &e) { notice(e.what(),true); } }
        state("Stopped");
    }
    void reopen() { showNormal(); raise(); activateWindow(); }
    void control(const QByteArray &command) {
        if(command=="quit") QApplication::quit();
        else if(command=="about") showAbout();
        else if(command=="start") start();
        else if(command=="background") { /* Do not undo an existing manual Stop. */ }
        else reopen();
    }
protected:
    void closeEvent(QCloseEvent *event) override {
        if(!smoke) { hide(); event->ignore(); }
        else { stop(); event->accept(); }
    }
};

int main(int argc, char **argv) {
    QApplication app(argc,argv); QApplication::setOrganizationName("Freeman4848");
    QApplication::setApplicationName("DoubleTapSwitcher");
    QApplication::setQuitOnLastWindowClosed(false);
    if (QFontDatabase::families().contains("Lato")) app.setFont(QFont("Lato",10));
    if(app.arguments().contains("--report-bug")) return QDesktopServices::openUrl(bugReportUrl()) ? 0 : 1;
    bool smoke = app.arguments().contains("--smoke-test");
    if (smoke) {
        Window window(true); window.start();
        if(app.arguments().contains("--dark-preview")) window.setPreviewTheme("dark");
        if(app.arguments().contains("--light-preview")) window.setPreviewTheme("light");
        window.show();
        QTimer::singleShot(200,&app,[&] {
            if (app.arguments().contains("--preview")) window.grab().save(app.arguments().contains("--dark-preview") ? "/tmp/dt-130-dark.png" : app.arguments().contains("--light-preview") ? "/tmp/dt-130-light.png" : "/tmp/dt-130-green.png");
            app.quit();
        }); return app.exec();
    }
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) return 1;
    QLockFile lock(runtime+"/double-tap-switcher-cpp.lock");
    const QString path=runtime+"/double-tap-switcher-cpp-control";
    if(!lock.tryLock(0)) {
        QLocalSocket notify; notify.connectToServer(path);
        if(notify.waitForConnected(1000)){
            notify.write(app.arguments().contains("--quit") ? "quit" : app.arguments().contains("--about") ? "about" :
                app.arguments().contains("--start") ? "start" : app.arguments().contains("--background") ? "background" : "show");
            notify.waitForBytesWritten(1000);
        }
        return 0;
    }
    QLocalServer server; QLocalServer::removeServer(path); server.setSocketOptions(QLocalServer::UserAccessOption);
    if(!server.listen(path)) return 1;
    if(app.arguments().contains("--quit")) return 0;
    if(!QDBusConnection::sessionBus().registerService("org.freeman4848.DoubleTapApp")) {
        return 1;
    }
    Window window;
    QObject::connect(&server,&QLocalServer::newConnection,&window,[&]{
        while(auto peer=server.nextPendingConnection()) {
            QObject::connect(peer,&QLocalSocket::readyRead,&window,[&,peer]{
                window.control(peer->readAll());
                peer->disconnectFromServer(); peer->deleteLater();
            });
            if(peer->bytesAvailable()) QMetaObject::invokeMethod(peer,"readyRead",Qt::QueuedConnection);
        }
    });
    if (!app.arguments().contains("--start") && !app.arguments().contains("--background")) window.show();
    if(app.arguments().contains("--about")) QTimer::singleShot(0,&window,&Window::showAbout);
    return app.exec();
}
