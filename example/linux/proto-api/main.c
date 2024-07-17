/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wpc_proto.h"

#define PRINT_BUFFERS
#define LOG_MODULE_NAME "Main"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

// Default serial port
static char * port_name = "/dev/ttyACM0";
#define DEFAULT_BITRATE 1000000

#define GATEWAY_ID "GR_poto_gw"
#define SINK_ID "sink0"

static uint8_t m_response_buffer[WPC_PROTO_MAX_RESPONSE_SIZE];

/* Dummy DataRequest generated as followed */
/* message = wmm.SendDataRequest(dest_add=0xc0fe9b57, src_ep=2, dst_ep=3, qos=1, payload=b'test_payload_12345', initial_delay_ms=0, sink_id="Sink0", req_id=1234, is_unack_csma_ca=True, hop_limit=2).payload */
/* "0x" + ',0x'.join( '{:02x}'.format(x) for x in message )*/
static const uint8_t DUMMY_MESSAGE[] = {0x0a,0x39,0x32,0x37,0x0a,0x11,0x08,0xd2,0x09,0x12,0x05,0x53,0x69,0x6e,0x6b,0x30,0x18,0xcb,0xf9,0xff,0xd5,0x8b,0x32,0x10,0xd7,0xb6,0xfa,0x87,0x0c,0x18,0x02,0x20,0x03,0x28,0x01,0x32,0x12,0x74,0x65,0x73,0x74,0x5f,0x70,0x61,0x79,0x6c,0x6f,0x61,0x64,0x5f,0x31,0x32,0x33,0x34,0x35,0x40,0x01,0x48,0x02};

static void onDataRxEvent_cb(uint8_t * event_p,
                            size_t event_size,
                            uint32_t network_id,
                            uint16_t src_ep,
                            uint16_t dst_ep)
{
    LOGI("Event to publish size = %d\n", event_size);
    LOG_PRINT_BUFFER(event_p, event_size);
}

int main(int argc, char * argv[])
{
    unsigned long bitrate = DEFAULT_BITRATE;
    app_proto_res_e res;
    size_t response_size;

    if (argc > 1)
    {
        port_name = argv[1];
    }

    if (argc > 2)
    {
        bitrate = strtoul(argv[2], NULL, 0);
    }

    if (WPC_Proto_initialize(port_name,
                             bitrate,
                             GATEWAY_ID,
                             SINK_ID) != APP_RES_PROTO_OK)

        return -1;

    WPC_Proto_register_for_data_rx_event(onDataRxEvent_cb);

    response_size = sizeof(m_response_buffer);

    res = WPC_Proto_handle_request(DUMMY_MESSAGE, sizeof(DUMMY_MESSAGE), m_response_buffer, &response_size);
    if (res == APP_RES_PROTO_OK)
    {
        LOGI("Response generated of size = %d\n", response_size);
        LOG_PRINT_BUFFER(m_response_buffer, response_size);
    }
    else
    {
        LOGE("Cannot handle request: %d\n", res);
    }


    pause();
}
