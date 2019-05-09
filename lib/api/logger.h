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
 *          Log level as a letter: D, I, W, E
 * \param   moule
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
 * Macros to define several level of Log: Debug(3), Info(2), Warning(1),
 * Error(0)
 */
#define DEBUG_LOG_LEVEL 3
#define INFO_LOG_LEVEL 2
#define WARNING_LOG_LEVEL 1
#define ERROR_LOG_LEVEL 0
#define NO_LOG_LEVEL -1

/**
 * Only logs with a level lower or equal to MAX_LOG_LEVEL will be printed
 *
 * \note this constant can be defined in each file including this file
 */
#ifndef MAX_LOG_LEVEL
/* By default only errors are displayed */
#    define MAX_LOG_LEVEL ERROR_LOG_LEVEL
#endif

#ifndef LOG_MODULE_NAME
/* Name of the module */
#    error "No module name set for logger"
#endif

/**
 * Helpers macros to print logs
 */
#if MAX_LOG_LEVEL >= DEBUG_LOG_LEVEL
static inline void LOGD(char * format, ...)
{
    va_list arg;
    va_start(arg, format);
    Platform_LOG('D', LOG_MODULE_NAME, format, arg);
    va_end(arg);
}
#else
#    define LOGD(__log__, ...)
#endif

#if MAX_LOG_LEVEL >= INFO_LOG_LEVEL
static inline void LOGI(char * format, ...)
{
    va_list arg;
    va_start(arg, format);
    Platform_LOG('I', LOG_MODULE_NAME, format, arg);
    va_end(arg);
}
#else
#    define LOGI(__log__, ...)
#endif

#if MAX_LOG_LEVEL >= WARNING_LOG_LEVEL
static inline void LOGW(char * format, ...)
{
    va_list arg;
    va_start(arg, format);
    Platform_LOG('W', LOG_MODULE_NAME, format, arg);
    va_end(arg);
}
#else
#    define LOGW(__log__, ...)
#endif

#if MAX_LOG_LEVEL >= ERROR_LOG_LEVEL
static inline void LOGE(char * format, ...)
{
    va_list arg;
    va_start(arg, format);
    Platform_LOG('E', LOG_MODULE_NAME, format, arg);
    va_end(arg);
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
