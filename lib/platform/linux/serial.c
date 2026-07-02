/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <time.h>
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <asm/termbits.h>

#include "platform.h"

#define LOG_MODULE_NAME "SERIAL"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

static int fd = -1;

/** \brief Forward declaration of internal open */
static int int_open();

/** \brief  Port name to open */
static char m_port_name[256];

/** \brief Bitrate to use */
static unsigned long m_bitrate;

static int set_interface_attribs(int fd, unsigned long bitrate, int parity)
{
    struct termios2 tty;
    struct serial_struct serial_s;

    if (ioctl(fd, TCGETS2, &tty) < 0)
    {
        LOGE("Error %d from TCGETS2\n", errno);
        return -1;
    }

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

    // shut off xon/xoff ctrl
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // ignore modem controls, enable reading
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_cflag &= ~CBAUD;
    tty.c_cflag |= BOTHER;
    tty.c_ispeed = bitrate;
    tty.c_ospeed = bitrate;

    if (ioctl(fd, TCSETS2, &tty) < 0)
    {
        LOGE("Error %d from TCSETS2\n", errno);
        return -1;
    }

    LOGD("Custom bitrate set: %lu\n", bitrate);

    // Set low latency flag to serial
    ioctl(fd, TIOCGSERIAL, &serial_s);
    serial_s.flags |= ASYNC_LOW_LATENCY;
    ioctl(fd, TIOCSSERIAL, &serial_s);

    return 0;
}

static int int_open()
{
    fd = open(m_port_name, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (fd < 0)
    {
        LOGE("Error %d opening serial link %s: %s\n", errno, m_port_name, strerror(errno));
        return -1;
    }

    // set the requested bitrate, 8n1, no parity
    if (set_interface_attribs(fd, m_bitrate, 0) < 0)
    {
        close(fd);
        fd = -1;
        return -1;
    }

    LOGD("Serial opened\n");
    return 0;
}
/****************************************************************************/
/*                Public method implementation                              */
/****************************************************************************/
int Serial_open(const char * port_name, unsigned long bitrate)
{
    // Copy the settings locally
    strcpy(m_port_name, port_name);
    m_bitrate = bitrate;

    return int_open();
}

int Serial_close()
{
    if (fd < 0)
    {
        LOGW("Link already closed\n");
        return -1;
    }

    if (close(fd) < 0)
    {
        LOGW("Error %d closing serial link: %s\n", errno, strerror(errno));
        return -1;
    }

    fd = -1;
    LOGD("Serial closed\n");
    return 0;
}

/*
 * \brief     Get timespec representing the deadline in the future
 * \param[in] timeout_ms
 *            Timeout in milliseconds
 * \return    On success: returns a timespec for the time in future
 *            On failure: Logs a warning and returns a zero-filled dummy timespec
 */
static struct timespec get_deadline(const unsigned int timeout_ms)
{
    struct timespec deadline = {0};

    struct timespec now;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &now))
    {
        LOGW("Could not get current time, will have invalid deadline: %s\n", strerror(errno));
        return deadline;
    }

    const time_t timeout_s = timeout_ms / 1000;
    const int_fast32_t timeout_nsec = (timeout_ms % 1000) * 1000000;

    deadline.tv_sec = now.tv_sec + timeout_s;
    deadline.tv_nsec = now.tv_nsec + timeout_nsec;
    if (deadline.tv_nsec > 999999999)
    {
        deadline.tv_nsec -= 1000000000;
        deadline.tv_sec++;
    }

    return deadline;
}

/*
 * \brief     Get remaining time in milliseconds based on the deadline
 * \param[in] deadline
 *            Deadline
 * \return    If deadline has not passed: Returns remaining time in milliseconds
 *            If deadline has passed: Silently returns zero
 *            On failure: logs a warning and returns zero
 */
