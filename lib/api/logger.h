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
 * All following functions are declared extern instead of a dedicated
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
 * \brief   Set the log level for all registered modules and future modules
 * \param   level
 *          Log level as defined with macros below (for example INFO_LOG_LEVEL)
 * \return  Non-negative value upon success,
 *          a negative value otherwise.
 * \note    This level will be applied to modules registered after this call
 */
extern int Platform_set_log_level(const int level);

/**
 * \brief   Register a logging module if needed and set its log level
 * \param   module_name
 *          Module name string
 * \param   level
 *          Log level as defined with macros below (for example INFO_LOG_LEVEL)
 * \return  Non-negative value upon success,
 *          a negative value otherwise.
 */
extern int Platform_set_module_log_level(const char *const module_name, const int level);

/**
 * \brief   Register a logging module if needed and get a pointer to its log level
 * \param   module_name
 *          Module name string
 * \param   default_level
 *          Default log level as defined with macros below (for example INFO_LOG_LEVEL)
 * \return  Pointer to the log modules log level
 * \note    Implementation should always return a non-NULL pointer. Upon
 *          failure, a valid pointer to a fallback value can be returned.
 *          If a global log level was previously set via Platform_set_log_level,
 *          that level takes precedence over the default_level.
 */
extern int *Platform_get_logging_module_level(const char *const module_name, const int default_level);

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

// Static pointer to this module's log level - initialized on first use
static int *__module_log_level = NULL;

// Get the module's log level, initializing the module if needed
static inline int __get_module_log_level(void)
{
    if (__module_log_level == NULL)
    {
        __module_log_level = Platform_get_logging_module_level(LOG_MODULE_NAME, MAX_LOG_LEVEL);
    }
    return *__module_log_level;
}

/**
 * Macro to generate logging functions
 */
#define DEFINE_LOG_FUNCTION(func_name, prefix, log_level) \
    static inline void func_name(char * format, ...) \
    { \
        if (__get_module_log_level() >= log_level) \
        { \
            va_list arg; \
            va_start(arg, format); \
            Platform_LOG(prefix, LOG_MODULE_NAME, format, arg); \
            va_end(arg); \
        } \
    }

/**
 * Define NO_BUILD_ALL_LOG_LEVELS to prevent building of logging functions with
 * higher logging levels than the MAX_LOG_LEVEL for the current module. Can be
 * used to decrease binary size.
 */
#if !defined(NO_BUILD_ALL_LOG_LEVELS)
#define BUILD_ALL_LOG_LEVELS
#endif

/**
 * Helpers macros to print logs
 */
#if defined(BUILD_ALL_LOG_LEVELS) || MAX_LOG_LEVEL >= DEBUG_LOG_LEVEL
DEFINE_LOG_FUNCTION(LOGD, 'D', DEBUG_LOG_LEVEL)
#else
#    define LOGD(__log__, ...)
#endif

#if defined(BUILD_ALL_LOG_LEVELS) || MAX_LOG_LEVEL >= INFO_LOG_LEVEL
DEFINE_LOG_FUNCTION(LOGI, 'I', INFO_LOG_LEVEL)
#else
#    define LOGI(__log__, ...)
#endif

#if defined(BUILD_ALL_LOG_LEVELS) || MAX_LOG_LEVEL >= WARNING_LOG_LEVEL
DEFINE_LOG_FUNCTION(LOGW, 'W', WARNING_LOG_LEVEL)
#else
#    define LOGW(__log__, ...)
#endif

#if defined(BUILD_ALL_LOG_LEVELS) || MAX_LOG_LEVEL >= ERROR_LOG_LEVEL
DEFINE_LOG_FUNCTION(LOGE, 'E', ERROR_LOG_LEVEL)
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
