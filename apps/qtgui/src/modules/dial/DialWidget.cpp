#include "modules/dial/DialWidget.h"
#include "ui_DialWidget.h"

#include "wqt_dial_server.h"
#include "YtHookPage.h"

#include <QPushButton>
#include <QTextEdit>
#include <QScrollBar>
#include <QDateTime>
#include <QVBoxLayout>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEnginePage>

#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineSettings>

static const char* kYtHookJs = R"JS(
(() => {
  if (window.__hh_yt_hooked) return;
  window.__hh_yt_hooked = true;

  const PATTERNS = [
    /\/api\/lounge\/pairing\/generate_screen_id/i,
    /\/api\/lounge\/pairing\/get_lounge_token_batch/i,
    /\/api\/lounge\/bc\/bind/i,
  ];

  function match(url) {
    url = String(url || "");
    return PATTERNS.some(re => re.test(url));
  }

  function emit(type, payload) {
    try { console.log("HHYT|" + type + "|" + JSON.stringify(payload || {})); } catch (_) {}
  }

  // -----------------------------
  // Meta (videoId/listId/index/time/duration)
  // -----------------------------
  let lastMeta = { videoId: "", listId: "", currentIndex: -1, currentTime: NaN, duration: NaN };

  function maybeEmitMeta(patch) {
    if (!patch || !patch.videoId) return;

    const merged = {
      videoId: (patch.videoId != null ? patch.videoId : lastMeta.videoId),
      listId: ((patch.listId != null ? patch.listId : lastMeta.listId) || ""),
      currentIndex: (patch.currentIndex != null ? patch.currentIndex : lastMeta.currentIndex),
      currentTime: (patch.currentTime != null ? patch.currentTime : lastMeta.currentTime),
      duration: (patch.duration != null ? patch.duration : lastMeta.duration),
    };

    const changed =
      merged.videoId !== lastMeta.videoId ||
      merged.listId !== lastMeta.listId ||
      merged.currentIndex !== lastMeta.currentIndex ||
      (Number.isFinite(merged.duration) && merged.duration !== lastMeta.duration);

    if (!changed) return;
    lastMeta = merged;
    emit("mediaMeta", merged);
  }

  // -----------------------------
  // URL/hash meta extractor (#/watch?v=...&list=...&index=...)
  // -----------------------------
  function parseTvHashMeta() {
    try {
      const h = String(location.hash || "");
      const qpos = h.indexOf("?");
      const query = (qpos >= 0) ? h.slice(qpos + 1) : "";
      if (!query) return null;

      const sp = new URLSearchParams(query);
      const videoId = sp.get("v") || sp.get("video_id") || sp.get("contentId") || "";
      const listId  = sp.get("list") || sp.get("listId") || "";
      const idxStr  = sp.get("index");

      let currentIndex = -1;
      if (idxStr != null && idxStr !== "") {
        const n = parseInt(idxStr, 10);
        if (!Number.isNaN(n)) currentIndex = (n > 0) ? (n - 1) : n; // best-effort
      }

      if (!videoId) return null;
      return { videoId, listId, currentIndex };
    } catch (_) {
      return null;
    }
  }

  function pollUrlMeta() {
    const m = parseTvHashMeta();
    if (m) maybeEmitMeta(m);
  }

  (function hookHistory() {
    const _push = history.pushState;
    const _rep  = history.replaceState;

    history.pushState = function(...args) {
      const r = _push.apply(this, args);
      setTimeout(pollUrlMeta, 0);
      return r;
    };
    history.replaceState = function(...args) {
      const r = _rep.apply(this, args);
      setTimeout(pollUrlMeta, 0);
      return r;
    };

    window.addEventListener("hashchange", () => setTimeout(pollUrlMeta, 0), true);
    window.addEventListener("popstate",  () => setTimeout(pollUrlMeta, 0), true);
  })();

  // -----------------------------
  // Extract meta from lounge bind response (often NOT JSON)
  // -----------------------------
  function extractMetaFromText(text) {
    text = String(text || "");

    const reVid  = /["']?(?:videoId|video_id|contentId)["']?\s*[:=]\s*["']([A-Za-z0-9_-]{6,})["']/;
    const reList = /["']?(?:listId|list_id)["']?\s*[:=]\s*["']([A-Za-z0-9_-]+)["']/;
    const reIdx  = /["']?(?:currentIndex|current_index)["']?\s*[:=]\s*(\d+)/;
    const reTime = /["']?(?:currentTime|current_time)["']?\s*[:=]\s*([0-9.]+)/;

    const mVid = text.match(reVid);
    if (!mVid) return;

    const videoId = mVid[1];
    const mList = text.match(reList);
    const listId = mList ? mList[1] : "";

    const mIdx = text.match(reIdx);
    const currentIndex = mIdx ? parseInt(mIdx[1], 10) : -1;

    const mTime = text.match(reTime);
    const currentTime = mTime ? parseFloat(mTime[1]) : NaN;

    maybeEmitMeta({ videoId, listId, currentIndex, currentTime });
  }

  function tryParseJson(text) { try { return JSON.parse(text); } catch (_) { return null; } }

  function tryParse(url, text) {
    if (!text) return;
    url = String(url || "");

    if (/\/api\/lounge\/bc\/bind/i.test(url)) {
      extractMetaFromText(text);
      return;
    }

    const obj = tryParseJson(text);
    if (!obj) return;

    if (obj.screenId) {
      emit("screenId", { screenId: obj.screenId, screenIdSecret: obj.screenIdSecret || "" });
    }
    if (obj.screens && obj.screens.length) {
      const s = obj.screens[0];
      if (s && s.screenId) {
        emit("lounge", { screenId: s.screenId, loungeToken: s.loungeToken || "", expiration: s.expiration || 0 });
      }
    }
  }

  // Hook fetch
  const _fetch = window.fetch;
  if (_fetch) {
    window.fetch = async function (...args) {
      const res = await _fetch.apply(this, args);
      try {
        const url = (args[0] && args[0].url) ? args[0].url : args[0];
        if (match(url)) res.clone().text().then(t => tryParse(url, t));
      } catch (_) {}
      return res;
    };
  }

  // Hook XHR
  const XHR = XMLHttpRequest.prototype;
  const _open = XHR.open;
  const _send = XHR.send;

  XHR.open = function (method, url, ...rest) {
    this.__hh_url = url;
    return _open.call(this, method, url, ...rest);
  };

  XHR.send = function (body) {
    this.addEventListener("load", function () {
      const url = this.__hh_url || "";
      if (match(url)) tryParse(url, this.responseText);
    });
    return _send.call(this, body);
  };

  // -----------------------------
  // <video> debug: src changes + error + encrypted
  // -----------------------------
  (function hookMediaSrcSetter() {
    try {
      const desc = Object.getOwnPropertyDescriptor(HTMLMediaElement.prototype, "src");
      if (desc && typeof desc.set === "function") {
        Object.defineProperty(HTMLMediaElement.prototype, "src", {
          get: desc.get,
          set: function(v) {
            try { emit("srcSet", { tag: this.tagName, src: String(v || "") }); } catch(_) {}
            return desc.set.call(this, v);
          },
          configurable: true,
          enumerable: desc.enumerable
        });
        emit("srcHook", { ok:true });
      } else {
        emit("srcHook", { ok:false, reason:"no src setter descriptor" });
      }
    } catch (e) {
      emit("srcHook", { ok:false, error: String(e) });
    }

    // also catch setAttribute('src', ...)
    try {
      const _setAttr = Element.prototype.setAttribute;
      Element.prototype.setAttribute = function(name, value) {
        try {
          if ((this.tagName === "VIDEO" || this.tagName === "AUDIO") && String(name).toLowerCase() === "src") {
            emit("srcAttr", { tag: this.tagName, src: String(value || "") });
          }
        } catch(_) {}
        return _setAttr.call(this, name, value);
      };
      emit("attrHook", { ok:true });
    } catch (e) {
      emit("attrHook", { ok:false, error:String(e) });
    }
  })();

  // -----------------------------
  // MSE hook: isTypeSupported + addSourceBuffer + appendBuffer (with mime mapping)
  // -----------------------------
  (function hookMSE() {
    if (!window.MediaSource) {
      emit("mse", { ok:false, reason:"MediaSource missing" });
      return;
    }

    // 1) isTypeSupported probe (dedupe)
    const seenITS = new Set();
    if (typeof MediaSource.isTypeSupported === "function") {
      const _its = MediaSource.isTypeSupported.bind(MediaSource);
      MediaSource.isTypeSupported = function(mime) {
        const s = String(mime || "");
        const ok = _its(s);
        const key = s + "|" + ok;
        if (!seenITS.has(key)) {
          seenITS.add(key);
          emit("mseITS", { name:"MediaSource", mime:s, ok });
        }
        return ok;
      };
    }

    // 2) addSourceBuffer hook (log real mime used by player)
    const sbMime = new WeakMap();
    const _add = MediaSource.prototype.addSourceBuffer;
    if (typeof _add === "function") {
      MediaSource.prototype.addSourceBuffer = function(mime) {
        const s = String(mime || "");
        emit("mseAdd", { mime:s });

        try {
          const sb = _add.call(this, mime);
          try { sbMime.set(sb, s); } catch (_) {}
          emit("mseAddOK", { mime:s });
          return sb;
        } catch (e) {
          emit("mseAddFail", { mime:s, error: String(e && (e.name || e.message) ? (e.name + ": " + e.message) : e) });
          throw e;
        }
      };
    }

    // 3) appendBuffer hook (catch runtime failures)
    if (window.SourceBuffer && SourceBuffer.prototype && typeof SourceBuffer.prototype.appendBuffer === "function") {
      const _append = SourceBuffer.prototype.appendBuffer;
      SourceBuffer.prototype.appendBuffer = function(buf) {
        try {
          return _append.call(this, buf);
        } catch (e) {
          const mime = sbMime.get(this) || "";
          emit("sbAppendFail", {
            mime,
            error: String(e && (e.name || e.message) ? (e.name + ": " + e.message) : e),
          });
          throw e;
        }
      };
    }

    emit("mse", { ok:true });
  })();


  // -----------------------------
  // <video> playback + error polling
  // -----------------------------
  let hookedVideo = null;
  let lastPlaybackTs = 0;
  let lastErrCode = -999;

  function snapshotVideo(v, reason) {
    const now = Date.now();
    if (now - lastPlaybackTs < 300) return;
    lastPlaybackTs = now;

    if (!v) {
      emit("playback", { reason, ok:false, playerState:"NO_VIDEO" });
      return;
    }

    let playerState = "IDLE";
    let ytState = 0;

    if (v.ended) { playerState = "ENDED"; ytState = 4; }
    else if (v.seeking || (v.readyState < 3 && !v.paused)) { playerState = "BUFFERING"; ytState = 3; }
    else if (v.paused) { playerState = "PAUSED"; ytState = 2; }
    else { playerState = "PLAYING"; ytState = 1; }

    const dur = (typeof v.duration === "number" && Number.isFinite(v.duration) && v.duration > 0) ? v.duration : NaN;

    emit("playback", {
      reason,
      ok: true,
      currentTime: (typeof v.currentTime === "number") ? v.currentTime : 0,
      playbackRate: (typeof v.playbackRate === "number") ? v.playbackRate : 1,
      ytState,
      playerState,
      duration: dur,
      readyState: v.readyState,
      networkState: v.networkState,
      currentSrc: v.currentSrc || "",
    });

    if (Number.isFinite(dur) && lastMeta.videoId) {
      maybeEmitMeta({ videoId: lastMeta.videoId, listId: lastMeta.listId, currentIndex: lastMeta.currentIndex, duration: dur });
    }
  }

  function hookVideo(v) {
    if (!v || hookedVideo === v) return;
    hookedVideo = v;
    lastErrCode = -999;

    const events = [
      "loadstart","loadedmetadata","durationchange","canplay","playing","pause",
      "waiting","stalled","seeking","seeked","timeupdate","ended","ratechange"
    ];
    events.forEach(ev => v.addEventListener(ev, () => snapshotVideo(v, ev), { passive: true }));

    v.addEventListener("error", () => {
      const err = v.error;
      emit("videoError", {
        from: "event",
        code: err ? err.code : 0,
        message: (err && err.message) ? err.message : "",
        currentSrc: v.currentSrc || "",
        readyState: v.readyState,
        networkState: v.networkState,
      });
    }, false);

    v.addEventListener("encrypted", (e) => {
      try {
        emit("encrypted", {
          initDataType: e && e.initDataType ? e.initDataType : "",
          initDataBytes: (e && e.initData && e.initData.byteLength) ? e.initData.byteLength : 0
        });
      } catch(_) {}
    }, true);

    emit("videoHooked", { ok:true });
    snapshotVideo(v, "hook");
  }

  function ensureVideoHook() {
    const v = document.querySelector("video");
    if (v) hookVideo(v);
  }

  // EME / Widevine probe (keep it simple)
  async function probeWidevineOnce() {
    if (window.__hh_wv_probed) return;
    window.__hh_wv_probed = true;

    if (!navigator.requestMediaKeySystemAccess) {
      emit("drmProbe", { ok:false, reason:"EME API missing" });
      return;
    }

    const cfg = [{
      initDataTypes: ["cenc"],
      audioCapabilities: [{ contentType: 'audio/mp4; codecs="mp4a.40.2"', robustness: "SW_SECURE_DECODE" }],
      videoCapabilities: [{ contentType: 'video/mp4; codecs="avc1.42E01E"', robustness: "SW_SECURE_DECODE" }],
    }];

    try {
      const access = await navigator.requestMediaKeySystemAccess("com.widevine.alpha", cfg);
      emit("drmProbe", { ok:true, keySystem: access.keySystem });
    } catch (e) {
      emit("drmProbe", { ok:false, error: String(e && e.message ? e.message : e) });
    }
  }

  // global error hooks
  window.addEventListener("error", (ev) => {
    emit("pageError", {
      message: String(ev && ev.message ? ev.message : ""),
      filename: String(ev && ev.filename ? ev.filename : ""),
      lineno: ev && ev.lineno ? ev.lineno : 0,
      colno: ev && ev.colno ? ev.colno : 0,
    });
  }, true);

  window.addEventListener("unhandledrejection", (ev) => {
    emit("promiseRejection", { reason: String(ev && ev.reason ? ev.reason : "") });
  }, true);

  // tick
  setInterval(() => {
    try {
      ensureVideoHook();
      pollUrlMeta();

      if (hookedVideo) {
        // error polling (sometimes event isn't fired)
        const err = hookedVideo.error;
        const code = err ? err.code : 0;
        if (code && code !== lastErrCode) {
          lastErrCode = code;
          emit("videoError", {
            from: "poll",
            code,
            message: (err && err.message) ? err.message : "",
            currentSrc: hookedVideo.currentSrc || "",
            readyState: hookedVideo.readyState,
            networkState: hookedVideo.networkState,
          });
        }

        snapshotVideo(hookedVideo, "tick");
      }
    } catch (_) {}
  }, 800);

  setTimeout(pollUrlMeta, 0);
  setTimeout(() => { try { probeWidevineOnce(); } catch(_) {} }, 0);

  emit("hookReady", { ok:true });
})();
)JS";


namespace {
// UI-thread cache for quick verification.
static QString g_lastVideoId;
static QString g_lastListId;
static int g_lastIndex = -1;
static QString g_lastPlayerState;
static int g_lastYtState = -999;
static double g_lastTime = -1.0;
static QElapsedTimer g_playbackLogTimer;
static bool g_playbackLogTimerStarted = false;
}

static QString nowTag()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}

DialWidget::DialWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DialWidget)
{
    ui->setupUi(this);

    connect(ui->btnStartDial, &QPushButton::clicked, this, &DialWidget::onStartClicked);
    connect(ui->btnStopDial,  &QPushButton::clicked, this, &DialWidget::onStopClicked);
    connect(ui->btnClearLog,  &QPushButton::clicked, this, &DialWidget::onClearLogClicked);

    ui->textLog->setReadOnly(true);

    ensureWebView();

    // 创建 DIAL server（QObject，挂到 DialWidget 下面自动析构）
    m_server = new WQtDialServer(this);

    // Qt 信号：已经在 wqt_dial_server.cpp 里做了 QueuedConnection 投递到 Qt 线程
    connect(m_server, &WQtDialServer::youtubeStart, this, &DialWidget::onYoutubeStart);
    connect(m_server, &WQtDialServer::youtubeStop,  this, &DialWidget::onYoutubeStop);
    connect(m_server, &WQtDialServer::youtubeHide,  this, &DialWidget::onYoutubeHide);

    // 状态回调：必须同步/快速/线程安全（别直接碰 UI）
    m_server->setYoutubeStatusCallback([this](uint32_t sessionId, bool& canStop) -> bool {
        canStop = true;
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        return (m_runningSessions.find(sessionId) != m_runningSessions.end());
    });

    appendLog(QString("[%1] DialWidget ready.").arg(nowTag()));
}

DialWidget::~DialWidget()
{
    // m_server 是 QObject child，会自动析构；它自己的析构里也会 stop/uninit（双保险）
    delete ui;
}

void DialWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    applySplitterRatioOnce();
}

