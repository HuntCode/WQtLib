#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    int run_ssdp(int dial_port,
        const char* friendly_name,
        const char* model_name,
        const char* uuid);

#ifdef __cplusplus
}
#endif
