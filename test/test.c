/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdio.h>
#include <string.h>

#define LOG_MODULE_NAME "Test"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "wpc.h"
#include "platform.h"

static bool setInitialState(app_role_t role,
                            app_addr_t id,
                            net_addr_t network_add,
                            net_channel_t network_channel,
                            bool start)
{
    WPC_stop_stack();

    if (WPC_set_node_address(id) != APP_RES_OK)
        return false;

    if (WPC_set_role(role) != APP_RES_OK)
        return false;

    if (WPC_set_network_address(network_add) != APP_RES_OK)
        return false;

    if (WPC_set_network_channel(network_channel) != APP_RES_OK)
        return false;

    if (start && (WPC_start_stack() == APP_RES_OK))
        return false;

    return true;
}

static bool testFactoryReset()
{
    app_res_e res = WPC_stop_stack();
    app_addr_t add;
    if (res != APP_RES_STACK_ALREADY_STOPPED && res != APP_RES_OK)
    {
        LOGE("Cannot stop stack\n");
        return false;
    }

    if (WPC_set_node_address(0x123456) != APP_RES_OK)
    {
        LOGE("Cannot set node address\n");
        return false;
    }

    if (WPC_do_factory_reset() != APP_RES_OK)
    {
        LOGE("Cannot do factory reset\n");
        return false;
    }

    // Check node address previously set is correctly removed
    if (WPC_get_node_address(&add) != APP_RES_ATTRIBUTE_NOT_SET)
    {
        LOGE("Node address is still set\n");
        return false;
    }

    return true;
}

static bool setAppconfig(uint8_t * config, uint16_t interval, uint8_t size)
{
    uint8_t cur_seq, new_seq = 0;
    uint16_t cur_interval;
    uint8_t cur_config[128];

    if (size > 128)
    {
        LOGE("Specified size is too big\n");
        return false;
    }

    if (WPC_get_app_config_data(&cur_seq, &cur_interval, cur_config, size) == APP_RES_OK)
    {
        new_seq = cur_seq + 1;
    }

    if (WPC_set_app_config_data(new_seq, interval, config, size) != APP_RES_OK)
    {
        LOGE("Cannot set new app config with seq = %d\n", new_seq);
        return false;
    }

    // Read back App config
    if (WPC_get_app_config_data(&cur_seq, &cur_interval, cur_config, size) != APP_RES_OK)
    {
        LOGE("Cannot read back new app config\n", new_seq);
        return false;
    }

    if (memcmp(config, cur_config, size) != 0)
    {
        LOGE("App config set differs from app config read\n", new_seq);
        return false;
    }

    return true;
}

static bool dumpCSAPAttributes()
{
    uint8_t role;
    net_channel_t channel;
    app_addr_t node_address;
    net_addr_t network;
    uint8_t mtu, pdus, scratch_seq, first_channel, last_channel, data_size;
    uint16_t firmware[4], api_v, hw_magic, stack_profile;

    if (WPC_get_role(&role) == APP_RES_ATTRIBUTE_NOT_SET)
    {
        LOGW("Role not set\n");
        role = 0;
    }

    if (WPC_get_node_address(&node_address) == APP_RES_ATTRIBUTE_NOT_SET)
    {
        LOGW("Node address not set\n");
        node_address = 0;
    }

    if (WPC_get_network_address(&network) == APP_RES_ATTRIBUTE_NOT_SET)
    {
        LOGW("Network address not set\n");
        network = 0;
    }

    if (WPC_get_network_channel(&channel) == APP_RES_ATTRIBUTE_NOT_SET)
    {
        LOGW("Network channel not set\n");
        channel = 0;
    }

    LOGI("Role is 0x%02x / Add is %d / Network is 0x%06x (channel = %d)\n", role, node_address, network, channel);

    WPC_get_mtu(&mtu);
    LOGI("MTU size is %d\n", mtu);

    WPC_get_pdu_buffer_size(&pdus);
    LOGI("PDU size is %d\n", pdus);

    WPC_get_scratchpad_sequence(&scratch_seq);
    LOGI("Scratchpad seq is %d\n", scratch_seq);

    WPC_get_mesh_API_version(&api_v);
    LOGI("Mesh API version is %d\n", api_v);

    WPC_get_firmware_version(firmware);
    LOGI("Firmware version is %d:%d:%d:%d\n", firmware[0], firmware[1], firmware[2], firmware[3]);

    WPC_get_channel_limits(&first_channel, &last_channel);
    LOGI("Channel Limits are: [%d - %d]\n", first_channel, last_channel);

    WPC_get_app_config_data_size(&data_size);
    LOGI("Max config data size is %d\n", data_size);

    WPC_get_hw_magic(&hw_magic);
    LOGI("Radio hardware is %d\n", hw_magic);

    WPC_get_stack_profile(&stack_profile);
    LOGI("Stack profile is %d\n", stack_profile);

    return true;
}