void DialWidget::ensureWebView()
{
    if (m_view) return;

    // ui 里 videoContainer 是 native=true 且没 layout，需要我们自己补一个 layout 再塞控件进去
    if (!ui->videoContainer->layout()) {
        auto* lay = new QVBoxLayout(ui->videoContainer);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);
        ui->videoContainer->setLayout(lay);
    }

    m_view = new QWebEngineView(ui->videoContainer);
    ui->videoContainer->layout()->addWidget(m_view);

    // 让 YouTube TV 更像“客厅端”
    m_view->page()->profile()->setHttpUserAgent(
        "Mozilla/5.0 (PlayStation 3; 3.55) AppleWebKit/531.22.8 "
        "(KHTML, like Gecko) Version/4.0 Safari/531.22.8");

    // 测试阶段尽量别被手势策略卡住（后面你可以再收紧）
    m_view->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);

    auto* page = new YtHookPage(m_view);
    m_view->setPage(page);

    connect(page, &YtHookPage::screenIdFound, this,
            [this](const QString& screenId, const QString& secret) {
                qInfo() << "[YT] screenId =" << screenId << "secret =" << secret;
                appendLog(QString("[%1] [YT  ] screenId=%2").arg(nowTag(), screenId));
                Q_UNUSED(secret);
            });

    connect(page, &YtHookPage::loungeTokenFound, this,
            [this](const QString& screenId, const QString& token, qint64 exp) {
                qInfo() << "[YT] lounge screenId =" << screenId << "token =" << token << "exp =" << exp;
                appendLog(QString("[%1] [YT  ] loungeToken ok (exp=%2)").arg(nowTag()).arg(exp));
            });

    connect(page, &YtHookPage::mediaMetaFound, this,
            [this](const QString& videoId, const QString& listId) {
                qInfo() << "[YT] mediaMeta videoId=" << videoId << "listId=" << listId;
            });

    connect(page, &YtHookPage::playbackFound, this,
            [this](double t, double rate, const QString& st) {
                qInfo() << "[YT] playback t=" << t << "rate=" << rate << "state=" << st;
            });

    QWebEngineScript script;
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(QString::fromUtf8(kYtHookJs));
    page->scripts().insert(script);

    m_view->setUrl(QUrl("https://www.youtube.com/tv"));

    QWebEngineView* dev = new QWebEngineView;
    dev->resize(1200, 800);
    dev->show();
    m_view->page()->setDevToolsPage(dev->page());
}

