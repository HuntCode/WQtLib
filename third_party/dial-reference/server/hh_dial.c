#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "hh_dial.h"

#include "dial_server.h"
#include "quick_ssdp.h"
#include "system_callbacks.h"

static int g_initialized = 0;  /* 是否已调用过 HHDialInit */
static int g_running = 0;  /* DIAL 线程是否在跑 */
static volatile int g_stop_requested = 0;

static HHDialCallbacks g_callbacks;       /* 当前回调集合（浅拷贝） */
static int             g_callbacks_valid = 0;

#ifdef _WIN32
static HANDLE          g_dial_thread = NULL;
static int             g_wsa_inited = 0;
#else
static pthread_t       g_dial_thread;
static int             g_thread_valid = 0;
#endif

#define HH_DIAL_BUFSIZE 256

char spSleepPassword[HH_DIAL_BUFSIZE];

static char g_friendly_name[HH_DIAL_BUFSIZE] = "HHDIAL_Server";
static char g_model_name[HH_DIAL_BUFSIZE] = "deadbeef-dead-beef-dead-beefdeadbeef";
static char g_uuid[HH_DIAL_BUFSIZE] = "deadbeef-dead-beef-dead-beefdeadbeef";
static int  g_http_port_cfg = 0;  /* <=0 表示使用底层默认 */