static bool testCipherKey()
{
    bool key_set;
    if (WPC_is_cipher_key_set(&key_set) != APP_RES_OK)
    {
        return false;
    }

    if (key_set)
    {
        LOGW("Key already set\n");
        // return false
    }

    LOGI("No cipher key\n");
    uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    LOGI("Set cipher key\n");
    if (WPC_set_cipher_key(key) != APP_RES_OK)
    {
        LOGE("Cannot set key\n");
        return false;
    }

    LOGI("Check cipher key set\n");
    if (WPC_is_cipher_key_set(&key_set) != APP_RES_OK)
    {
        return false;
    }

    if (!key_set)
    {
        LOGE("Key is not set\n");
        return false;
    }

    LOGI("Remove cipher key\n");
    if (WPC_remove_cipher_key() != APP_RES_OK)
    {
        LOGE("Cannot remove key\n");
        return false;
    }

    LOGI("Check cipher key not set\n");
    if (WPC_is_cipher_key_set(&key_set) != APP_RES_OK)
    {
        return false;
    }

    if (key_set)
    {
        LOGE("Key is still set\n");
        return false;
    }

    return true;
}

static bool testAuthenticationKey()
{
    bool key_set;
    if (WPC_is_authentication_key_set(&key_set) != APP_RES_OK)
    {
        return false;
    }

    if (key_set)
    {
        LOGW("Key already set\n");
        // return false
    }

    LOGI("No authentication key\n");
    uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    LOGI("Set authentication key\n");
    if (WPC_set_authentication_key(key) != APP_RES_OK)
    {
        LOGE("Cannot set key\n");
        return false;
    }

    LOGI("Check authentication key set\n");
    if (WPC_is_authentication_key_set(&key_set) != APP_RES_OK)
    {
        return false;
    }

    if (!key_set)
    {
        LOGE("Key is not set\n");
        return false;
    }

    LOGI("Remove authentication key\n");
    if (WPC_remove_authentication_key() != APP_RES_OK)
    {
        LOGE("Cannot remove key\n");
        return false;
    }

    LOGI("Check authentication key not set\n");
    if (WPC_is_authentication_key_set(&key_set) != APP_RES_OK)
    {
        return false;
    }

    if (key_set)
    {
        LOGE("Key is still set\n");
        return false;
    }

    return true;
}

static bool testScratchpadTarget()
{
    uint8_t target_seq;
    uint16_t target_crc;
    uint8_t action;
    uint8_t param;

    // Read it a first time
    if (WPC_read_target_scratchpad(&target_seq,
                                   &target_crc,
                                   &action,
                                   &param) != APP_RES_OK)
    {
        LOGE("Cannot read target scratchpad\n");
        return false;
    }

    LOGI("Target is: %d, 0x%x, %d, %d\n", target_seq, target_crc, action, param);

    app_res_e res = WPC_write_target_scratchpad(12,
            0x1234,
            2,
            13);
    if (res != APP_RES_OK)
    {
        LOGE("Cannot write target scratchpad %d\n", res);
        return false;
    }

    LOGI("Write new target\n");

    if (WPC_read_target_scratchpad(&target_seq,
                                   &target_crc,
                                   &action,
                                   &param))
    {
        LOGE("Cannot read target back scratchpad\n");
        return false;
    }

    LOGI("Target read back is: %d, 0x%x, %d, %d\n", target_seq, target_crc, action, param);

    if (target_seq != 12 || target_crc != 0x1234 || action != 2 || param != 13)
    {
        LOGE("Wrong read-back value\n");
        return false;
    }

    return true;
}

