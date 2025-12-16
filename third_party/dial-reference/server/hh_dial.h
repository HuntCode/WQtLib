#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /* DIAL 服务配置 */
    typedef struct HHDialConfig {
        /* 设备对外显示的名称（可选，NULL 表示使用库内默认） */
        const char* friendly_name;

        /* 设备型号名称（可选，NULL 表示使用库内默认） */
        const char* model_name;

        /* 设备 UUID（可选，NULL 表示使用库内默认） */
        const char* uuid;

        /* DIAL HTTP 监听端口（<=0 表示让库使用默认逻辑） */
        int http_port;
    } HHDialConfig;

    /* YouTube 启动回调：
     * 每次有新的投屏请求时触发。
     *
     * 参数：
     *  - session_id: 会话 ID（当前实现你会用 sender IPv4）
     *  - url:        最终构造好的 YouTube 播放 URL（UTF-8，以 '\0' 结尾）
     *  - user_data:  HHDialInit 时注册的用户指针原样传回
     */
    typedef void (*HHDialYoutubeStartCb)(uint32_t session_id,
        const char* url,
        void* user_data);

    /* YouTube 停止回调：
     * 对应的会话结束时触发。
     *
     * 参数：
     *  - session_id: 要结束的会话 ID（与 start 时一致）
     *  - user_data:  HHDialInit 时注册的用户指针
     */
    typedef void (*HHDialYoutubeStopCb)(uint32_t session_id,
        void* user_data);

    /* YouTube Hide 回调：
     * - DIAL 的 hide 语义：请求“隐藏”应用（是否真正隐藏由上层决定）
     */
    typedef void (*HHDialYoutubeHideCb)(uint32_t session_id,
        void* user_data);

    /* YouTube Status 回调：
     * - 让上层告诉库：当前 session 是否在运行
     * - 返回值：1=running, 0=stopped
     * - can_stop：可选输出，1=允许 stop，0=不允许 stop（可传 NULL）
     */
    typedef int (*HHDialYoutubeStatusCb)(uint32_t session_id,
        int* can_stop,
        void* user_data);

    /* 回调集合，将来如果需要可以在这里扩展更多回调，例如日志、错误通知等 */
    typedef struct HHDialCallbacks {
        HHDialYoutubeStartCb on_youtube_start;
        HHDialYoutubeStopCb  on_youtube_stop;
        HHDialYoutubeHideCb   on_youtube_hide;
        HHDialYoutubeStatusCb on_youtube_status;
        void* user_data;
    } HHDialCallbacks;

    /* 初始化 DIAL 模块
     *
     * - config    : 可选配置，传 NULL 表示全部使用默认
     * - callbacks : 可选回调，传 NULL 表示不关心 YouTube 事件（仅作为协议服务）。
     *
     * 返回：
     *  - 0  : 成功
     *  - <0 : 失败（例如 Windows 下 WSAStartup 失败等）
     */
    int HHDialInit(const HHDialConfig* config, const HHDialCallbacks* callbacks);

    /* 启动 DIAL 服务
     *
     * - 必须在 HHDialInit 成功后调用。
     * - 内部会创建 DIALServer、注册应用，并在新线程中启动 HTTP + SSDP 循环。
     * - 调用返回后服务在后台运行。
     *
     * 返回：
     *  - 0  : 启动成功
     *  - <0 : 启动失败
     */
    int HHDialStart(void);

    /* 停止 DIAL 服务
     *
     * - 请求内部线程优雅退出，等待其结束。
     * - 不清除配置，仅停止服务。
     * - 必须在 HHDialUninit 之前调用。
     */
    void HHDialStop(void);

    /* 反初始化 DIAL 模块
     *
     * - 清理初始化阶段分配的资源（例如 Windows 下调用 WSACleanup）
     */
    void HHDialUninit(void);

#ifdef __cplusplus
}
#endif
