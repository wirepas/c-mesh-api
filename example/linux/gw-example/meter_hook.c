/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdio.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#include "wpc.h"

#define LOG_MODULE_NAME "Meter Hook"
#define MAX_LOG_LEVEL DEBUG_LOG_LEVEL
#include "logger.h"

#define MAX_PAYLOAD_SIZE    1500

#define UPLINK_QUEUE_KEY     12345
#define DOWNLINK_QUEUE_KEY   54321

static int m_queue_uplink_id;
static int m_queue_downlink_id;

static bool m_meter_hook_running = false;
static uint32_t m_meter_address = 0;

static app_addr_t m_sink_add = 0;

/* HACK: platform.h should be part of public API too */
extern unsigned long long Platform_get_timestamp_ms_epoch();

/* Define the message structure */
typedef struct {
    long int mtype;
    uint8_t src_ep;
    uint8_t dst_ep;
    uint16_t len;
    uint8_t payload[MAX_PAYLOAD_SIZE];
} message_t;

static bool onDownlinkTrafficReceived_cb(app_message_t * data_p)
{
    int ret;

    // Check if data is for our meter
    if (data_p->dst_addr == m_meter_address)
    {
        // Message is for our meter, propagate it
        message_t msg;
        msg.src_ep = data_p->src_ep;
        msg.dst_ep = data_p->dst_ep;

        memcpy(&msg.payload, data_p->bytes, data_p->num_bytes);
        msg.len = data_p->num_bytes;
        msg.mtype = 1;

        /* Send the message */
        LOGI("Sending buffer of len %d of type %d\n", msg.len, msg.mtype);
        ret = msgsnd(m_queue_downlink_id, &msg, sizeof(message_t) - sizeof(long), 0);
        if (ret < 0)
        {
            LOGE("Cannot send %d, %s\n", ret, strerror(errno));
        }
        return true;
    }
    return false;
}
void Meter_Hook_init(uint32_t meter_address)
{
    m_queue_uplink_id = msgget(UPLINK_QUEUE_KEY, IPC_CREAT | 0666);
    m_queue_downlink_id = msgget(DOWNLINK_QUEUE_KEY, IPC_CREAT | 0666);

    LOGD("Tx key = %d, Rx Key = %d\n", m_queue_uplink_id, m_queue_downlink_id);

    m_meter_address = meter_address;

    // Get the sink address to set the target address
    WPC_get_node_address(&m_sink_add);

    LOGI("m_meter_address=%d sink address = %d\n", m_meter_address, m_sink_add);
}

void Meter_Hook_start(void)
{
    int ret;
    message_t msg;
    m_meter_hook_running = true;
    app_res_e wpc_res;

    WPC_register_downlink_data_hook(onDownlinkTrafficReceived_cb);

    while (m_meter_hook_running)
    {
        LOGD("Waiting on queue\n");
        ret = msgrcv(m_queue_uplink_id, &msg, sizeof(message_t) - sizeof(long), 1, 0);
        if (ret < 0) {
            LOGE("Cannot receive message %d, %s", ret, strerror(errno));
        }
        LOGI("Data to send of len %d %d->%d\n", msg.len, msg.src_ep, msg.dst_ep);

        wpc_res = WPC_inject_uplink_data(
            msg.payload,
            msg.len,
            m_meter_address,
            m_sink_add,
            APP_QOS_HIGH,
            msg.src_ep,
            msg.dst_ep,
            0, // No travel time
            1, // 1 hop count
            Platform_get_timestamp_ms_epoch()
        );

        LOGI("Message injected : %d\n", wpc_res);
    }
}

void Meter_Hook_stop(void)
{
    m_meter_hook_running = false;
    // TODO unblock the msgrcv

    WPC_unregister_downlink_data_hook();
}
