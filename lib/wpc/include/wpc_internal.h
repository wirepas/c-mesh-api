/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef WPC_INTERNAL_H_
#define WPC_INTERNAL_H_

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
 * \brief   Function to send a request
 * \param   frame
 *          The request to send
 * \param   confirm
 *          The confirm received by the stack
 * \return  0 if successful or negative value if an error happen
 *
 * \note    This method can be called from different context at the same time
 *          or before the last is acknowledged from the stack. Thus, the calling
 *          thread can be locked
 */
int WPC_Int_send_request(wpc_frame_t *frame,
                         wpc_frame_t *confirm);

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
int WPC_Int_get_indication(unsigned int max_ind,
                           onIndicationReceivedLocked_cb_f cb_locked);

/**
 * \brief   Dispatch a received indication
 * \param   frame
 *          The indication to dispatch
 * \param   timestamp_ms
 *          The timestamp of this indication reception
 * \note    It is up to the platform implementation to call this method at
 *          the right place
 */
void WPC_Int_dispatch_indication(wpc_frame_t * frame,
                                 unsigned long long timestamp_ms);

int WPC_Int_initialize(char * port_name, unsigned long bitrate);

void WPC_Int_close(void);

#endif