void onDataSent(uint16_t pduid, uint32_t buffering_delay, uint8_t result)
{
    LOGI("Indication received for %d, delay=%d, result=%d\n", pduid, buffering_delay, result);
}

static bool testSendWithCallbacks()
{
    // Send 5 message to ourself
    uint8_t data[] = "This is a test message #00 with ind\0";
    for (int i = 0; i < 2; i++)
    {
        data[24] = i / 10 + 0x30;
        data[25] = i % 10 + 0x30;
        if (WPC_send_data(data, sizeof(data), i, APP_ADDR_ANYSINK, APP_QOS_HIGH, 50, 50, onDataSent, 0) !=
            APP_RES_OK)
        {
            return false;
        }
    }
    return true;
}

static bool testSendWithoutCallbacks()
{
    // Send 5 message to ourself
    uint8_t data[] = "This is a test message #00\0";
    for (int i = 2; i < 4; i++)
    {
        data[24] = i / 10 + 0x30;
        data[25] = i % 10 + 0x30;
        if (WPC_send_data(data, sizeof(data), i, APP_ADDR_ANYSINK, APP_QOS_HIGH, 50, 50, NULL, 0) != APP_RES_OK)
        {
            return false;
        }
    }
    return true;
}

static bool testSendWithInitialTime()
{
    // Send 5 message to ourself
    uint8_t data[] = "This is a test message #00\0";
    for (int i = 4; i < 6; i++)
    {
        data[24] = i / 10 + 0x30;
        data[25] = i % 10 + 0x30;
        if (WPC_send_data(data, sizeof(data), i, APP_ADDR_ANYSINK, APP_QOS_HIGH, 50, 50, NULL, 300) !=
            APP_RES_OK)
        {
            return false;
        }
    }
    return true;
}

static void onAppConfigDataReceived(uint8_t seq, uint16_t interval, uint8_t * config_p)
{
    LOGI("AppConfig received %d, %d, %s\n", seq, interval, config_p);
}

static bool testAppConfigData()
{
    uint8_t new_config[128];
    uint8_t max_size;

    if (WPC_get_app_config_data_size(&max_size) != APP_RES_OK || (max_size > 128))
    {
        LOGE("Cannot get max app config size of bigger than 128 bytes\n");
        return false;
    }

    for (uint8_t i = 0; i < max_size; i++)
    {
        new_config[i] = i;
    }

    if (!setAppconfig(new_config, 30, max_size))
    {
        LOGE("Cannot setAppConfig\n");
        return false;
    }

    if (WPC_register_for_app_config_data(onAppConfigDataReceived) != APP_RES_OK)
    {
        LOGE("Cannot register for app_config\n");
        return false;
    }
    return true;
}

static bool testMSAPAttributesStackOff()
{
    uint16_t min, max;
    // Access cycle
    int res = WPC_get_access_cycle_range(&min, &max);
    if (res != APP_RES_OK && res != APP_RES_ATTRIBUTE_NOT_SET)
    {
        LOGE("Cannot get access cycle range\n");
        return false;
    }

    if (res != APP_RES_ATTRIBUTE_NOT_SET)
    {
        LOGI("Access cycle range is %d - %d\n", min, max);
    }

    if (WPC_set_access_cycle_range(4000, 4000) != APP_RES_OK)
    {
        LOGE("Cannot set cycle range\n");
        return false;
    }
    else
    {
        if (WPC_get_access_cycle_range(&min, &max) != APP_RES_OK)
        {
            LOGE("Cannot get cycle range\n");
            return false;
        }

        if (min != 4000 || max != 4000)
        {
            LOGE("Cannot read back access cycle range set\n");
            return false;
        }
    }
    LOGI("Cycle range is now %d - %d\n", min, max);

    if (WPC_get_access_cycle_limits(&min, &max) != APP_RES_OK)
    {
        LOGE("Cannot get cycle limits\n");
        return false;
    }
    LOGI("Cycle range limits is %d - %d\n", min, max);
    return true;
}

