/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/serial.h>

#include "serial_termios2.h"

#define LOG_MODULE_NAME "SERIAL"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

static int fd = -1;

static int set_interface_attribs(int fd, unsigned long bitrate, int parity)
{
    struct termios tty;
    struct serial_struct serial_s;

    memset(&tty, 0, sizeof tty);
    if(tcgetattr(fd, &tty) != 0)
    {
        LOGE("Error %d from tcgetattr", errno);
        return -1;
    }

    // default to 9600 bps, but use TCSETS2 to set the actual bitrate
    // use a bitrate that is not 115200 or 125000 bps here, to make sure
    // TCSETS2 actually sets the bitrate
    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    // 8-bit chars
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    // disable break processing
    tty.c_iflag &= ~IGNBRK;
    // disable CR -> NL translation
    tty.c_iflag &= ~ICRNL;
    // no signaling chars, no echo, no canonical processing
    tty.c_lflag = 0;
    // no remapping, no delays
    tty.c_oflag = 0;
    // read doesn't block
    tty.c_cc[VMIN] = 0;
    // No timeout
    tty.c_cc[VTIME] = 0;

    // shut off xon/xoff ctrl
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // ignore modem controls, enable reading
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if(tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        LOGE("Error %d from tcsetattr", errno);
        return -1;
    }

    if(Serial_set_termios2_bitrate(fd, bitrate) != 0)
    {
        return -1;
    }

    // Set low latency flag to serial
    ioctl(fd, TIOCGSERIAL, &serial_s);
    serial_s.flags |= ASYNC_LOW_LATENCY;
    ioctl(fd, TIOCSSERIAL, &serial_s);

    return 0;
}

static void set_blocking(int fd, int should_block)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if(tcgetattr(fd, &tty) != 0)
    {
        LOGE("Error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN] = should_block ? 1 : 0;
    // No timeout
    tty.c_cc[VTIME] = 0;

    if(tcsetattr(fd, TCSANOW, &tty) != 0)
        LOGE("Error %d setting term attributes", errno);
}

/****************************************************************************/
/*                Public method implementation                              */
/****************************************************************************/
int Serial_open(const char * port_name, unsigned long bitrate)
{
    fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC);
    if(fd < 0)
    {
        LOGE("Error %d opening serial link %s: %s\n", errno, port_name, strerror(errno));
        return -1;
    }

    // set the requested bitrate, 8n1, no parity
    if(set_interface_attribs(fd, bitrate, 0) < 0)
    {
        close(fd);
        fd = -1;
        return -1;
    }

    // set no blocking
    set_blocking(fd, 0);

    LOGD("Serial opened\n");
    return 0;
}

int Serial_close()
{
    if(fd < 0)
    {
        LOGW("Link already closed\n");
        return -1;
    }

    if(close(fd) < 0)
    {
        LOGW("Error %d closing serial link: %s\n", errno, strerror(errno));
        return -1;
    }

    fd = -1;
    LOGD("Serial closed\n");
    return 0;
}

int Serial_read(unsigned char * c, unsigned int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    int retval;

    if(fd < 0)
    {
        LOGE("No serial link opened\n");
        return -1;
    }

    /* Watch our fd to see when it has char available */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    /* Set up timeout */
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    /* Block for the given timeout */
    retval = select(fd + 1, &rfds, NULL, NULL, &tv);

    /* Check the reason for exiting the select */
    if(retval > 0)
    {
        /* Useless test as only one single fd monitored */
        if(FD_ISSET(fd, &rfds))
        {
            // LOGD("Data available on serial\n");
            return read(fd, c, 1);
        }
        else
        {
            LOGE("Problem in serial read, fd not set\n");
            return -1;
        }
    }
    else if(retval == 0)
    {
        LOGD("Timeout to wait for char on serial line\n");
        return 0;
    }
    else
    {
        LOGE("Error in Serial read %d\n", retval);
        return -1;
    }
}

int Serial_write(const unsigned char * buffer, unsigned int buffer_size)
{
    if(fd < 0)
    {
        LOGE("No serial link opened\n");
        return -1;
    }

    return write(fd, buffer, buffer_size);
}
