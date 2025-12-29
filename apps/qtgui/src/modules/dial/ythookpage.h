#pragma once
#include <QWebEnginePage>

class YtHookPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit YtHookPage(QObject* parent = nullptr);

    // 最近一次捕获到的数据（给外部读取用）
    QString currentScreenId() const { return m_screenId; }
    QString currentScreenIdSecret() const { return m_screenIdSecret; }
    QString currentVideoId() const { return m_videoId; }
    QString currentListId() const { return m_listId; }
    double  currentTimeSec() const { return m_currentTimeSec; }
    double  playbackRate() const { return m_playbackRate; }
    QString playerState() const { return m_playerState; }

signals:
    void hookReady(bool ok);

    void screenIdFound(const QString& screenId, const QString& screenIdSecret);
    void loungeTokenFound(const QString& screenId, const QString& loungeToken, qint64 expiration);

    void mediaMetaFound(const QString& videoId, const QString& listId);
    void playbackFound(double currentTime, double playbackRate, const QString& playerState);

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString& message,
                                  int lineNumber,
                                  const QString& sourceID) override;

private:
    void updateMediaMeta(const QString& videoId, const QString& listId);
    void updatePlayback(double currentTime, double playbackRate, const QString& playerState);

private:
    QString m_screenId;
    QString m_screenIdSecret;

    QString m_videoId;
    QString m_listId;

    double  m_currentTimeSec = 0.0;
    double  m_playbackRate = 1.0;
    QString m_playerState; // PLAYING/PAUSED/BUFFERING/IDLE
};