static bool testMSAPAttributesStackOn()
{
    uint8_t res8;
    uint16_t res16;
    uint32_t res32;

    if (WPC_get_stack_status(&res8) != APP_RES_OK)
    {
        LOGE("Cannot get stack status\n");
        return false;
    }
    LOGI("Stack status is %d\n", res8);

    if (WPC_get_PDU_buffer_usage(&res8) != APP_RES_OK)
    {
        LOGE("Cannot get PDU buffer usage\n");
        return false;
    }
    LOGI("PDU buffer usage is %d\n", res8);

    if (WPC_get_PDU_buffer_capacity(&res8) != APP_RES_OK)
    {
        LOGE("Cannot get PDU buffer capacity\n");
        return false;
    }
    LOGI("PDU buffer capacity is %d\n", res8);

    // Battery remaining setting
    if (WPC_get_remaining_energy(&res8) != APP_RES_OK)
    {
        LOGE("Cannot get remaining battery usage\n");
        return false;
    }
    LOGI("Remaining battery is %d\n", res8);

    if (WPC_set_remaining_energy(128) != APP_RES_OK)
    {
        LOGE("Cannot set remaining battery usage\n");
        return false;
    }
    else
    {
        if (WPC_get_remaining_energy(&res8) != APP_RES_OK)
        {
            LOGE("Cannot get just set remaining battery usage\n");
            return false;
        }

        if (res8 != 128)
        {
            LOGE("Expecting 128 but receive %d\n", res8);
            return false;
        }
    }
    LOGI("Able to set remaining battery to %d\n", res8);

    if (WPC_get_autostart(&res8) != APP_RES_OK)
    {
        LOGE("Cannot get autostart setting\n");
        return false;
    }
    LOGI("Autostart is %d\n", res8);

    if (WPC_get_route_count(&res8) != APP_RES_OK)
    {
        LOGE("Cannot get route count\n");
        return false;
    }
    LOGI("Route count is %d\n", res8);

    if (WPC_get_system_time(&res32) != APP_RES_OK)
    {
        LOGE("Cannot get system time\n");
        return false;
    }
    LOGI("System time is %d\n", res32);

    if (WPC_get_current_access_cycle(&res16) != APP_RES_OK)
    {
        LOGE("Cannot get current access cycle\n");
        return false;
    }
    LOGI("Current Access Cycle is %d\n", res16);

    return true;
}

bool testClearScratchpad()
{
    app_scratchpad_status_t status;
    if (WPC_clear_local_scratchpad() != APP_RES_OK)
    {
        LOGE("Cannot clear local scratchpad\n");
        return false;
    }

    if (WPC_get_local_scratchpad_status(&status) != APP_RES_OK)
    {
        LOGE("Cannot get scratchpad status\n");
        return false;
    }

    if (status.scrat_len != 0)
    {
        LOGE("Scratchpad is not cleared\n");
        return false;
    }

    return true;
}

