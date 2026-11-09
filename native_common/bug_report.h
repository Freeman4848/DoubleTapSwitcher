#pragma once
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>

// Only opens a mail draft. Never attaches logs, configuration or typed text.
inline QUrl dtBugReportUrl(const QString& platform) {
    QUrl url(QStringLiteral("mailto:freeman4848.dev@gmail.com"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("subject"),
        QStringLiteral("Double Tap Switcher - Bug report"));
    query.addQueryItem(QStringLiteral("body"),
        QStringLiteral("Platform: ") + platform +
        QStringLiteral("\nVersion:\nDesktop / session (X11 or Wayland):\n\n"
            "What happened:\n\nSteps to reproduce:\n\nExpected result:\n\n"
            "Please do not include passwords or private keyboard input.\n"));
    url.setQuery(query);
    return url;
}
