/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdint.h>
#include <stdio.h>

#include <time.h>

/* Full log level string  */
static char DEBUG[] = "DEBUG";
static char INFO[] = "INFO";
static char WARNING[] = "WARNING";
static char ERROR[] = "ERROR";

static inline void get_timestamp(char timestamp[37])
{
    struct tm result;
    long ms;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    localtime_r(&s, &result);

    ms = spec.tv_nsec / 1.0e6;  // Convert nanoseconds to milliseconds

    sprintf(timestamp,
            "%d-%02d-%02d %02d:%02d:%02d,%03ld",
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
    char timestamp[37];
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
        if ((i & 0xF) == 0xF)
            printf("\n");
        printf("%02x ", buffer[i]);
    }
    printf("\n");
}
