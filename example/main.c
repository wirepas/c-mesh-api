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

#define LOG_MODULE_NAME "Main"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

// Default serial port
char * port_name = "/dev/ttyACM0";

// Node configuration
#define NODE_ADDRESS 0x1
#define NODE_ROLE APP_ROLE_SINK
#define NETWORK_CHANNEL 0x2
#define NETWORK_ADDRESS 0xABCDEF

static bool onDataReceived(const uint8_t * bytes,
                           uint8_t num_bytes,
                           app_addr_t src_addr,
                           app_addr_t dst_addr,
                           app_qos_e qos,
                           uint8_t src_ep,
                           uint8_t dst_ep,
                           uint32_t travel_time,
                           uint8_t hop_count,
                           unsigned long long timestamp_ms_epoch)
{
    // Handle Data received here
    LOGI("Data received on EP %d of len %d with id %d from 0x%x to 0x%x\n",
         dst_ep,
         num_bytes,
         bytes[0],
         src_addr,
         dst_addr);
    return true;
}

static bool onDiagReceived(const uint8_t * bytes,
                           uint8_t num_bytes,
                           app_addr_t src_addr,
                           app_addr_t dst_addr,
                           app_qos_e qos,
                           uint8_t src_ep,
                           uint8_t dst_ep,
                           uint32_t travel_time,
                           uint8_t hop_count,
                           unsigned long long timestamp_ms_epoch)
{
    // Handle diag packet here

    return true;
}

int main(int argc, char * argv[])
{
    app_res_e res;
    unsigned long bitrate = DEFAULT_BITRATE;

    if (argc > 1)
    {
        port_name = argv[1];
    }

    if (argc > 2)
    {
        bitrate = strtoul(argv[2], NULL, 0);
    }

    if (WPC_initialize(port_name, bitrate) != APP_RES_OK)
        return -1;

    res = WPC_stop_stack();
    sleep(2);
    if (res != APP_RES_OK && res != APP_RES_STACK_ALREADY_STOPPED)
    {
        LOGE("Cannot stop stack %d\n", res);
        goto exit_on_error;
    }

    LOGI("Stack is stopped\n");

    // Register for all datas from EP 50
    WPC_register_for_data(50, onDataReceived);
    // Register for all diagnostics from EP 255
    WPC_register_for_data(255, onDiagReceived);

    // Configure the node
    if (WPC_set_node_address(NODE_ADDRESS) != APP_RES_OK)
    {
        LOGE("Cannot set node address\n");
        goto exit_on_error;
    }
    LOGI("Node address set\n");

    if (WPC_set_network_channel(NETWORK_CHANNEL) != APP_RES_OK)
    {
        LOGE("Cannot set network channel\n");
        goto exit_on_error;
    }
    LOGI("Network channel set\n");

    if (WPC_set_role(NODE_ROLE) != APP_RES_OK)
    {
        LOGE("Cannot set node role\n");
        goto exit_on_error;
    }
    LOGI("Current role set\n");

    if (WPC_set_network_address(NETWORK_ADDRESS) != APP_RES_OK)
    {
        LOGE("Cannot set network address\n");
        goto exit_on_error;
    }
    LOGI("Network address set\n");

    uint8_t config[16] = "Test Mesh API\0";
    uint8_t seq;
    uint16_t interval;

    if (WPC_get_app_config_data(&seq, &interval, config, 16) == APP_RES_NO_CONFIG)
    {
        seq = 1;
        interval = 30;
    }

    if (NODE_ROLE == APP_ROLE_SINK &&
        WPC_set_app_config_data(seq + 1, 30, config, 16) != APP_RES_OK)
    {
        LOGE("Cannot set config data\n");
        goto exit_on_error;
    }

    // Node is configured, start it
    res = WPC_start_stack();
    if (res != APP_RES_OK)
    {
        LOGE("Cannot start node error=%d\n", res);
        return -1;
    }
    LOGI("Stack is started\n");

    // Then wait !
    for (;;)
        pause();

exit_on_error:
    WPC_close();
    return -1;
}
