/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <windows.h>

static inline void get_timestamp(char * buffer, size_t size)
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    snprintf(buffer,
             size,
             "%04u-%02u-%02u %02u:%02u:%02u.%03u",
             (unsigned int) now.wYear,
             (unsigned int) now.wMonth,
             (unsigned int) now.wDay,
             (unsigned int) now.wHour,
             (unsigned int) now.wMinute,
             (unsigned int) now.wSecond,
             (unsigned int) now.wMilliseconds);
}

void Platform_LOG(char level, const char * module, const char * format, va_list args)
{
    char timestamp[24];
    char desc_buf[2];
    const char * level_desc;

    if (level == 'D')
    {
        level_desc = "DEBUG";
    }
    else if (level == 'I')
    {
        level_desc = "INFO";
    }
    else if (level == 'W')
    {
        level_desc = "WARNING";
    }
    else if (level == 'E')
    {
        level_desc = "ERROR";
    }
    else
    {
        desc_buf[0] = level;
        desc_buf[1] = '\0';
        level_desc = desc_buf;
    }

    get_timestamp(timestamp, sizeof(timestamp));
    printf("%s | [%s] %s: ", timestamp, level_desc, module);
    vprintf(format, args);
}

void Platform_print_buffer(const uint8_t * buffer, int size)
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
