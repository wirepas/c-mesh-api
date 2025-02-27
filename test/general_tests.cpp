#include "wpc_test.hpp"

class WpcGeneralTestStackOff : public WpcTest
{
public:
    static void SetUpTestSuite()
    {
        ASSERT_NO_FATAL_FAILURE(WpcTest::SetUpTestSuite());
        ASSERT_NO_FATAL_FAILURE(WpcTest::StopStack());
    }
};

class WpcGeneralTestStackOn : public WpcTest
{
public:
    static void SetUpTestSuite()
    {
        ASSERT_NO_FATAL_FAILURE(WpcTest::SetUpTestSuite());

        // Following parameters should be set in order to be able to start the
        // stack, and stack should be stopped in order to be able to set them.
        ASSERT_NO_FATAL_FAILURE(WpcTest::StopStack());
        ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));
        ASSERT_EQ(APP_RES_OK, WPC_set_node_address(1234));
        ASSERT_EQ(APP_RES_OK, WPC_set_network_address(0x654321));
        ASSERT_EQ(APP_RES_OK, WPC_set_network_channel(38));
        ASSERT_NO_FATAL_FAILURE(WpcTest::StartStack());
    }
};

TEST_F(WpcGeneralTestStackOff, testFactoryReset)
{
    ASSERT_EQ(APP_RES_OK, WPC_set_node_address(200));
    ASSERT_EQ(APP_RES_OK, WPC_set_network_address(0x12123));
    ASSERT_EQ(APP_RES_OK, WPC_set_network_channel(39));

    ASSERT_EQ(APP_RES_OK, WPC_do_factory_reset());

    app_addr_t node_address;
    ASSERT_EQ(APP_RES_ATTRIBUTE_NOT_SET, WPC_get_node_address(&node_address));

    app_addr_t network_address;
    ASSERT_EQ(APP_RES_ATTRIBUTE_NOT_SET, WPC_get_network_address(&network_address));

    net_channel_t network_channel;
    ASSERT_EQ(APP_RES_ATTRIBUTE_NOT_SET, WPC_get_network_channel(&network_channel));
}

TEST_F(WpcGeneralTestStackOff, testSetCSAPAddressingAttributes)
{
    const app_role_t TEST_ROLE = APP_ROLE_HEADNODE;
    const app_addr_t TEST_NODE_ADDRESS = 3211;
    const app_addr_t TEST_NETWORK_ADDRESS = 45457;
    const net_channel_t TEST_NETWORK_CHANNEL = 36;

    ASSERT_EQ(APP_RES_OK, WPC_set_role(TEST_ROLE));
    ASSERT_EQ(APP_RES_OK, WPC_set_node_address(TEST_NODE_ADDRESS));
    ASSERT_EQ(APP_RES_OK, WPC_set_network_address(TEST_NETWORK_ADDRESS));
    ASSERT_EQ(APP_RES_OK, WPC_set_network_channel(TEST_NETWORK_CHANNEL));

    app_role_t role;
    ASSERT_EQ(APP_RES_OK, WPC_get_role(&role));
    ASSERT_EQ(TEST_ROLE, role);

    app_addr_t node_address;
    ASSERT_EQ(APP_RES_OK, WPC_get_node_address(&node_address));
    ASSERT_EQ(TEST_NODE_ADDRESS, node_address);

    app_addr_t network_address;
    ASSERT_EQ(APP_RES_OK, WPC_get_network_address(&network_address));
    ASSERT_EQ(TEST_NETWORK_ADDRESS, network_address);

    net_channel_t network_channel;
    ASSERT_EQ(APP_RES_OK, WPC_get_network_channel(&network_channel));
    ASSERT_EQ(TEST_NETWORK_CHANNEL, network_channel);
}

