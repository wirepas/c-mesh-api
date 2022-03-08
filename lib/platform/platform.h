/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <stdbool.h>
#include "wpc_types.h"

/**
 * \brief   Callback to be called when an indication is received
 * \param   frame
 *          The received indication
 * \param   timestamp_ms
 *          Timestamp of teh received indication
 */
typedef void (*onIndicationReceivedLocked_cb_f)(wpc_frame_t * frame,
                                                unsigned long long timestamp_ms);

/**
 * \brief   Function to retrieved indication
 * \param   max_ind
 *          Maximum number of indication to retrieve in a single poll request
 * \param   cb_locked
 *          Callback to call when an indication is received
 * \return  Negative value if an error happen
 *          0 if successfull and no more indication pending
 *          1 if at least one indication is still pending
 * \note    It is up to the platform implementation to call this method at
 *          the right place from the decided context (polling Thread for example)
 */
typedef int (*Platform_get_indication_f)(unsigned int max_ind, onIndicationReceivedLocked_cb_f cb_locked);

/**
 * \brief   Dispatch a received indication
 * \param   frame
 *          The indication to dispatch
 * \param   timestamp_ms
 *          The timestamp of this indication reception
 * \note    It is up to the platform implementation to call this method at
 *          the right place
 */
typedef void (*Platform_dispatch_indication_f)(wpc_frame_t * frame, unsigned long long timestamp_ms);

/**
 * \brief   Initialization of the platform part
 *          It is up to the platform to initialize all what is required to operate
 *          correctly
 * \param   get_indication_f
 *          function pointer to be called by the platform code periodically or on
 *          event depending on operation mode
 * \param   dispatch_indication_f
 *          function pointer to handle a received indication. It is a distinct call
 *          from previous function to allow a more flexible design
 * \return  true if platform is correctly initialized, false otherwise
 */
bool Platform_init(Platform_get_indication_f get_indication_f,
                   Platform_dispatch_indication_f dispatch_indication_f);


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
