/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <errno.h>
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <asm/termbits.h>
#include <linux/serial.h>

#include "posix/serial_posix.h"

#define LOG_MODULE_NAME "serial_linux"
#define MAX_LOG_LEVEL DEBUG_LOG_LEVEL
#include "logger.h"

int Serial_set_custom_bitrate(int fd, unsigned long bitrate)
{
    struct termios2 tty2;

    if (ioctl(fd, TCGETS2, &tty2) < 0)
    {
        LOGE("Error %d from TCGETS2\n", errno);
        return -1;
    }

    // set custom bitrate
    tty2.c_cflag &= ~CBAUD;
    tty2.c_cflag |= BOTHER;
    tty2.c_ispeed = bitrate;
    tty2.c_ospeed = bitrate;

    if (ioctl(fd, TCSETS2, &tty2) < 0)
    {
        LOGE("Error %d from TCSETS2\n", errno);
        return -1;
    }

    LOGD("Custom bitrate set: %lu\n", bitrate);

    return 0;
}

int Serial_set_extra_params(int fd)
{
    struct serial_struct serial_s;

    // Set low latency flag of serial device. This flag is driver-dependent.
    // On FTDI USB adapters it reduces the latency timer to 1 ms from the
    // default of 16 ms. See Part III of the FTDI application note
    // "AN232B-04 Data Throughput, Latency and Handshaking" for details
    if (ioctl(fd, TIOCGSERIAL, &serial_s) >= 0)
    {
        serial_s.flags |= ASYNC_LOW_LATENCY;
        ioctl(fd, TIOCSSERIAL, &serial_s);
    }

    // Errors are not fatal
    return 0;
}
