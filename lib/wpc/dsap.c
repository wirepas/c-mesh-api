/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#define LOG_MODULE_NAME "dsap"
#define MAX_LOG_LEVEL WARNING_LOG_LEVEL
#include "logger.h"
#include "wpc_types.h"
#include "wpc_internal.h"
#include "util.h"
#include "platform.h"

#include "string.h"

// Table to store the registered client callbacks for Rx data
static onDataReceived_cb_f data_cb_table[MAX_NUMBER_EP];

typedef struct
{
    onDataSent_cb_f cb;
    uint16_t pdu_id;
    bool busy;
} packet_with_indication_t;

// Table to store the data sent callbacks status for Tx data
static packet_with_indication_t indication_sent_cb_table[MAX_SENT_PACKET_WITH_INDICATION];

static bool set_indication_cb(onDataSent_cb_f cb, uint16_t pdu_id)
{
    int i;

    for (i = 0; i < MAX_SENT_PACKET_WITH_INDICATION; i++)
    {
        if (!indication_sent_cb_table[i].busy)
        {
            indication_sent_cb_table[i].busy = true;
            indication_sent_cb_table[i].cb = cb;
            indication_sent_cb_table[i].pdu_id = pdu_id;
            break;
        }
    }

    return i < MAX_SENT_PACKET_WITH_INDICATION;
}

static onDataSent_cb_f get_indication_cb(uint16_t pdu_id)
{
    onDataSent_cb_f cb = NULL;

    for (int i = 0; i < MAX_SENT_PACKET_WITH_INDICATION; i++)
    {
        if (indication_sent_cb_table[i].busy && indication_sent_cb_table[i].pdu_id == pdu_id)
        {
            // Release entry
            indication_sent_cb_table[i].busy = false;
            cb = indication_sent_cb_table[i].cb;
        }
    }

    return cb;
}

static void fill_tx_request(wpc_frame_t * request,
                            const uint8_t * buffer,
                            int len,
                            uint16_t pdu_id,
                            uint32_t dest_add,
                            uint8_t qos,
                            uint8_t src_ep,
                            uint8_t dest_ep,
                            uint8_t tx_options)
{
    dsap_data_tx_req_pl_t * payload = &request->payload.dsap_data_tx_request_payload;

    payload->pdu_id = pdu_id;
    payload->src_endpoint = src_ep;
    payload->dest_add = dest_add;
    payload->dest_endpoint = dest_ep;
    payload->qos = qos;
    payload->tx_options = tx_options;
    payload->apdu_length = len;

    // copy the APDU
    memcpy(&payload->apdu, buffer, len);
}

static void fill_tx_tt_request(wpc_frame_t * request,
                               const uint8_t * buffer,
                               int len,
                               uint16_t pdu_id,
                               uint32_t dest_add,
                               uint8_t qos,
                               uint8_t src_ep,
                               uint8_t dest_ep,
                               uint8_t tx_options,
                               uint32_t buffering_delay)
{
    dsap_data_tx_tt_req_pl_t * payload = &request->payload.dsap_data_tx_tt_request_payload;

    payload->pdu_id = pdu_id;
    payload->src_endpoint = src_ep;
    payload->dest_add = dest_add;
    payload->dest_endpoint = dest_ep;
    payload->qos = qos;
    payload->tx_options = tx_options;
    payload->apdu_length = len;
    payload->buffering_delay = buffering_delay;

    // copy the APDU
    memcpy(&payload->apdu, buffer, len);
}

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
                         uint8_t hop_limit)
{
    wpc_frame_t request, confirm;
    int res;
    uint8_t confirm_res;
    uint8_t tx_options = 0;

    if (len > MAX_DATA_PDU_SIZE)
        return -1;

    // Fill the tx options
    if (on_data_sent_cb != NULL)
    {
        tx_options |= 0x1;
    }

    // Is it a unack_csma_ca transmission
    if (is_unack_csma_ca)
    {
        tx_options |= 0x2;
    }

    // Add hop limit (on 4 bits)
    tx_options |= (hop_limit & 0xf) << 2;

    // Create the frame
    if (buffering_delay == 0)
    {
        request.primitive_id = DSAP_DATA_TX_REQUEST;
        fill_tx_request(&request, buffer, len, pdu_id, dest_add, qos, src_ep, dest_ep, tx_options);
        request.payload_length =
            sizeof(dsap_data_tx_req_pl_t) - (MAX_DATA_PDU_SIZE - len);
    }
    else
    {
        request.primitive_id = DSAP_DATA_TX_TT_REQUEST;
        fill_tx_tt_request(&request, buffer, len, pdu_id, dest_add, qos, src_ep, dest_ep, tx_options, buffering_delay);
        request.payload_length =
            sizeof(dsap_data_tx_tt_req_pl_t) - (MAX_DATA_PDU_SIZE - len);
    }

    // Do the sending
    res = WPC_Int_send_request(&request, &confirm);

    if (res < 0)
        return res;

    confirm_res = confirm.payload.dsap_data_tx_confirm_payload.result;

    // If success, register the callback
    if (confirm_res == 0 && on_data_sent_cb != NULL)
    {
        set_indication_cb(on_data_sent_cb, pdu_id);
    }

    LOGI("Send data result = 0x%02x capacity = %d \n",
         confirm_res,
         confirm.payload.dsap_data_tx_confirm_payload.capacity);

    return confirm_res;
}

