/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

/**
 * \file    serial.h
 *          Low level serial interface. Used for transmitting and
 *          receiving data via UART.
 */

/**
 * \brief   Open a serial link to the Wirepas Mesh MCU
 * \param   port_name
 *          Name of the port as enumerated on the platform
 * \param   bitrate
 *          Bitrate in bits per second, typically 115200 or 125000
 * \return  0 if success, -1 otherwise
 */
int Serial_open(const char * port_name, unsigned long bitrate);

/**
 * \brief   Close a serial link previously opened with Serial_open
 * \return  0 if success, -1 otherwise
 */
int Serial_close(void);

/**
 * \brief   Read data from the serial link
 * \param   c
 *          the buffer to store read char
 * \param   timeout_ms
 *          timeout in ms to receive char
 * \return  the number of bytes read, 0 in case of timeout or -1 in case of
 * error
 */
int Serial_read(unsigned char * c, unsigned int timeout_ms);

/**
 * \brief   Write data from the serial link
 * \param   buffer
 *          The buffer for the chars to write
 * \param   buffer_size
 *          The size of the buffer to write
 * \return  The number of written char or a negative value in case of error
 */
int Serial_write(const unsigned char * buffer, unsigned int buffer_size);
