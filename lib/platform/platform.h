/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef PLATFORM_H_
#define PLATFORM_H_

bool Platform_init();

void Platform_usleep(unsigned int time_us);

/**
 * \brief   Get a timestamp in ms since epoch
 * \Note    If this information is not available on the platform,
 *          a different timesatmp can be used and up to upper layer
 *          to convert it later.
 * \return  Timestamp when the call to this function is made
 */
unsigned long long Platform_get_timestamp_ms_epoch();

/**
 * \brief  Call at the beginning of a locked section to send a request
 * \Note   It is up to the platform implementation to see if
 *         this lock must be implemented or not. If all the requests
 *         to the stack are done in the same context, it is useless.
 *         But if poll requests (and indication handling) are not done
 *         from same thread as other API requests, it has to be implemented
 *         accordingly to the architecture chosen.
 */
bool Platform_lock_request();

/**
 *
 * \brief  Called at the end of a locked section to send a request
 */
void Platform_unlock_request();

void Platform_close();

#endif /* PLATFORM_H_ */