void dsap_data_tx_indication_handler(dsap_data_tx_ind_pl_t * payload)
{
    onDataSent_cb_f cb = get_indication_cb(payload->pdu_id);

    LOGD("Tx indication received: indication_status = %d, buffering_delay = "
         "%d\n",
         payload->indication_status,
         payload->buffering_delay);
    if (cb != NULL)
    {
        LOGD("App cb set, call it...\n");
        cb(payload->pdu_id,
           internal_time_to_ms(payload->buffering_delay),
           payload->indication_status);
    }
}

void dsap_data_rx_indication_handler(dsap_data_rx_ind_pl_t * payload,
                                     unsigned long long timestamp_ms_epoch)
{
    uint32_t internal_travel_time = uint32_decode_le((uint8_t *) &(payload->travel_time));
    app_qos_e qos;
    uint8_t hop_count;

    LOGI("Data received: indication_status = %d, src_add = %d, lenght=%u, "
         "travel time = %d, dst_ep = %d, ts=%llu\n",
         payload->indication_status,
         payload->src_add,
         payload->apdu_length,
         internal_travel_time,
         payload->dest_endpoint,
         timestamp_ms_epoch);

    if (data_cb_table[payload->dest_endpoint] != 0)
    {
        // Create the qos
        qos = APP_QOS_NORMAL;
        if (payload->qos_hop_count & 0x01)
        {
            qos = APP_QOS_HIGH;
        }

        // Get the number of hops
        hop_count = payload->qos_hop_count >> 2;

        // Someone is registered on this endpoint
        data_cb_table[payload->dest_endpoint](payload->apdu,
                                              payload->apdu_length,
                                              payload->src_add,
                                              payload->dest_add,
                                              qos,
                                              payload->src_endpoint,
                                              payload->dest_endpoint,
                                              internal_time_to_ms(internal_travel_time),
                                              hop_count,
                                              timestamp_ms_epoch);
    }
}

bool dsap_register_for_data(uint8_t dst_ep, onDataReceived_cb_f onDataReceived)
{
    bool ret = false;

    Platform_lock_request();
    if (dst_ep < MAX_NUMBER_EP && data_cb_table[dst_ep] == NULL)
    {
        data_cb_table[dst_ep] = onDataReceived;
        ret = true;
    }
    Platform_unlock_request();

    return ret;
}

bool dsap_unregister_for_data(uint8_t dst_ep)
{
    bool ret = false;

    Platform_lock_request();
    if (dst_ep < MAX_NUMBER_EP && data_cb_table[dst_ep] != NULL)
    {
        data_cb_table[dst_ep] = NULL;
        ret = true;
    }
    Platform_unlock_request();

    return ret;
}

void dsap_init()
{
    // Initialize internal structures
    memset(data_cb_table, 0, sizeof(data_cb_table));
    memset(indication_sent_cb_table, 0, sizeof(indication_sent_cb_table));
}