#define BLOCK_SIZE 128
#define SEQ_NUMBER 50
#define OTAP_FILE_PATH "source/test/scratchpad_nrf52.otap"
bool testLoadScratchpad()
{
    FILE * fp;
    app_scratchpad_status_t status;
    long file_size = 0;
    long written = 0;
    const char * filename = OTAP_FILE_PATH;
    uint8_t block[BLOCK_SIZE];

    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        LOGE("Cannot open file %s. Please update OTAP_FILE_PATH to a valid "
             "otap image\n",
             filename);
        return false;
    }

    /* Get size of binary */
    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    if (file_size <= 0)
    {
        LOGE("Cannot determine file size\n");
        return false;
    }
    LOGI("Loading otap file of %d bytes\n", file_size);

    /* Set cursor to beginning of file */
    fseek(fp, 0L, SEEK_SET);

    /* Start scratchpad update*/
    if (WPC_start_local_scratchpad_update(file_size, SEQ_NUMBER) != APP_RES_OK)
    {
        LOGE("Cannot start scratchpad update\n");
        return false;
    }

    /* Send scratchpad image block by block */
    LOGI("Start sending otap (it mays take up to 1 minute)\n");
    long remaining = file_size;
    while (remaining > 0)
    {
        uint8_t block_size = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        size_t read;

        read = fread(block, 1, block_size, fp);
        if (read != block_size)
        {
            LOGE("Error while reading file asked = %d read = %d\n", block_size, read);
            return false;
        }

        /* Send the block */
        if (WPC_upload_local_block_scratchpad(block_size, block, written) != APP_RES_OK)
        {
            LOGE("Cannot upload scratchpad block\n");
            return false;
        }

        written += block_size;
        remaining -= block_size;
    }

    if (WPC_get_local_scratchpad_status(&status) != APP_RES_OK)
    {
        LOGE("Cannot get scratchpad status\n");
        return false;
    }

    if (status.scrat_len != file_size)
    {
        LOGE("Scratchpad is not loaded correctly (wrong size) %d vs %d\n",
             status.scrat_len,
             file_size);
        return false;
    }

    if (status.scrat_seq_number != SEQ_NUMBER)
    {
        LOGE("Wrong seq number after loading a scratchpad image \n");
        return false;
    }

    return true;
}

static void onRemoteStatusCb(app_addr_t source_address,
                             app_scratchpad_status_t * status,
                             uint16_t request_timeout)
{
    LOGI("Received status from %d with tiemout to %d\n", source_address, request_timeout);
    LOGI("Stored Seq/Crc:%d/0x%04x, Processed Seq/Crc:%d/0x%04x\n",
         status->scrat_seq_number,
         status->scrat_crc,
         status->processed_scrat_seq_number,
         status->processed_scrat_crc);
}

static bool testRemoteStatus()
{
    if (WPC_register_for_remote_status(onRemoteStatusCb) != APP_RES_OK)
    {
        LOGE("Cannot register for remote status \n");
        return false;
    }

    // Wait for network to form
    // TODO replace by getneighbors
    Platform_usleep(60 * 1000 * 1000);

    if (WPC_get_remote_status(APP_ADDR_BROADCAST) != APP_RES_OK)
    {
        LOGE("Cannot send remote status\n");
        return false;
    }

    // Wait for remote status
    Platform_usleep(30 * 1000 * 1000);

    if (WPC_unregister_for_remote_status() != APP_RES_OK)
    {
        LOGE("Cannot unregister from remote status \n");
        return false;
    }

    return true;
}

static bool testRemoteUpdate()
{
    // Send a update with a 0 reboot delay to just test the command
    if (WPC_remote_scratchpad_update(APP_ADDR_BROADCAST, 50, 0) != APP_RES_OK)
    {
        LOGE("Cannot send remote update\n");
        return false;
    }

    return true;
}

static bool scan_done = false;

static void onScanNeighborsDone(uint8_t status)
{
    LOGI("Scan neighbors is done res=%d\n", status);
    scan_done = true;
}

