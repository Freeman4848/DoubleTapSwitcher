// Exercise the actual frontend with simulated backend, never the live Shell.
#define main doubleTapApplicationMain
#include "main.cpp"
#undef main
#include <cassert>
#include <QTemporaryDir>

struct WindowTests {
    static void run() {
        Window window(true);
        assert(window.theme->currentData()=="green");
        window.theme->setCurrentIndex(window.theme->findData("dark"));
        assert(window.styleSheet().contains("#0e1012"));
        window.theme->setCurrentIndex(window.theme->findData("light"));
        assert(window.styleSheet().contains("#f1f4f1"));
        window.theme->setCurrentIndex(window.theme->findData("green"));
        assert(window.styleSheet().contains("#10241f"));
        assert(!window.wanted);
        window.notice("first",true); window.notice("second",true);
        assert(window.errorLabel->text()=="second");
        assert(!window.wanted);
        const auto report=bugReportUrl();
        assert(report.scheme()=="mailto" && report.path()=="freeman4848.dev@gmail.com");
        assert(!QUrlQuery(report).queryItemValue("body").contains("second"));
        assert(!window.wanted && !window.isVisible());
        window.control("start");
        assert(window.wanted && window.ready && !window.isVisible());
        window.toggle->click();
        assert(!window.wanted && !window.isVisible());
        window.toggle->click();
        assert(window.wanted && !window.isVisible());
        window.sleepChanged(true);
        assert(window.wanted && !window.ready && window.sleeping);
        assert(!window.retry.isActive());
        window.sleepChanged(false);
        assert(window.retry.isActive() && window.wanted);
        window.retry.stop(); window.start();
        assert(window.ready);
        window.stop();
        Preferences original(true);
        original.setValue("theme","dark"); original.setValue("threshold",250);
        original.setValue("protect",true); original.setValue("diagnostics",true);
        assert(original.save());
        Preferences restored;
        assert(restored.value("theme","")=="dark");
        QFile saved(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)+"/double-tap-switcher/settings.json");
        assert(saved.open(QIODevice::ReadOnly));
        assert(!QJsonDocument::fromJson(saved.readAll()).object()["diagnostics"].toBool());
        window.sleepChanged(true); window.sleepChanged(false);
        assert(!window.wanted && !window.retry.isActive());
        window.control("background");
        assert(!window.wanted && !window.isVisible());
        window.reopen();
        assert(!window.wanted && window.isVisible());
        window.hide(); window.control("start");
        assert(window.wanted && !window.isVisible());
        for (int i=0;i<12;++i) {
            window.retry.stop(); window.retryLater();
            assert(window.wanted && window.retry.isActive());
            assert(window.retry.interval() <= 16000);
        }
        window.stop();
        assert(!window.wanted && !window.retry.isActive());
        window.sleepChanged(true); window.start();
        assert(window.wanted && !window.ready);
        window.sleepChanged(false);
        assert(window.retry.isActive());
        window.stop();
    }
};
int main(int argc, char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir config;
    assert(config.isValid()); qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
    WindowTests::run();
    std::puts("PASS: lifecycle, light/dark themes, theme persistence, diagnostics disabled, mail draft privacy");
}
