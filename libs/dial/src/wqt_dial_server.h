#pragma once

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

class WQtDialServer : public QObject {
    Q_OBJECT
public:
    explicit WQtDialServer(QObject* parent = nullptr);
    ~WQtDialServer() override;

    // ---- 生命周期 ----
    // 只设置 friendly_name（最常用）
    bool init(const QString& friendlyName);

    // 可选：设置 model/uuid/httpPort（你现在 httpPort 还没真正用起来也没关系）
    bool initEx(const QString& friendlyName,
                const QString& modelName,
                const QString& uuid,
                int httpPort = 0);

    bool start();
    void stop();
    void uninit();

    bool isInited() const noexcept { return inited_; }
    bool isStarted() const noexcept { return started_; }

    // ---- 状态回调（同步）----
    // DIAL 在处理 /apps/YouTube 时会调用 status/hide，属于“HTTP 请求线程/网络线程”语境，
    // 这里必须是同步、快速、线程安全。
    //
    // 返回值：true=Running, false=Stopped
    // canStop：输出参数，是否允许 stop
    using YoutubeStatusCallback = std::function<bool(uint32_t sessionId, bool& canStop)>;
    void setYoutubeStatusCallback(YoutubeStatusCallback cb);

signals:
    void youtubeStart(uint32_t sessionId, const QString& url);
    void youtubeStop(uint32_t sessionId);
    void youtubeHide(uint32_t sessionId);

private:
    // ---- C 回调 thunk（static）----
    static void OnYoutubeStartThunk(uint32_t sessionId, const char* url, void* userData);
    static void OnYoutubeStopThunk(uint32_t sessionId, void* userData);
    static void OnYoutubeHideThunk(uint32_t sessionId, void* userData);
    static int  OnYoutubeStatusThunk(uint32_t sessionId, int* canStop, void* userData);

private:
    bool initImpl(const QString& friendlyName,
                  const QString& modelName,
                  const QString& uuid,
                  int httpPort);

    // 投递到 Qt 线程再 emit
    void postYoutubeStart(uint32_t sessionId, const QString& url);
    void postYoutubeStop(uint32_t sessionId);
    void postYoutubeHide(uint32_t sessionId);

private:
    // 保证传给 HHDialConfig 的 const char* 生命周期足够长
    std::string friendlyNameUtf8_;
    std::string modelNameUtf8_;
    std::string uuidUtf8_;
    int httpPort_ = 0;

    bool inited_ = false;
    bool started_ = false;

    mutable std::mutex cbMutex_;
    YoutubeStatusCallback statusCb_;
};
