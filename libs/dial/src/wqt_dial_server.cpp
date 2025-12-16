#include "wqt_dial_server.h"

#include <QMetaObject>
#include <QPointer>
#include <Qt>

extern "C" {
#include "hh_dial.h"
}

WQtDialServer::WQtDialServer(QObject* parent)
    : QObject(parent) {}

WQtDialServer::~WQtDialServer() {
    // 防御性：避免资源泄露/后台线程悬挂
    stop();
    uninit();
}

bool WQtDialServer::init(const QString& friendlyName) {
    return initImpl(friendlyName, QString(), QString(), 0);
}

bool WQtDialServer::initEx(const QString& friendlyName,
                           const QString& modelName,
                           const QString& uuid,
                           int httpPort) {
    return initImpl(friendlyName, modelName, uuid, httpPort);
}

bool WQtDialServer::initImpl(const QString& friendlyName,
                             const QString& modelName,
                             const QString& uuid,
                             int httpPort)
{
    if (inited_) {
        return true; // 你前面已经决定：init 之后不再允许热改
    }

    // 保存 UTF-8 字符串到成员，确保生命周期覆盖整个 HHDial 生命周期
    friendlyNameUtf8_ = friendlyName.toUtf8().constData();
    modelNameUtf8_    = modelName.isEmpty() ? std::string() : std::string(modelName.toUtf8().constData());
    uuidUtf8_         = uuid.isEmpty() ? std::string() : std::string(uuid.toUtf8().constData());
    httpPort_         = httpPort;

    HHDialConfig cfg{};
    cfg.friendly_name = friendlyNameUtf8_.empty() ? nullptr : friendlyNameUtf8_.c_str();
    cfg.model_name    = modelNameUtf8_.empty() ? nullptr : modelNameUtf8_.c_str();
    cfg.uuid          = uuidUtf8_.empty() ? nullptr : uuidUtf8_.c_str();
    cfg.http_port     = httpPort_;

    HHDialCallbacks cbs{};
    cbs.on_youtube_start  = &WQtDialServer::OnYoutubeStartThunk;
    cbs.on_youtube_stop   = &WQtDialServer::OnYoutubeStopThunk;
    cbs.on_youtube_hide   = &WQtDialServer::OnYoutubeHideThunk;
    cbs.on_youtube_status = &WQtDialServer::OnYoutubeStatusThunk;
    cbs.user_data         = this;

    const int ret = HHDialInit(&cfg, &cbs);
    if (ret != 0) {
        return false;
    }

    inited_ = true;
    return true;
}

bool WQtDialServer::start() {
    if (!inited_) return false;
    if (started_) return true;

    const int ret = HHDialStart();
    if (ret != 0) {
        return false;
    }
    started_ = true;
    return true;
}

void WQtDialServer::stop() {
    if (!started_) return;
    HHDialStop();
    started_ = false;
}

void WQtDialServer::uninit() {
    if (!inited_) return;

    // 你的 hh_dial.c 里 Uninit 会先 Stop（如果 running）
    HHDialUninit();

    inited_ = false;
    started_ = false;

    // 可选：清空字符串，避免误用（不是必须）
    friendlyNameUtf8_.clear();
    modelNameUtf8_.clear();
    uuidUtf8_.clear();
    httpPort_ = 0;

    std::lock_guard<std::mutex> lk(cbMutex_);
    statusCb_ = nullptr;
}

void WQtDialServer::setYoutubeStatusCallback(YoutubeStatusCallback cb) {
    std::lock_guard<std::mutex> lk(cbMutex_);
    statusCb_ = std::move(cb);
}

/* ===================== C 回调 thunk ===================== */

void WQtDialServer::OnYoutubeStartThunk(uint32_t sessionId, const char* url, void* userData) {
    auto* self = static_cast<WQtDialServer*>(userData);
    if (!self) return;

    const QString qurl = QString::fromUtf8(url ? url : "");
    self->postYoutubeStart(sessionId, qurl);
}

void WQtDialServer::OnYoutubeStopThunk(uint32_t sessionId, void* userData) {
    auto* self = static_cast<WQtDialServer*>(userData);
    if (!self) return;

    self->postYoutubeStop(sessionId);
}

void WQtDialServer::OnYoutubeHideThunk(uint32_t sessionId, void* userData) {
    auto* self = static_cast<WQtDialServer*>(userData);
    if (!self) return;

    self->postYoutubeHide(sessionId);
}

int WQtDialServer::OnYoutubeStatusThunk(uint32_t sessionId, int* canStop, void* userData) {
    auto* self = static_cast<WQtDialServer*>(userData);
    if (!self) {
        if (canStop) *canStop = 1;
        return 1; // 默认 running（兼容性更好）
    }

    YoutubeStatusCallback cbCopy;
    {
        std::lock_guard<std::mutex> lk(self->cbMutex_);
        cbCopy = self->statusCb_;
    }

    bool can = true;
    bool running = true;

    if (cbCopy) {
        running = cbCopy(sessionId, can);
    } else {
        // 上层没提供 status：兜底 running，避免 sender 认为 app 没起来
        running = true;
        can = true;
    }

    if (canStop) *canStop = can ? 1 : 0;
    return running ? 1 : 0;
}

/* ===================== 投递到 Qt 线程再 emit ===================== */

void WQtDialServer::postYoutubeStart(uint32_t sessionId, const QString& url) {
    QPointer<WQtDialServer> that(this);
    QMetaObject::invokeMethod(this, [that, sessionId, url]() {
        if (!that) return;
        emit that->youtubeStart(sessionId, url);
    }, Qt::QueuedConnection);
}

void WQtDialServer::postYoutubeStop(uint32_t sessionId) {
    QPointer<WQtDialServer> that(this);
    QMetaObject::invokeMethod(this, [that, sessionId]() {
        if (!that) return;
        emit that->youtubeStop(sessionId);
    }, Qt::QueuedConnection);
}

void WQtDialServer::postYoutubeHide(uint32_t sessionId) {
    QPointer<WQtDialServer> that(this);
    QMetaObject::invokeMethod(this, [that, sessionId]() {
        if (!that) return;
        emit that->youtubeHide(sessionId);
    }, Qt::QueuedConnection);
}
