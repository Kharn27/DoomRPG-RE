#ifndef DOOMRPG_LOG_H
#define DOOMRPG_LOG_H

#include <stdio.h>

#define DOOMRPG_LOG_ERROR 1
#define DOOMRPG_LOG_INFO  2
#define DOOMRPG_LOG_DEBUG 3
#define DOOMRPG_LOG_TRACE 4

#ifndef DOOMRPG_LOG_LEVEL
#define DOOMRPG_LOG_LEVEL DOOMRPG_LOG_INFO
#endif

#if DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_ERROR || \
    DOOMRPG_LOG_LEVEL > DOOMRPG_LOG_TRACE
#error "DOOMRPG_LOG_LEVEL must be ERROR, INFO, DEBUG or TRACE"
#endif

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_ERROR
#define DRPG_LOGE(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DRPG_LOGE(...) do { } while (0)
#endif

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_INFO
#define DRPG_LOGI(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DRPG_LOGI(...) do { } while (0)
#endif

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_DEBUG
#define DRPG_LOGD(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DRPG_LOGD(...) do { } while (0)
#endif

#if DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE
#define DRPG_LOGT(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DRPG_LOGT(...) do { } while (0)
#endif

#endif
