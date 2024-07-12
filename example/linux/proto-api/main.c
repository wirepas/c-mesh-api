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

    WPC_register_for_data_rx_event(onDataRxEvent_cb);

    pause();
}
