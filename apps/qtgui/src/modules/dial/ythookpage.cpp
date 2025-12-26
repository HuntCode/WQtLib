#include "YtHookPage.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QDebug>

YtHookPage::YtHookPage(QObject* parent)
    : QWebEnginePage(parent) {}

void YtHookPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                          const QString& message,
                                          int lineNumber,
                                          const QString& sourceID) {
    QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);

    if (!message.startsWith("HHYT|")) return;

    // HHYT|type|json
    const auto parts = message.split('|');
    if (parts.size() < 3) return;

    const QString type = parts[1];
    const QString json = parts.mid(2).join("|");

    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return;
    const auto obj = doc.object();

    if (type == "screenId") {
        const QString screenId = obj.value("screenId").toString();
        const QString secret   = obj.value("screenIdSecret").toString();
        if (!screenId.isEmpty())
            emit screenIdFound(screenId, secret);
        return;
    }

    if (type == "lounge") {
        const QString screenId = obj.value("screenId").toString();
        const QString token    = obj.value("loungeToken").toString();
        const qint64 expiration = static_cast<qint64>(obj.value("expiration").toDouble(0));
        if (!screenId.isEmpty() && !token.isEmpty())
            emit loungeTokenFound(screenId, token, expiration);
        return;
    }
}
