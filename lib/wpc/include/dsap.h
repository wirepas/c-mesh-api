/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef DSAP_H_
#define DSAP_H_

#include <stdint.h>
#include <stdbool.h>
#include "wpc.h"
#include "wpc_constants.h"

typedef struct __attribute__((__packed__))
{
    uint16_t pdu_id;
    uint8_t src_endpoint;
    uint32_t dest_add;
    uint8_t dest_endpoint;
    uint8_t qos;
    uint8_t tx_options;
    uint8_t apdu_length;
    uint8_t apdu[MAX_DATA_PDU_SIZE];
} dsap_data_tx_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint16_t pdu_id;
    uint8_t src_endpoint;
    uint32_t dest_add;
    uint8_t dest_endpoint;
    uint8_t qos;
    uint8_t tx_options;
    uint32_t buffering_delay;
    uint8_t apdu_length;
    uint8_t apdu[MAX_DATA_PDU_SIZE];
} dsap_data_tx_tt_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t indication_status;
    uint16_t pdu_id;
    uint8_t src_endpoint;
    uint32_t dest_add;
    uint8_t dest_endpoint;
    uint32_t buffering_delay;
    uint8_t result;
} dsap_data_tx_ind_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t indication_status;
    uint32_t src_add;
    uint8_t src_endpoint;
    uint32_t dest_add;
    uint8_t dest_endpoint;
    uint8_t qos_hop_count;
    uint32_t travel_time;
    uint8_t apdu_length;
    uint8_t apdu[MAX_DATA_PDU_SIZE];
} dsap_data_rx_ind_pl_t;

typedef struct __attribute__((__packed__))
{
    uint16_t pdu_id;
    uint8_t result;
    uint8_t capacity;
} dsap_data_tx_conf_pl_t;

/**
 * \brief   Function for sending data to the network
 *
 * \param   buffer
 *          the buffer containing the data to send
 * \param   len
 *          the length of the buffer in bytes. It must be lower
 *          or equal than MAX_DATA_PDU_SIZE
 * \param   pdu_id
 *          the pdu id
 * \param   src_ep
 *          the source endpoint
 * \param   dst_add
 *          the destination address
 * \param   dest_ep
 *          the destination endpoint
 * \param   on_data_sent_cb
 *          the callback to call when the packet is sent
 * \param   buffering_delay
 *          initial buffering delay in 1/128 second unit
 * \param   is_unack_csma_ca
 *          is target only CB-MAC nodes
 * \param   hop_limit
 *          hop limitation for this transmission
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int dsap_data_tx_request(const uint8_t * buffer,
                         int len,
                         uint16_t pdu_id,
                         uint32_t dest_add,
                         uint8_t qos,
                         uint8_t src_ep,
                         uint8_t dest_ep,
                         onDataSent_cb_f on_data_sent_cb,
                         uint32_t buffering_delay,
                         bool is_unack_csma_ca,
                         uint8_t hop_limit);

/**
 * \brief   Handler for tx indication. It is called when sent data leaves the
 * node \param   payload Pointer to payload
 */
void dsap_data_tx_indication_handler(dsap_data_tx_ind_pl_t * payload);

/**
 * \brief   Handler for rx indication
 * \param   rx_indication
 *          Pointer to rx indication
 * \param   timestamp
 *          Timestamp of reception of rx reception
 */
void dsap_data_rx_indication_handler(dsap_data_rx_ind_pl_t * rx_indication,
                                     unsigned long long timestamp_ms_epoch);

/**
 * \brief   Register for receiving data on a given EP
 * \param   dst_ep
 *          The destination endpoint to register
 * \param   onDataReceived
 *          The callback to call when data is received
 * \return  True if success, false otherwise
 */
bool dsap_register_for_data(uint8_t dst_ep, onDataReceived_cb_f onDataReceived);

/**
 * \brief   Unregister for receiving data
 * \param   dst_ep
 *          The destination endpoint to unregister
 * \return  True if success, false otherwise
 */
bool dsap_unregister_for_data(uint8_t dst_ep);

/**
 * \brief   Initialize the dsap module
 */
void dsap_init();

#endif /* DSAP_H_ */