static int get_remaining_time_ms(const struct timespec *const deadline)
{
    struct timespec now;
    if (0 != clock_gettime(CLOCK_MONOTONIC, &now))
    {
        LOGW("Could not get current time, will assume deadline has passed: %s\n", strerror(errno));
        return 0;
    }

    if ((deadline->tv_sec < now.tv_sec) ||
        (deadline->tv_sec == now.tv_sec && deadline->tv_nsec < now.tv_nsec))
    {
        return 0;
    }

    time_t remaining_sec = deadline->tv_sec - now.tv_sec;
    int_fast32_t remaining_nsec = deadline->tv_nsec - now.tv_nsec;
    if (remaining_nsec < 0)
    {
        remaining_sec--;
        remaining_nsec += 1000000000;

    }

    return (remaining_sec * 1000) + (remaining_nsec / 1000000);
}

/*
 * \brief     Poll the serial file descriptor with the given timeout
 *
 * Polls the serial file descriptor with the given timeout. If poll fails with
 * EINTR due to a signal, retries again with the remaining timeout.
 *
 * \param[in] timeout_ms
 *            Timeout in milliseconds
 * \return    If an event occured on the file descriptor: Returns true
 *            On timeout: Silently returns false
 *            On failure: Logs an error and returns false
 */
static bool poll_serial_with_timeout(const unsigned int timeout_ms)
{
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN
    };

    const struct timespec deadline = get_deadline(timeout_ms);

    int res;
    bool interrupted;
    int remaining = get_remaining_time_ms(&deadline);
    do
    {
        res = poll(&pfd, 1, remaining);
        interrupted = (res < 0 && errno == EINTR);
        if (interrupted)
        {
            remaining = get_remaining_time_ms(&deadline);
            if (remaining <= 0)
            {
                res = 0;
                break;
            }
        }
    } while (interrupted);

    if (res < 0)
    {
        LOGE("Error when waiting for char on serial line: %s\n", strerror(errno));
        return false;
    }

    return res > 0;
}

/*
 * \brief      Get a single byte from the serial line
 *
 * Returns the next buffered byte if one is available. Otherwise waits up to
 * the given timeout for data on the serial line, refills the internal buffer
 * with a single read and returns the first byte from it.
 *
 * \param[out] c
 *             Pointer where the read byte is stored
 * \param[in]  timeout_ms
 *             Timeout in milliseconds to wait
 * \return     If a byte was read: Returns 1
 *             On timeout or if no data could be read: Returns 0
 */
static ssize_t get_single_char(unsigned char * c, unsigned int timeout_ms)
{
    static unsigned char m_buffer[128];
    static uint8_t m_elem = 0;
    static uint8_t m_read = 0;
    ssize_t read_bytes;

    // Do we still have char buffered?
    if (m_elem > 0)
    {
        *c = m_buffer[m_read++];
        m_elem--;
        return 1;
    }

    if (!poll_serial_with_timeout(timeout_ms))
    {
        LOGD("Timeout to wait for char on serial line\n");
        return 0;
    }

    read_bytes = read(fd, m_buffer, sizeof(m_buffer));
    if (read_bytes > 0)
    {
        m_elem = read_bytes;
        m_read = 0;
        *c = m_buffer[m_read++];
        m_elem--;
        return 1;
    }

    if (read_bytes < 0)
    {
        LOGD("Reading the serial device failed, but treating it as a timeout: %s\n", strerror(errno));
    }

    return 0;
}

int Serial_read(unsigned char * c, unsigned int timeout_ms)
{
    if (fd < 0)
    {
        LOGE("No serial link opened\n");
        return -1;
    }

    return get_single_char(c, timeout_ms);
}

int Serial_write(const unsigned char * buffer, unsigned int buffer_size)
{
    if (fd < 0)
    {
        LOGE("No serial link opened\n");
        // Try to reopen
        if (int_open() < 0)
        {
            // Wait a bit before next try
            usleep(1000 * 1000);
            return 0;
        }
        LOGI("Serial reopened\n");
    }

    ssize_t ret = write(fd, buffer, buffer_size);
    if (ret < 0)
    {
        LOGE("Error in write: %d\n", errno);
        LOGE("Close connection\n");
        Serial_close();

        // Connection will be checked reopen at next try

        return 0;
    }
    return ret;
}