static void hh_copy_cstr(char* dst, size_t dst_cap, const char* src)
{
    if (!dst || dst_cap == 0) return;
    if (!src || src[0] == '\0') return;
#if defined(_MSC_VER)
    strncpy_s(dst, dst_cap, src, _TRUNCATE);
#else
    strncpy(dst, src, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
#endif
}

static void hh_copy_config(const HHDialConfig* config)
{
    if (!config) return;

    if (config->friendly_name && config->friendly_name[0] != '\0') {
        hh_copy_cstr(g_friendly_name, sizeof(g_friendly_name), config->friendly_name);
    }
    if (config->model_name && config->model_name[0] != '\0') {
        hh_copy_cstr(g_model_name, sizeof(g_model_name), config->model_name);
    }
    if (config->uuid && config->uuid[0] != '\0') {
        hh_copy_cstr(g_uuid, sizeof(g_uuid), config->uuid);
    }
    if (config->http_port > 0) {
        g_http_port_cfg = config->http_port;
    }
}

static void hh_copy_callbacks(const HHDialCallbacks* cbs)
{
    if (!cbs) {
        memset(&g_callbacks, 0, sizeof(g_callbacks));
        g_callbacks_valid = 0;
        return;
    }
    g_callbacks = *cbs;
    g_callbacks_valid = 1;
}

static void HHDialInternal_DispatchYoutubeStart(uint32_t session_id, const char* url)
{
    if (!g_callbacks_valid || !g_callbacks.on_youtube_start) {
        return;
    }
    g_callbacks.on_youtube_start(session_id, url, g_callbacks.user_data);
}

static void HHDialInternal_DispatchYoutubeStop(uint32_t session_id)
{
    if (!g_callbacks_valid || !g_callbacks.on_youtube_stop) {
        return;
    }
    g_callbacks.on_youtube_stop(session_id, g_callbacks.user_data);
}

static void HHDialInternal_DispatchYoutubeHide(uint32_t session_id)
{
    if (!g_callbacks_valid || !g_callbacks.on_youtube_hide) {
        return;
    }
    g_callbacks.on_youtube_hide(session_id, g_callbacks.user_data);
}

static int HHDialInternal_QueryYoutubeStatus(uint32_t session_id, int* can_stop)
{
    if (!g_callbacks_valid || !g_callbacks.on_youtube_status) {
        if (can_stop) *can_stop = 1;   /* 默认允许 stop */
        return 0;                      /* 默认停止 */
    }
    return g_callbacks.on_youtube_status(session_id, can_stop, g_callbacks.user_data) ? 1 : 0;
}

static DIALStatus youtube_start(DIALServer* ds, const char* appname,
    const char* payload, const char* query_string,
    const char* additionalDataUrl,
    uint32_t session_id, DIAL_run_t* run_id, void* callback_data) {
    printf("\n\n ** LAUNCH YouTube ** with payload %s\n\n", payload);

    (void)ds; (void)appname; (void)query_string; (void)callback_data;

    char url[1024] = { 0 };
    const char* p = payload ? payload : "";
    const char* add = additionalDataUrl ? additionalDataUrl : "";

    if (p[0] && add[0]) {
        snprintf(url, sizeof(url), "https://www.youtube.com/tv?%s&%s", p, add);
    }
    else if (p[0]) {
        snprintf(url, sizeof(url), "https://www.youtube.com/tv?%s", p);
    }
    else {
        snprintf(url, sizeof(url), "https://www.youtube.com/tv");
    }

    /* 让 DIAL 的 run_id 跟 session_id 对齐：纯标识用途，不做状态管理 */
    if (run_id) {
        *run_id = (DIAL_run_t)(uintptr_t)session_id;
    }

    HHDialInternal_DispatchYoutubeStart(session_id, url);

    return kDIALStatusRunning;
}

static DIALStatus youtube_hide(DIALServer* ds, const char* app_name,
    uint32_t session_id, DIAL_run_t* run_id, void* callback_data)
{
    (void)ds; (void)app_name; (void)run_id; (void)callback_data;

    HHDialInternal_DispatchYoutubeHide(session_id);

    /* hide 需要返回 running/stopped：库不维护状态 => 只能问上层 */
    int can_stop = 1;
    int st = HHDialInternal_QueryYoutubeStatus(session_id, &can_stop);
    if (st >= 0) {
        return st ? kDIALStatusRunning : kDIALStatusStopped;
    }

    /* 上层未提供 status：兜底返回 Running（否则一些 sender 会认为 app 没起来） */
    return kDIALStatusRunning;
}

static DIALStatus youtube_status(DIALServer* ds, const char* appname,
    uint32_t session_id, DIAL_run_t run_id, int* pCanStop, void* callback_data) {
    (void)ds; (void)appname; (void)run_id; (void)callback_data;

    int st = HHDialInternal_QueryYoutubeStatus(session_id, pCanStop);
    if (st >= 0) {
        return st ? kDIALStatusRunning : kDIALStatusStopped;
    }

    /* 上层未提供 status：兜底 */
    if (pCanStop) *pCanStop = 1;
    return kDIALStatusRunning;
}

static void youtube_stop(DIALServer* ds, const char* appname, uint32_t session_id, DIAL_run_t run_id,
    void* callback_data) {
    printf("\n\n ** KILL YouTube **\n\n");
    (void)ds; (void)appname; (void)run_id; (void)callback_data;

    HHDialInternal_DispatchYoutubeStop(session_id);
}

int HHDialInternal_ShouldStop(void)
{
    return g_stop_requested != 0;
}

static void runDial(void)
{
    DIALServer* ds = DIAL_create();
    if (ds == NULL) {
        printf("Unable to create DIAL server.\n");
        return;
    }

    struct DIALAppCallbacks cb_yt = { youtube_start, youtube_hide, youtube_stop, youtube_status };
    struct DIALAppCallbacks cb_system = { system_start, system_hide, NULL, system_status };

//#if defined(DEBUG)
//    if (DIAL_register_app(ds, "YouTube", &cb_yt, NULL, 1,
//        "https://youtube.com https://www.youtube.com https://*.youtube.com:443 "
//        "https://port.youtube.com:123 package:com.google.android.youtube "
//        "package:com.google.ios.youtube proto:*") == -1 ||
//        DIAL_register_app(ds, "system", &cb_system, NULL, 1, "") == -1)
//#else
    if (DIAL_register_app(ds, "YouTube", &cb_yt, NULL, 1,
        "https://youtube.com https://*.youtube.com package:*") == -1 ||
        DIAL_register_app(ds, "system", &cb_system, NULL, 1, "") == -1)
//#endif
    {
        printf("Unable to register DIAL applications.\n");
        free(ds);
        return;
    }

    if (!DIAL_start(ds)) {
        printf("Unable to start DIAL master listening thread.\n");
        free(ds);
        return;
    }

    /* DIAL 的真实 HTTP 端口（原逻辑） */
    int dial_port = DIAL_get_port(ds);
    printf("launcher listening on DIAL port %d\n", dial_port);

    run_ssdp(dial_port, g_friendly_name, g_model_name, g_uuid);

    DIAL_stop(ds);
    free(ds);
}

#ifdef _WIN32
static unsigned __stdcall hh_dial_thread_proc(void* arg)
#else
static void* hh_dial_thread_proc(void* arg)
#endif
{
    (void)arg;

    g_running = 1;
    g_stop_requested = 0;

    runDial();

    g_running = 0;
    g_stop_requested = 0;

#ifdef _WIN32
    _endthreadex(0);
    return 0;
#else
    return NULL;
#endif
}

int HHDialInit(const HHDialConfig* config,
    const HHDialCallbacks* callbacks)
{
    if (g_initialized) {
        return 0;
    }

#ifdef _WIN32
    if (!g_wsa_inited) {
        WSADATA wsa;
        int ret = WSAStartup(MAKEWORD(2, 2), &wsa);
        if (ret != 0) {
            fprintf(stderr, "HHDial: WSAStartup failed: %d\n", ret);
            return -1;
        }
        g_wsa_inited = 1;
    }
#endif

    hh_copy_config(config);
    hh_copy_callbacks(callbacks);

    g_initialized = 1;
    return 0;
}

int HHDialStart(void)
{
    if (!g_initialized) {
        fprintf(stderr, "HHDial: HHDialStart called before HHDialInit.\n");
        return -1;
    }
    if (g_running) {
        return 0;
    }

#ifdef _WIN32
    uintptr_t h = _beginthreadex(NULL, 0, hh_dial_thread_proc, NULL, 0, NULL);
    if (!h) {
        fprintf(stderr, "HHDial: failed to create DIAL thread.\n");
        g_running = 0;
        return -1;
    }
    g_dial_thread = (HANDLE)h;
#else
    if (pthread_create(&g_dial_thread, NULL, hh_dial_thread_proc, NULL) != 0) {
        perror("HHDial: pthread_create");
        g_running = 0;
        return -1;
    }
    g_thread_valid = 1;
#endif

    return 0;
}

void HHDialStop(void)
{
    if (!g_running) {
        return;
    }

    g_stop_requested = 1;

#ifdef _WIN32
    if (g_dial_thread) {
        WaitForSingleObject(g_dial_thread, INFINITE);
        CloseHandle(g_dial_thread);
        g_dial_thread = NULL;
    }
#else
    if (g_thread_valid) {
        pthread_join(g_dial_thread, NULL);
        g_thread_valid = 0;
    }
#endif

    g_stop_requested = 0;
}

void HHDialUninit(void)
{
    if (!g_initialized) {
        return;
    }

    if (g_running) {
        HHDialStop();
    }

#ifdef _WIN32
    if (g_wsa_inited) {
        WSACleanup();
        g_wsa_inited = 0;
    }
#endif

    memset(&g_callbacks, 0, sizeof(g_callbacks));
    g_callbacks_valid = 0;
    g_stop_requested = 0;
    g_initialized = 0;
}

