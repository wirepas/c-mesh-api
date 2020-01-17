/**
 * \brief   Set custom serial bitrate
 * \param   fd
 *          The file descriptor of serial device
 * \param   bitrate
 *          Custom bitrate to set, in bps
 * \return  0 if custom bitrate was set, -1 if not
 */
int Serial_set_custom_bitrate(int fd, unsigned long bitrate);

/**
 * \brief   Write data from the serial link
 * \param   fd
 *          The file descriptor of serial device
 * \return  0 if all is well, -1 if not
 */
int Serial_set_extra_params(int fd);