TEST_F(WpcGeneralTestStackOff, dumpCSAPAttributes)
{
    uint8_t mtu, pdus, scratch_seq, first_channel, last_channel, data_size;
    uint16_t firmware[4], api_v, hw_magic, stack_profile;

    ASSERT_EQ(APP_RES_OK, WPC_get_mtu(&mtu));
    RecordProperty("mtu_size", (long) mtu);

    ASSERT_EQ(APP_RES_OK, WPC_get_pdu_buffer_size(&pdus));
    RecordProperty("pdu_size", (long) pdus);

    ASSERT_EQ(APP_RES_OK, WPC_get_scratchpad_sequence(&scratch_seq));
    RecordProperty("scratch_seq", (long) scratch_seq);

    ASSERT_EQ(APP_RES_OK, WPC_get_mesh_API_version(&api_v));
    RecordProperty("api_v", (long) api_v);

    ASSERT_EQ(APP_RES_OK, WPC_get_firmware_version(firmware));
    RecordProperty("firmware_v_major", (long) firmware[0]);
    RecordProperty("firmware_v_minor", (long) firmware[1]);
    RecordProperty("firmware_v_maintenance", (long) firmware[2]);
    RecordProperty("firmware_v_development", (long) firmware[3]);

    ASSERT_EQ(APP_RES_OK, WPC_get_channel_limits(&first_channel, &last_channel));
    RecordProperty("first_channel", (long) first_channel);
    RecordProperty("last_channel", (long) last_channel);

    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&data_size));
    RecordProperty("max_config_data_size", (long) data_size);

    ASSERT_EQ(APP_RES_OK, WPC_get_hw_magic(&hw_magic));
    RecordProperty("hw_magic", (long) hw_magic);

    ASSERT_EQ(APP_RES_OK, WPC_get_stack_profile(&stack_profile));
    RecordProperty("stack_profile", (long) stack_profile);
}

TEST_F(WpcGeneralTestStackOff, testCipherKey)
{
    bool key_set;

    // Verify setting the key
    const uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    ASSERT_EQ(APP_RES_OK, WPC_set_cipher_key(key));

    ASSERT_EQ(APP_RES_OK, WPC_is_cipher_key_set(&key_set));
    ASSERT_TRUE(key_set);

    // Verify removing the key
    ASSERT_EQ(APP_RES_OK, WPC_remove_cipher_key());
    ASSERT_EQ(APP_RES_OK, WPC_is_cipher_key_set(&key_set));
    ASSERT_FALSE(key_set);
}

TEST_F(WpcGeneralTestStackOff, testAuthenticationKey)
{
    bool key_set;

    // Verify setting the key
    const uint8_t key[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    ASSERT_EQ(APP_RES_OK, WPC_set_authentication_key(key));

    ASSERT_EQ(APP_RES_OK, WPC_is_authentication_key_set(&key_set));
    ASSERT_TRUE(key_set);

    // Verify removing the key
    ASSERT_EQ(APP_RES_OK, WPC_remove_authentication_key());
    ASSERT_EQ(APP_RES_OK, WPC_is_authentication_key_set(&key_set));
    ASSERT_FALSE(key_set);
}

TEST_F(WpcGeneralTestStackOff, testScratchpadTarget)
{
    ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));

    ASSERT_EQ(APP_RES_OK, WPC_write_target_scratchpad(12, 0x1234, 2, 13));

    uint8_t target_seq;
    uint16_t target_crc;
    uint8_t action;
    uint8_t param;
    ASSERT_EQ(APP_RES_OK, WPC_read_target_scratchpad(&target_seq, &target_crc, &action, &param));

    EXPECT_EQ(12, target_seq);
    EXPECT_EQ(0x1234, target_crc);
    EXPECT_EQ(2, action);
    EXPECT_EQ(13, param);
}

TEST_F(WpcGeneralTestStackOff, gettingAppConfigShouldFailIfBufferIsTooSmall)
{
    uint8_t max_size;
    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&max_size));
    ASSERT_GT(max_size, 1);

    uint8_t seq;
    uint16_t interval;
    uint8_t config[UINT8_MAX];
    ASSERT_EQ(APP_RES_INVALID_VALUE, WPC_get_app_config_data(&seq, &interval, config, max_size - 1));
}

