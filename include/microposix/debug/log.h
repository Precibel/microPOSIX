#ifndef MICROPOSIX_LOG_H
#define MICROPOSIX_LOG_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef enum {
    MP_LOG_VERBOSE,
    MP_LOG_DEBUG,
    MP_LOG_INFO,
    MP_LOG_WARNING,
    MP_LOG_ERROR,
    MP_LOG_CRITICAL,
    MP_LOG_NONE
} mp_log_level_t;

// Log function
void mp_log(mp_log_level_t level, const char *module, const char *fmt, ...);

// Macros for convenience
#define MP_LOGV(module, fmt, ...) mp_log(MP_LOG_VERBOSE, module, fmt, ##__VA_ARGS__)
#define MP_LOGD(module, fmt, ...) mp_log(MP_LOG_DEBUG, module, fmt, ##__VA_ARGS__)
#define MP_LOGI(module, fmt, ...) mp_log(MP_LOG_INFO, module, fmt, ##__VA_ARGS__)
#define MP_LOGW(module, fmt, ...) mp_log(MP_LOG_WARNING, module, fmt, ##__VA_ARGS__)
#define MP_LOGE(module, fmt, ...) mp_log(MP_LOG_ERROR, module, fmt, ##__VA_ARGS__)
#define MP_LOGC(module, fmt, ...) mp_log(MP_LOG_CRITICAL, module, fmt, ##__VA_ARGS__)

#endif // MICROPOSIX_LOG_H
