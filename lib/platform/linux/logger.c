/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdint.h>
#include <stdio.h>

#include <time.h>

static inline void get_timestamp(char timestamp[26])
{
    time_t ltime;
    struct tm result;

    ltime = time(NULL);
    localtime_r(&ltime, &result);
    sprintf(timestamp, "%d:%d:%d", result.tm_hour, result.tm_min, result.tm_sec);
}

static inline void print_prefix(char level, char * module)
{
    char timestamp[26];
    get_timestamp(timestamp);
    printf("[%s][%s] %c:", module, timestamp, level);
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
