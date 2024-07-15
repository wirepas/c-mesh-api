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
