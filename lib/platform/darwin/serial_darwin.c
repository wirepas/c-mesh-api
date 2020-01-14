/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <sys/ioctl.h>          // For ioctl()
#include <IOKit/serial/ioss.h>  // For IOSSIOSPEED
#include <sys/errno.h>          // For errno

#include "posix/serial_posix.h"

#define LOG_MODULE_NAME "serial_darwin"
#define MAX_LOG_LEVEL DEBUG_LOG_LEVEL
#include "logger.h"

int Serial_set_custom_bitrate(int fd, unsigned long bitrate)
{
    // Set custom bitrate on Darwin / macOS
    speed_t speed = bitrate;

    if (ioctl(fd, IOSSIOSPEED, &speed) < 0)
    {
        // Error setting serial port parameters, stop
        LOGE("Error %d from IOSSIOSPEED\n", errno);
        return -1;
    }

    LOGD("Custom bitrate set: %lu\n", bitrate);

    return 0;
}

int Serial_set_extra_params(int fd)
{
    // Nothing to do
    return 0;
}
