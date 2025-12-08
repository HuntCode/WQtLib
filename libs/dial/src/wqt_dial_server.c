#include "wqt_dial_server.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct WqtDialServer {
    char* friendly_name;
    char* uuid;
    int   http_port;

    int   running;

    wqt_dial_log_cb log_cb;
    void*           log_user_data;
};

static void wqt_dial_log(WqtDialServer* s, int level, const char* msg)
{
    if (s && s->log_cb) {
        s->log_cb(s->log_user_data, level, msg);
    } else {
        /* 兜底直接输出到 stdout，方便早期调试 */
        const char* prefix = "[INFO]";
        if (level == 1) prefix = "[WARN]";
        else if (level >= 2) prefix = "[ERR ]";
        printf("%s %s\n", prefix, msg ? msg : "");
    }
}

WqtDialServer* wqt_dial_server_create(const char* friendly_name,
                                      const char* uuid,
                                      int http_port)
{
    WqtDialServer* s = (WqtDialServer*)calloc(1, sizeof(WqtDialServer));
    if (!s) return NULL;

    if (friendly_name) {
        s->friendly_name = _strdup(friendly_name);
    }
    if (uuid) {
        s->uuid = _strdup(uuid);
    }
    s->http_port = http_port;
    s->running   = 0;
    s->log_cb    = NULL;
    s->log_user_data = NULL;

    wqt_dial_log(s, 0, "WqtDialServer created (stub).");
    return s;
}

void wqt_dial_server_destroy(WqtDialServer* server)
{
    if (!server) return;

    if (server->running) {
        wqt_dial_log(server, 1, "Destroying server while running, forcing stop.");
    }

    free(server->friendly_name);
    free(server->uuid);
    free(server);
}

int wqt_dial_server_start(WqtDialServer* server)
{
    if (!server) return -1;
    if (server->running) {
        wqt_dial_log(server, 1, "Server already running.");
        return 0;
    }

    server->running = 1;
    wqt_dial_log(server, 0, "DIAL server started (stub, no real networking yet).");
    return 0;
}

int wqt_dial_server_stop(WqtDialServer* server)
{
    if (!server) return -1;
    if (!server->running) {
        wqt_dial_log(server, 1, "Server is not running.");
        return 0;
    }

    server->running = 0;
    wqt_dial_log(server, 0, "DIAL server stopped (stub).");
    return 0;
}

void wqt_dial_server_set_log_callback(WqtDialServer* server,
                                      wqt_dial_log_cb cb,
                                      void* user_data)
{
    if (!server) return;

    server->log_cb        = cb;
    server->log_user_data = user_data;

    wqt_dial_log(server, 0, "Log callback registered.");
}
