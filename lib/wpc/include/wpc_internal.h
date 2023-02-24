/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef WPC_INTERNAL_H_
#define WPC_INTERNAL_H_

#include "wpc_types.h"

typedef enum
{
    WPC_INT_GEN_ERROR = -1,          //< Generic error code
    WPC_INT_TIMEOUT_ERROR = -2,      //< Timeout error
    WPC_INT_SYNC_ERROR = -3,         //< Synchronization error
    WPC_INT_WRONG_PARAM_ERROR = -4,  //< Wrong parameter
    WPC_INT_WRONG_CRC = -5,          //< Wrong crc
    WPC_INT_WRONG_BUFFER_SIZE = -6,  //< Wrong provided buffer size
    WPC_INT_WRONG_CRC_FROM_HOST = -7,//< Wrong crc detected from host to node (indirect detection)
} WPC_Int_error_code_e;

/**
 * \brief   Function to send a request and wait for confirm for default timeout
 * \param   frame
 *          The request to send
 * \param   confirm
 *          The confirm received by the stack
 * \return  0 if successful or negative value if an error happen
 *
 * \note    This method can be called from different context at the same time
 *          thus, the calling thread can be locked
 */
int WPC_Int_send_request(wpc_frame_t * frame, wpc_frame_t * confirm);

/**
 * \brief   Function to send a request and wait for confirm for given timeout
 * \param   frame
 *          The request to send
 * \param   confirm
 *          The confirm received by the stack
 * \param   timeout_ms
 *          Timeout to wait for confirm in ms
 * \return  0 if successful or negative value if an error happen
 *
 * \note    This method can be called from different context at the same time
 *          thus, the calling thread can be locked
 */
int WPC_Int_send_request_timeout(wpc_frame_t * frame, wpc_frame_t * confirm, uint16_t timeout_ms);

/**
 * \brief   Disable/Enable the poll requests
 * \param   disabled
 *          true to disable the poll request
 *          false to enable it again
 * \Note    It is mainly in case of periodic polling to be disabled
 *          when we know that sink is not able to answer (when rebooting node for example)
 */
void WPC_Int_disable_poll_request(bool disabled);

/**
 * \brief   Set timeout to consider the node lost (ie no answer)
 * \param   duration_s
 *          maximum duration the node can stay silent
 * \return  True if success
 */
bool WPC_Int_set_timeout_s_no_answer(unsigned int duration_s);

int WPC_Int_initialize(const char * port_name, unsigned long bitrate);

void WPC_Int_close(void);

#endif