static bool testScanNeighbors()
{
    app_nbors_t neighbors_list;

    // Register for end of scan Neighbors
    if (WPC_register_for_scan_neighbors_done(onScanNeighborsDone) != APP_RES_OK)
    {
        LOGE("Cannot register for remote status \n");
        return false;
    }

    // Ask for a scan
    if (WPC_start_scan_neighbors() != APP_RES_OK)
    {
        LOGE("Cannot start scan\n");
        return false;
    }

    LOGI("Wait 5 seconds for scan result\n");
    Platform_usleep(5 * 1000 * 1000);

    // scan_done should be protected but we can assume
    // that 5 sec is enough and no race can occur
    if (!scan_done)
    {
        LOGE("Scan is not done\n");
        return false;
    }

    if (WPC_get_neighbors(&neighbors_list) != APP_RES_OK)
    {
        LOGE("Cannot get neighbors list\n");
        return false;
    }

    LOGI("Get neighbors done and node has %d neighbors\n", neighbors_list.number_of_neighbors);
    if (neighbors_list.number_of_neighbors > 0)
    {
        LOGI("First node: %d, ch=%d, cost=%d, link=%d, type=%d, rssi=%d, "
             "tx_power=%d, rx_power=%d, last_update=%d \n",
             neighbors_list.nbors[0].add,
             neighbors_list.nbors[0].channel,
             neighbors_list.nbors[0].cost,
             neighbors_list.nbors[0].link_rel,
             neighbors_list.nbors[0].nbor_type,
             neighbors_list.nbors[0].norm_rssi,
             neighbors_list.nbors[0].tx_power,
             neighbors_list.nbors[0].rx_power,
             neighbors_list.nbors[0].last_update);
    }

    if (WPC_unregister_from_scan_neighbors_done() != APP_RES_OK)
    {
        LOGE("Cannot unregister from scan neighbors \n");
        return false;
    }

    return true;
}

static void onStackStatusReceived(uint8_t status)
{
    LOGI("Stack status received %d\n", status);
}

static bool testStackStatus()
{
    if (WPC_register_for_stack_status(onStackStatusReceived) != APP_RES_OK)
    {
        return false;
    }
    return true;
}

// Macro to launch a test and check result
#define RUN_TEST(_test_func_, _expected_result_)      \
    do                                                \
    {                                                 \
        LOGI("### Starting test %s\n", #_test_func_); \
        if (_test_func_() != _expected_result_)       \
        {                                             \
            LOGE("### Test is FAIL\n\n");             \
        }                                             \
        else                                          \
        {                                             \
            LOGI("### Test is PASS\n\n");             \
        }                                             \
    } while (0)

int Test_runAll()
{
    uint8_t status;

    RUN_TEST(testFactoryReset, true);

    setInitialState(APP_ROLE_SINK, 1234, 0x654321, 5, false);

    RUN_TEST(testScratchpadTarget, true);

    RUN_TEST(dumpCSAPAttributes, true);

    RUN_TEST(testAuthenticationKey, true);

    RUN_TEST(testCipherKey, true);

    RUN_TEST(testMSAPAttributesStackOff, true);

    RUN_TEST(testStackStatus, true);

    // Start the stack for following tests
    WPC_start_stack();

    // Ensure stack is started to avoid wrong test result
    while (true)
    {
        if (WPC_get_stack_status(&status) != APP_RES_OK)
        {
            LOGI("Cannot read stack state\n");
            continue;
        }

        if (status == 0)
        {
            break;
        }
        LOGI("Waiting for stack start\n");
    }

    RUN_TEST(testSendWithCallbacks, true);

    RUN_TEST(testSendWithoutCallbacks, true);

    RUN_TEST(testSendWithInitialTime, true);

    RUN_TEST(testAppConfigData, true);

    RUN_TEST(testMSAPAttributesStackOn, true);

    RUN_TEST(testScanNeighbors, true);

    return 0;
}

int Test_scratchpad()
{
    // Configure node as a sink
    setInitialState(APP_ROLE_SINK, 1234, 0x654321, 5, false);

    // Set app config
    setAppconfig((uint8_t *) "Test scratchpad", 1800, 14);

    RUN_TEST(testClearScratchpad, true);

    RUN_TEST(testLoadScratchpad, true);

    // Start the stack for following tests
    WPC_start_stack();

    // The following test needs at least a second connected node in the network
    RUN_TEST(testRemoteStatus, true);

    RUN_TEST(testRemoteUpdate, true);

    return 0;
}
