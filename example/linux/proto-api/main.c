/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wpc.h"
#include "wpc_proto.h"

#define PRINT_BUFFERS
#define LOG_MODULE_NAME "Main"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

// Default serial port
static char * port_name = "/dev/ttyACM0";

#define GATEWAY_ID      "GR_proto_gw_001"
#define GATEWAY_MODEL   "GR_proto_gw"
#define GATEWAY_VERSION "0.1"
#define SINK_ID         "sink0"

static uint8_t m_response_buffer[WPC_PROTO_MAX_RESPONSE_SIZE];

/* Dummy DataRequest generated as followed
 * pip install wirepas-mesh-messaging
 * in python :
 * import wirepas_mesh_messaging as wmm
 * message = wmm.SendDataRequest(dest_add=0xc0fe9b57, src_ep=2, dst_ep=3, qos=1, payload=b'test_payload_12345', initial_delay_ms=0, sink_id="Sink0", req_id=1234, is_unack_csma_ca=True, hop_limit=2).payload
 * "0x" + ',0x'.join('{:02x}'.format(x) for x in message ) */
static const uint8_t DUMMY_MESSAGE[]
    = { 0x0a, 0x39, 0x32, 0x37, 0x0a, 0x11, 0x08, 0xd2, 0x09, 0x12, 0x05, 0x53,
        0x69, 0x6e, 0x6b, 0x30, 0x18, 0xcb, 0xf9, 0xff, 0xd5, 0x8b, 0x32, 0x10,
        0xd7, 0xb6, 0xfa, 0x87, 0x0c, 0x18, 0x02, 0x20, 0x03, 0x28, 0x01, 0x32,
        0x12, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61,
        0x64, 0x5f, 0x31, 0x32, 0x33, 0x34, 0x35, 0x40, 0x01, 0x48, 0x02 };

static int open_and_check_connection(unsigned long baudrate,
                                     const char * port_name)
{
    uint16_t mesh_version;
    if (WPC_initialize(port_name, baudrate) != APP_RES_OK)
    {
        LOGE("Cannot open serial sink connection (%s)\n", port_name);
        return -1;
    }

    /* Check the connectivity with sink by reading mesh version */
    if (WPC_get_mesh_API_version(&mesh_version) != APP_RES_OK)
    {
        LOGE("Cannot establish communication with sink with baudrate %d bps\n",
             baudrate);
        WPC_close();
        return -1;
    }

    LOGI("Node is running mesh API version %d (uart baudrate is %d bps)\n",
         mesh_version,
         baudrate);
    return 0;
}

static void onDataRxEvent_cb(uint8_t * event_p,
                             size_t event_size,
                             uint32_t network_id,
                             uint16_t src_ep,
                             uint16_t dst_ep)
{
    LOGI("Event to publish size = %d\n", event_size);
    LOG_PRINT_BUFFER(event_p, event_size);
}

static void onEventStatus_cb(uint8_t * event_p, size_t event_size)
{
    LOGI("Event to publish size = %d\n", event_size);
    LOG_PRINT_BUFFER(event_p, event_size);
}

int main(int argc, char * argv[])
{
    unsigned long   bitrate = DEFAULT_BITRATE;
    app_proto_res_e res;
    size_t          response_size;

    if (argc > 1)
    {
        port_name = argv[1];
    }

    if (argc > 2)
    {
        bitrate = strtoul(argv[2], NULL, 0);
    }

    WPC_Proto_register_for_data_rx_event(onDataRxEvent_cb);
    WPC_Proto_register_for_event_status(onEventStatus_cb);

    if (open_and_check_connection(bitrate, port_name) != 0)
    {
        return -1;
    }

    if (WPC_Proto_initialize(GATEWAY_ID,
                             GATEWAY_VERSION,
                             GATEWAY_MODEL,
                             SINK_ID)
        != APP_RES_PROTO_OK)
    {
        WPC_Proto_close();
        return -1;
    }



    // Example : handle received protobuf
    response_size = sizeof(m_response_buffer);

    res = WPC_Proto_handle_request(DUMMY_MESSAGE,
                                   sizeof(DUMMY_MESSAGE),
                                   m_response_buffer,
                                   &response_size);
    if (res == APP_RES_PROTO_OK)
    {
        LOGI("Response generated of size = %d\n", response_size);
        LOG_PRINT_BUFFER(m_response_buffer, response_size);
    }
    else
    {
        LOGE("Cannot handle request: %d\n", res);
    }


    // Example, stop start stack
    app_res_e res_app;
    res_app = WPC_stop_stack();
    sleep(2);
    if (res_app != APP_RES_OK && res_app != APP_RES_STACK_ALREADY_STOPPED)
    {
        LOGE("Cannot stop stack %d\n", res_app);
        goto exit_on_error;
    }

    LOGI("Stack is stopped\n");

    // // Configure the node
    // if (WPC_set_node_address(NODE_ADDRESS) != APP_RES_OK)
    // {
    //     LOGE("Cannot set node address\n");
    //     goto exit_on_error;
    // }
    // LOGI("Node address set\n");

    // if (WPC_set_network_channel(NETWORK_CHANNEL) != APP_RES_OK)
    // {
    //     LOGE("Cannot set network channel\n");
    //     goto exit_on_error;
    // }
    // LOGI("Network channel set\n");

    // if (WPC_set_role(NODE_ROLE) != APP_RES_OK)
    // {
    //     LOGE("Cannot set node role\n");
    //     goto exit_on_error;
    // }
    // LOGI("Current role set\n");

    // if (WPC_set_network_address(NETWORK_ADDRESS) != APP_RES_OK)
    // {
    //     LOGE("Cannot set network address\n");
    //     goto exit_on_error;
    // }
    // LOGI("Network address set\n");

    // uint8_t config[100] = "Test Mesh API\0";
    // uint8_t seq;
    // uint16_t interval;
    // uint8_t max_size;

    // if (WPC_get_app_config_data_size(&max_size)  != APP_RES_OK)
	// {
	// 	LOGE("Cannot determine app config max size\n");
	// 	goto exit_on_error;
	// }
	// LOGI("App config max size is %d\n", max_size);

    // if (WPC_get_app_config_data(&seq, &interval, config, 100) == APP_RES_NO_CONFIG)
    // {
    //     seq = 1;
    //     interval = 30;
    // }

    // if (NODE_ROLE == APP_ROLE_SINK &&
    //     WPC_set_app_config_data(seq + 1, 30, config, max_size) != APP_RES_OK)
    // {
    //     LOGE("Cannot set config data\n");
    //     goto exit_on_error;
    // }

    // Node is configured, start it
    res_app = WPC_start_stack();
    if (res_app != APP_RES_OK)
    {
        LOGE("Cannot start node error=%d\n", res_app);
        return -1;
    }
    LOGI("Stack is started\n");


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
    
exit_on_error:
    WPC_Proto_close();
    return -1;
}
