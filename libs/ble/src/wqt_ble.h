#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WqtBleClient WqtBleClient;

typedef void (*WqtBleLogCallback)(void* user_data,
                                  int   level,
                                  const char* message);

WqtBleClient* wqt_ble_create(void);
void          wqt_ble_destroy(WqtBleClient* cli);

int  wqt_ble_start_scan(WqtBleClient* cli);
int  wqt_ble_stop_scan(WqtBleClient* cli);

void wqt_ble_set_log_callback(WqtBleClient* cli,
                              WqtBleLogCallback cb,
                              void* user_data);

#ifdef __cplusplus
}
#endif