void DialWidget::applySplitterRatioOnce()
{
    if (splitterInited_) return;
    splitterInited_ = true;

    ui->splitter->setHandleWidth(6);
    ui->splitter->setChildrenCollapsible(false);

    ui->splitter->setStretchFactor(0, 7); // 0=videoContainer
    ui->splitter->setStretchFactor(1, 3); // 1=logContainer

    const int total = ui->splitter->height();
    if (total > 0) {
        const int top = total * 7 / 10;
        const int bot = total - top;
        ui->splitter->setSizes({ top, bot });
    }
}

void DialWidget::onStartClicked()
{
    if (!m_server) return;

    if (!m_server->isInited()) {
        const bool ok = m_server->init("WQtLib DIAL");
        if (!ok) {
            appendLog(QString("[%1] [ERR ] DIAL init failed.").arg(nowTag()));
            return;
        }
        appendLog(QString("[%1] [INFO] DIAL inited.").arg(nowTag()));
    }

    if (m_server->start()) {
        appendLog(QString("[%1] [INFO] DIAL started.").arg(nowTag()));
    } else {
        appendLog(QString("[%1] [ERR ] DIAL start failed.").arg(nowTag()));
    }
}

void DialWidget::onStopClicked()
{
    if (!m_server) return;

    m_server->stop();

    {
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        m_runningSessions.clear();
    }

    appendLog(QString("[%1] [INFO] DIAL stopped.").arg(nowTag()));
}

void DialWidget::onClearLogClicked()
{
    ui->textLog->clear();
}

void DialWidget::onYoutubeStart(uint32_t sessionId, const QString& url)
{
    {
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        m_runningSessions.insert(sessionId);
    }

    appendLog(QString("[%1] [YT  ] start sid=%2 url=%3")
                  .arg(nowTag()).arg(sessionId).arg(url));

    ensureWebView();
    m_view->setUrl(QUrl(url));
}

void DialWidget::onYoutubeStop(uint32_t sessionId)
{
    {
        std::lock_guard<std::mutex> lk(m_sessionsMu);
        m_runningSessions.erase(sessionId);
    }

    appendLog(QString("[%1] [YT  ] stop  sid=%2").arg(nowTag()).arg(sessionId));
}

void DialWidget::onYoutubeHide(uint32_t sessionId)
{
    appendLog(QString("[%1] [YT  ] hide  sid=%2").arg(nowTag()).arg(sessionId));
}

void DialWidget::appendLog(const QString& text)
{
    ui->textLog->append(text);
    if (auto* bar = ui->textLog->verticalScrollBar()) {
        bar->setValue(bar->maximum());
    }
}
