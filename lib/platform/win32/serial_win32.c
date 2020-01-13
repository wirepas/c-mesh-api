/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdlib.h>

int Serial_open(const char * port_name, unsigned long bitrate)
{
    // TODO
    (void) port_name;
    (void) bitrate;
    return -1;
}

int Serial_close(void)
{
    // TODO
    return -1;
}

int Serial_read(unsigned char * c, unsigned int timeout_ms)
{
    // TODO
    (void) c;
    (void) timeout_ms;
    return 0;
}

int Serial_write(const unsigned char * buffer, size_t buffer_size)
{
    // TODO
    (void) buffer;
    (void) buffer_size;
    return 0;
}
