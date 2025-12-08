#include "wqt_ble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct WqtBleClient {
    int scanning;
    WqtBleLogCallback log_cb;
    void* log_user;
};

static void ble_log(WqtBleClient* c, int level, const char* msg)
{
    const char* lvl = "INFO";
    if (level == 1) lvl = "WARN";
    else if (level >= 2) lvl = "ERR";

    if (c && c->log_cb) {
        c->log_cb(c->log_user, level, msg);
    } else {
        fprintf(stderr, "[WqtBleClient][%s] %s\n", lvl, msg);
    }
}

WqtBleClient* wqt_ble_create(void)
{
    WqtBleClient* c = (WqtBleClient*)calloc(1, sizeof(WqtBleClient));
    if (!c) return NULL;
    c->scanning = 0;
    c->log_cb   = NULL;
    c->log_user = NULL;
    ble_log(c, 0, "create() stub");
    return c;
}

void wqt_ble_destroy(WqtBleClient* cli)
{
    if (!cli) return;
    ble_log(cli, 0, "destroy() stub");
    free(cli);
}

int wqt_ble_start_scan(WqtBleClient* cli)
{
    if (!cli) return -1;
    if (cli->scanning) {
        ble_log(cli, 1, "start_scan() but already scanning");
        return 0;
    }
    cli->scanning = 1;
    ble_log(cli, 0, "start_scan() stub: start BLE scanning");
    return 0;
}

int wqt_ble_stop_scan(WqtBleClient* cli)
{
    if (!cli) return -1;
    if (!cli->scanning) {
        ble_log(cli, 1, "stop_scan() but not scanning");
        return 0;
    }
    cli->scanning = 0;
    ble_log(cli, 0, "stop_scan() stub: stop BLE scanning");
    return 0;
}

void wqt_ble_set_log_callback(WqtBleClient* cli,
                              WqtBleLogCallback cb,
                              void* user_data)
{
    if (!cli) return;
    cli->log_cb   = cb;
    cli->log_user = user_data;
}
