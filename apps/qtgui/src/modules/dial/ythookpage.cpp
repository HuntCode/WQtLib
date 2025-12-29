#include "YtHookPage.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

// 注意：这里用 Console 做桥梁（不需要 QWebChannel）。
// JS 侧会打印：HHYT|type|json

static bool extractVideoAndListFromUrl(QString urlOrFragment, QString* videoId, QString* listId)
{
    if (!videoId || !listId) return false;

    QString v;
    QString l;

    // 1) 先按完整 URL 解析 query
    {
        QUrl u(urlOrFragment);
        QUrlQuery q(u);
        v = q.queryItemValue("v");
        l = q.queryItemValue("list");
    }

    // 2) 再解析 fragment（YouTube TV 常见： https://www.youtube.com/tv#/watch?v=...&list=... ）
    if (v.isEmpty()) {
        QUrl u(urlOrFragment);
        QString frag = u.fragment(); // "/watch?v=...&list=..."
        if (!frag.isEmpty()) {
            int pos = frag.indexOf('?');
            QString qs = (pos >= 0) ? frag.mid(pos + 1) : frag;
            QUrlQuery fq(qs);
            if (v.isEmpty()) v = fq.queryItemValue("v");
            if (l.isEmpty()) l = fq.queryItemValue("list");
        }
    }

    // 3) 兜底：纯字符串正则（防 sourceID 不是标准 URL）
    if (v.isEmpty()) {
        static const QRegularExpression reV(R"((?:[?#&]|^|\/)v=([A-Za-z0-9_-]{6,}))");
        auto m = reV.match(urlOrFragment);
        if (m.hasMatch()) v = m.captured(1);
    }
    if (l.isEmpty()) {
        static const QRegularExpression reL(R"((?:[?#&]|^|\/)list=([A-Za-z0-9_-]{6,}))");
        auto m = reL.match(urlOrFragment);
        if (m.hasMatch()) l = m.captured(1);
    }

    if (v.isEmpty()) return false;

    *videoId = v;
    *listId = l;
    return true;
}

YtHookPage::YtHookPage(QObject* parent) : QWebEnginePage(parent) {}

void YtHookPage::updateMediaMeta(const QString& videoId, const QString& listId)
{
    if (videoId.isEmpty()) return;

    if (m_videoId == videoId && m_listId == listId) return;

    m_videoId = videoId;
    m_listId = listId;
    emit mediaMetaFound(m_videoId, m_listId);
}

void YtHookPage::updatePlayback(double currentTime, double playbackRate, const QString& playerState)
{
    // 小幅变化也会频繁触发，这里做个轻量去重
    const bool stateChanged = (m_playerState != playerState);
    const bool timeChanged = (qAbs(m_currentTimeSec - currentTime) >= 0.5);
    const bool rateChanged = (qAbs(m_playbackRate - playbackRate) >= 0.01);

    if (!stateChanged && !timeChanged && !rateChanged) return;

    m_currentTimeSec = currentTime;
    m_playbackRate = playbackRate;
    m_playerState = playerState;
    emit playbackFound(m_currentTimeSec, m_playbackRate, m_playerState);
}

void YtHookPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                          const QString& message,
                                          int lineNumber,
                                          const QString& sourceID)
{
    Q_UNUSED(level);

    // 你想看的“打印一下 javaScriptConsoleMessage 内容”
    // （noquote 避免日志里把 \\n 变成一坨转义）
    qInfo().noquote() << QString("[JS] sourceID=[ %1 ] : lineNumber=%2 [ %3 ]")
                             .arg(sourceID)
                             .arg(lineNumber)
                             .arg(message);

    // 兜底：有时候你看到的 URL 就在 sourceID 里（比如 CORS 报错那条）
    {
        QString v, l;
        if (extractVideoAndListFromUrl(sourceID, &v, &l)) {
            updateMediaMeta(v, l);
        }
    }

    if (!message.startsWith("HHYT|")) return;

    const auto parts = message.split('|', Qt::KeepEmptyParts);
    if (parts.size() < 3) return;

    const QString type = parts[1];
    const QByteArray jsonBytes = parts[2].toUtf8();

    const auto doc = QJsonDocument::fromJson(jsonBytes);
    const QJsonObject o = doc.isObject() ? doc.object() : QJsonObject();

    if (type == "hookReady") {
        emit hookReady(o.value("ok").toBool());
        return;
    }

    if (type == "screenId") {
        m_screenId = o.value("screenId").toString();
        m_screenIdSecret = o.value("screenIdSecret").toString();
        emit screenIdFound(m_screenId, m_screenIdSecret);
        return;
    }

    if (type == "lounge") {
        const QString sid = o.value("screenId").toString();
        const QString token = o.value("loungeToken").toString();
        const qint64 exp = static_cast<qint64>(o.value("expiration").toDouble());
        emit loungeTokenFound(sid, token, exp);
        return;
    }

    if (type == "mediaMeta") {
        const QString vid = o.value("videoId").toString();
        const QString lid = o.value("listId").toString();
        updateMediaMeta(vid, lid);
        return;
    }

    if (type == "playback") {
        const double t = o.value("currentTime").toDouble();
        const double r = o.value("playbackRate").toDouble(1.0);
        const QString st = o.value("playerState").toString();
        updatePlayback(t, r, st);
        return;
    }

    if (type == "drmProbe") {
        const bool ok = o.value("ok").toBool(false);
        const QString keySystem = o.value("keySystem").toString();
        const QString err = o.value("error").toString();
        const QString reason = o.value("reason").toString();

        qInfo() << "[YT] DRM probe:"
                << (ok ? "SUPPORTED" : "NOT supported")
                << "keySystem=" << keySystem
                << (err.isEmpty() ? reason : err);

        // 你也可以 emit 一个信号给 DialWidget 做 UI 展示
        // emit drmProbeResult(ok, keySystem, err.isEmpty()?reason:err);
    }
}
