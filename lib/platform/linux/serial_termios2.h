/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

/**
 * \file    serial_termios2.h
 *          A function to set a custom bitrate on Linux, using struct termios2.
 *          This needs to be in its own file due to Linux header file issues.
 */

int Serial_set_termios2_bitrate(int fd, unsigned long bitrate);
