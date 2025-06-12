/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdint.h>
#include <stdio.h>

#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

#define LOG_MODULE_NAME "logger"
#include "logger.h"

/* Full log level string  */
static char DEBUG[] = "DEBUG";
static char INFO[] = "INFO";
static char WARNING[] = "WARNING";
static char ERROR[] = "ERROR";

// Max module name length (including null terminator)
#define MAX_LOG_MODULE_NAME_LENGTH 32
#define MAX_LOG_MODULE_COUNT 50

struct LoggingModule
{
    bool is_initialized;
    int level;
    char name[MAX_LOG_MODULE_NAME_LENGTH];
};

static pthread_mutex_t m_logging_modules_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct LoggingModule m_logging_modules[MAX_LOG_MODULE_COUNT];
static struct LoggingModule m_fallback_module = {
    .is_initialized = true,
    .level = NO_LOG_LEVEL,
    .name = "FallbackLogger"
};

static int m_global_log_level = NO_LOG_LEVEL;
static bool m_is_global_log_level_set = false;

static bool is_logging_module_name_length_valid(const char *const module_name)
{
    return strnlen(module_name, MAX_LOG_MODULE_NAME_LENGTH) != MAX_LOG_MODULE_NAME_LENGTH;
}

static struct LoggingModule *get_logging_module(const char *const module_name)
{
    if (!is_logging_module_name_length_valid(module_name))
    {
        return NULL;
    }

    for (size_t i = 0; i < MAX_LOG_MODULE_COUNT; i++)
    {
        if (m_logging_modules[i].is_initialized &&
            strncmp(m_logging_modules[i].name, module_name, MAX_LOG_MODULE_NAME_LENGTH) == 0)
        {
            return &m_logging_modules[i];
        }
    }

    return NULL;
}

static struct LoggingModule *register_logging_module(const char *const module_name, const int default_level)
{
    if (!is_logging_module_name_length_valid(module_name))
    {
        return NULL;
    }

    for (size_t i = 0; i < MAX_LOG_MODULE_COUNT; i++)
    {
        if (!m_logging_modules[i].is_initialized)
        {
            m_logging_modules[i].is_initialized = true;
            m_logging_modules[i].level = default_level;
            strncpy(m_logging_modules[i].name, module_name, MAX_LOG_MODULE_NAME_LENGTH);
            return &m_logging_modules[i];
        }
    }

    return NULL;
}

static inline void get_timestamp(char * timestamp)
{
    struct tm result;
    uint16_t ms;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    localtime_r(&s, &result);

    ms = (uint16_t) (spec.tv_nsec / 1.0e6);  // Convert nanoseconds to milliseconds

    // We know that output is always 23 bytes. We could use snprintf,
    // but recent compiler would complain about size of the output.
    // In fact, we know that ms is <= 999 so only 3 digits but compiler doesn't know
    sprintf(timestamp,
            "%04d-%02d-%02d %02d:%02d:%02d,%03d",
            result.tm_year + 1900,  // tm_year is in year - 1900
            result.tm_mon + 1,      // tm_mon is in [0-11]
            result.tm_mday,
            result.tm_hour,
            result.tm_min,
            result.tm_sec,
            ms);
}

static inline void print_prefix(char level, char * module)
{
    // Timestamp should always feat to 23 char, but some margins
    char timestamp[50];
    char * full_level;

    switch (level)
    {
        case ('D'):
            full_level = DEBUG;
            break;
        case ('I'):
            full_level = INFO;
            break;
        case ('W'):
            full_level = WARNING;
            break;
        case ('E'):
            full_level = ERROR;
            break;
        default:
            full_level = &level;
    }
    get_timestamp(timestamp);
    printf("%s | [%s] %s:", timestamp, full_level, module);
}

void Platform_LOG(char level, char * module, char * format, va_list args)
{
    print_prefix(level, module);
    vprintf(format, args);
}

void Platform_print_buffer(uint8_t * buffer, int size)
{
    int i;
    for (i = 0; i < size; i++)
    {
        printf("%02x ", buffer[i]);
        if ((i & 0xF) == 0xF)
            printf("\n");
    }
    printf("\n");
}

int Platform_set_log_level(const int level)
{
    pthread_mutex_lock(&m_logging_modules_mutex);

    m_global_log_level = level;
    m_is_global_log_level_set = true;

    for (size_t i = 0; i < MAX_LOG_MODULE_COUNT; i++)
    {
        if (m_logging_modules[i].is_initialized)
        {
            m_logging_modules[i].level = level;
        }
    }

    pthread_mutex_unlock(&m_logging_modules_mutex);
    return 0;
}

int Platform_set_module_log_level(const char *const module_name, const int level)
{
    pthread_mutex_lock(&m_logging_modules_mutex);

    struct LoggingModule *module = get_logging_module(module_name);
    if (module == NULL)
    {
        module = register_logging_module(module_name, level);
        if (module == NULL)
        {
            pthread_mutex_unlock(&m_logging_modules_mutex);
            return -1;
        }
    }

    module->level = level;

    pthread_mutex_unlock(&m_logging_modules_mutex);
    return 0;
}

int *Platform_get_logging_module_level(const char *const module_name, const int default_level)
{
    pthread_mutex_lock(&m_logging_modules_mutex);

    struct LoggingModule *module = get_logging_module(module_name);
    if (module == NULL)
    {
        const int initial_level = m_is_global_log_level_set ? m_global_log_level : default_level;
        module = register_logging_module(module_name, initial_level);
        if (module == NULL)
        {
            pthread_mutex_unlock(&m_logging_modules_mutex);
            return &m_fallback_module.level;
        }
    }

    pthread_mutex_unlock(&m_logging_modules_mutex);
    return &module->level;
}

