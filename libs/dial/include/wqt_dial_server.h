#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WqtDialServer WqtDialServer;

typedef void (*wqt_dial_log_cb)(void* user_data, int level, const char* message);

/**
 * 创建 DIAL server 实例（桩实现）
 *
 * @param friendly_name   设备名/友好名称
 * @param uuid            设备 UUID，占位
 * @param http_port       HTTP 监听端口，占位
 *
 * @return 非 NULL 表示成功
 */
WqtDialServer* wqt_dial_server_create(const char* friendly_name,
                                      const char* uuid,
                                      int http_port);

/**
 * 销毁实例
 */
void wqt_dial_server_destroy(WqtDialServer* server);

/**
 * 启动服务（桩实现，仅记录状态并回调日志）
 *
 * @return 0 成功，非 0 失败
 */
int wqt_dial_server_start(WqtDialServer* server);

/**
 * 停止服务
 *
 * @return 0 成功，非 0 失败
 */
int wqt_dial_server_stop(WqtDialServer* server);

/**
 * 设置日志回调
 */
void wqt_dial_server_set_log_callback(WqtDialServer* server,
                                      wqt_dial_log_cb cb,
                                      void* user_data);

#ifdef __cplusplus
}
#endif
