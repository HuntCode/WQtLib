#pragma once

#include <QtWebEngineCore/QWebEnginePage>
#include <QtCore/QString>

class YtHookPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit YtHookPage(QObject* parent = nullptr);

signals:
    void screenIdFound(const QString& screenId, const QString& screenIdSecret);
    void loungeTokenFound(const QString& screenId, const QString& loungeToken, qint64 expiration);

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString& message,
                                  int lineNumber,
                                  const QString& sourceID) override;
};
