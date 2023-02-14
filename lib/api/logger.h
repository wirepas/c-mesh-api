/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * Simple logger lib to print only relevant log messages.
 *
 * Usage is as followed:
 *
 * #define LOG_MODULE_NAME "module_name"
 * #define MAX_LOG_LEVEL INFO_LOG_LEVEL
 * //#define PRINT_BUFFERS // Un-comment to print buffers
 * #include "logger.h"
 *
 * ...
 * LOGD("This is a debug message not printed with current MAX_LOG_LEVEL n=%d",
 * n);
 * ...
 * LOGI("This is an info message printed with current MAX_LOG_LEVEL n=%d", n);
 * ...
 *
 */

/*
 * Both following functions are declared extern instead of a dedicated
 * header file to avoid multiple inclusion headers for that module.
 * This Logger lib header is not a real lib as depending on parameter
 * defined before including this file: MAX_LOG_LEVEL and LOG_MODULE_NAME
 */

/**
 * \brief   Implemented in platform specific part to print a LOG
 * \param   level
 *          Log level as a letter: D, V, I, W, E
 * \param   module
 *          Module nam
 * \param   format
 *          Message to print
 * \param   args
 *          Argument for the string format
 */
extern void Platform_LOG(char level, char * module, char * format, va_list args);

/**
 * \brief   Implemented in platform specific part to print a buffer
 * \param   buffer
 *          the buffer to print
 * \param   size
 *          the size of the buffer to print
 */
extern void Platform_print_buffer(uint8_t * buffer, int size);

/**
 * Macros to define several level of Log:  Debug(4), Verbose(3), Info(2), Warning(1),
 * Error(0)
 */
#define DEBUG_LOG_LEVEL 4
#define VERBOSE_LOG_LEVEL 3
#define INFO_LOG_LEVEL 2
#define WARNING_LOG_LEVEL 1
#define ERROR_LOG_LEVEL 0
#define NO_LOG_LEVEL -1

/**
 * Only logs with a level lower or equal to MAX_LOG_LEVEL can be printed
 *
 * \note this constant can be defined in each file including this file
 */
#ifndef MAX_LOG_LEVEL
/* By default, the global maximum log level will prevail. */
#    define MAX_LOG_LEVEL DEBUG_LOG_LEVEL
#endif

#ifndef LOG_MODULE_NAME
/* Name of the module */
#    error "No module name set for logger"
#endif

/**
 * Global maximum log level for the files including this file.
 *
 * \note MAX_LOG_LEVEL can only decrease the global maximum log level.
 */
extern int8_t m_logger_global_max_log_level;

/**
 * Initialize m_logger_global_max_log_level variable with environment variable
 */
extern void Logger_set_global_max_log_level(int8_t max_log_level);

/**
 * Helpers macros to print logs
 */
#if MAX_LOG_LEVEL >= DEBUG_LOG_LEVEL
static inline void LOGD(char * format, ...)
{
    if(m_logger_global_max_log_level >= DEBUG_LOG_LEVEL)
    {
        va_list arg;
        va_start(arg, format);
        Platform_LOG('D', LOG_MODULE_NAME, format, arg);
        va_end(arg);
    }
}
#else
#    define LOGD(__log__, ...)
#endif

#if MAX_LOG_LEVEL >= INFO_LOG_LEVEL
static inline void LOGI(char * format, ...)
{
    if(m_logger_global_max_log_level >= INFO_LOG_LEVEL)
    {
        va_list arg;
        va_start(arg, format);
        Platform_LOG('I', LOG_MODULE_NAME, format, arg);
        va_end(arg);
    }
}
#else
#    define LOGI(__log__, ...)
#endif

#if MAX_LOG_LEVEL >= WARNING_LOG_LEVEL
static inline void LOGW(char * format, ...)
{
    if(m_logger_global_max_log_level >= WARNING_LOG_LEVEL)
    {
        va_list arg;
        va_start(arg, format);
        Platform_LOG('W', LOG_MODULE_NAME, format, arg);
        va_end(arg);
    }
}
#else
#    define LOGW(__log__, ...)
#endif

#if MAX_LOG_LEVEL >= VERBOSE_LOG_LEVEL
static inline void LOGV(char * format, ...)
{
    if(m_logger_global_max_log_level >= VERBOSE_LOG_LEVEL)
    {
        va_list arg;
        va_start(arg, format);
        Platform_LOG('V', LOG_MODULE_NAME, format, arg);
        va_end(arg);
    }
}
#else
#    define LOGV(__log__, ...)
#endif

#if MAX_LOG_LEVEL >= ERROR_LOG_LEVEL
static inline void LOGE(char * format, ...)
{
    if(m_logger_global_max_log_level >= ERROR_LOG_LEVEL)
    {
        va_list arg;
        va_start(arg, format);
        Platform_LOG('E', LOG_MODULE_NAME, format, arg);
        va_end(arg);
    }
}
#else
#    define LOGE(__log__, ...)
#endif

#ifdef PRINT_BUFFERS
static inline void LOG_PRINT_BUFFER(uint8_t * buffer, int size)
{
    Platform_print_buffer(buffer, size);
}
#else
#    define LOG_PRINT_BUFFER(buffer, size)
#endif
