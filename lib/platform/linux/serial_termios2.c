/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <errno.h>
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <asm/termbits.h>

#define LOG_MODULE_NAME "SERIAL"
#define MAX_LOG_LEVEL DEBUG_LOG_LEVEL
#include "logger.h"

int Serial_set_termios2_bitrate(int fd, unsigned long bitrate)
{
    struct termios2 tty2;

    if (ioctl(fd, TCGETS2, &tty2) < 0)
    {
        LOGE("Error %d from TCGETS2", errno);
        return -1;
    }

    // set custom bitrate
    tty2.c_cflag &= ~CBAUD;
    tty2.c_cflag |= BOTHER;
    tty2.c_ispeed = bitrate;
    tty2.c_ospeed = bitrate;

    if (ioctl(fd, TCSETS2, &tty2) < 0)
    {
        LOGE("Error %d from TCSETS2", errno);
        return -1;
    }

    LOGD("Custom bitrate set: %lu\n", bitrate);

    return 0;
}