TEST_F(WpcGeneralTestStackOff, settingAppConfigShouldFailIfConfigTooLarge)
{
    ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));

    uint8_t max_size;
    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&max_size));
    ASSERT_LT(max_size, UINT8_MAX);

    const uint8_t config[UINT8_MAX] = {0};
    ASSERT_EQ(APP_RES_INVALID_VALUE, WPC_set_app_config_data(1, 30, config, max_size + 1));
}

TEST_F(WpcGeneralTestStackOff, testSettingAppConfigDataWithInvalidInterval)
{
    ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));

    const uint8_t INVALID_INTERVAL = 62;
    const uint8_t config[1] = {0};
    ASSERT_EQ(APP_RES_INVALID_DIAG_INTERVAL, WPC_set_app_config_data(1, INVALID_INTERVAL, config, sizeof(config)));
}

TEST_F(WpcGeneralTestStackOff, testSettingAppConfigData)
{
    ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));

    uint8_t max_size;
    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&max_size));

    uint8_t existing_seq;
    uint16_t existing_interval;
    uint8_t existing_config[UINT8_MAX];
    ASSERT_EQ(APP_RES_OK,
              WPC_get_app_config_data(&existing_seq,
                                      &existing_interval,
                                      existing_config,
                                      sizeof(existing_config)));

    uint8_t new_config[UINT8_MAX] = {0};
    for (uint8_t i = 0; i < sizeof(new_config); i++) {
        new_config[i] = i;
    }
    const uint16_t new_interval = 1800;
    // Set new app config. On recent stack releases, sequence is ignored.
    ASSERT_EQ(APP_RES_OK, WPC_set_app_config_data(100, new_interval, new_config, max_size));

    {
        uint8_t seq;
        uint16_t interval;
        uint8_t config[UINT8_MAX];
        ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data(&seq, &interval, config, sizeof(config)));
        ASSERT_EQ(new_interval, interval);
        ASSERT_EQ_ARRAY(new_config, config, max_size);
    }
}


TEST_F(WpcGeneralTestStackOff, testSettingAccesCycleRange)
{
    const uint16_t RANGE_MIN = 2000;
    const uint16_t RANGE_MAX = 4000;

    ASSERT_EQ(APP_RES_OK, WPC_set_access_cycle_range(RANGE_MIN, RANGE_MAX));

    {
        uint16_t min, max;
        ASSERT_EQ(APP_RES_OK, WPC_get_access_cycle_range(&min, &max));
        ASSERT_EQ(RANGE_MIN, min);
        ASSERT_EQ(RANGE_MAX, max);
    }

    {
        uint16_t min, max = 0;
        ASSERT_EQ(APP_RES_OK, WPC_get_access_cycle_limits(&min, &max));
        ASSERT_NE(0, min);
        ASSERT_NE(0, max);
    }
}

TEST_F(WpcGeneralTestStackOn, testReadMSAPAttributes)
{
    uint8_t res8;
    uint16_t res16;
    uint32_t res32;

    ASSERT_EQ(APP_RES_OK, WPC_get_stack_status(&res8));
    RecordProperty("stack_status", (long) res8);

    ASSERT_EQ(APP_RES_OK, WPC_get_PDU_buffer_usage(&res8));
    RecordProperty("pdu_buffer_usage", (long) res8);

    ASSERT_EQ(APP_RES_OK, WPC_get_PDU_buffer_capacity(&res8));
    RecordProperty("pdu_buffer_capacity", (long) res8);

    ASSERT_EQ(APP_RES_OK, WPC_get_autostart(&res8));
    RecordProperty("autostart", (long) res8);

    ASSERT_EQ(APP_RES_OK, WPC_get_route_count(&res8));
    RecordProperty("route_count", (long) res8);

    ASSERT_EQ(APP_RES_OK, WPC_get_system_time(&res32));
    RecordProperty("system_time", (long) res32);

    ASSERT_EQ(APP_RES_OK, WPC_get_current_access_cycle(&res16));
    RecordProperty("current_access_cycle", (long) res16);
}

