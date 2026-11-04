/*
 * provisioning_tests.cpp - HIL test cases for the FW-5902 provisioning
 * configuration attribute (CSAP attribute 28).
 *
 * Requirements to run:
 *  - a device supporting the provisioning configuration attribute, i.e. a
 *    dualmcu_app built with use_key_mgmt=yes (otherwise every case fails
 *    with APP_RES_INTERNAL_ERROR, the "unsupported attribute" mapping)
 *  - run from test/ with:
 *      WPC_SERIAL_PORT=/dev/ttyACM0 WPC_BAUD_RATE=125000 ./build/meshAPItest
 *
 * The tests are self-contained: the round-trip cases write their own known
 * configuration first, so they pass regardless of what is stored on the
 * device. The "not yet configured" read path (APP_RES_ATTRIBUTE_NOT_SET)
 * is deliberately not asserted here: device history decides whether
 * settings are stored, and it is covered by the wpagent mock tier instead.
 */
#include "wpc_test.hpp"

#include <cstring>

// CRC16-CCITT-FALSE: init 0xffff, poly 0x1021, MSB first, no final xor.
// Same algorithm as the node side's Crc_fromBuffer (util/crc.c), applied
// there over the encryption key followed by the authentication key.
static uint16_t crc16_ccitt(const uint8_t * buf, uint32_t len)
{
    uint16_t crc = 0xffff;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t) ((uint16_t) buf[i] << 8);
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x8000u)
            {
                crc = (uint16_t) ((crc << 1) ^ 0x1021u);
            }
            else
            {
                crc = (uint16_t) (crc << 1);
            }
        }
    }
    return crc;
}

// Expected CRC16 of the keys as the node reports it: encryption key
// followed by authentication key (32 bytes in total).
static uint16_t expected_keys_crc16(const wpc_provisioning_config_t & config)
{
    uint8_t keys[32];
    memcpy(keys, config.encryption_key, 16);
    memcpy(keys + 16, config.authentication_key, 16);
    return crc16_ccitt(keys, sizeof(keys));
}

// A known configuration to write. uid is ASCII, uid_len must be 1..79.
static void fill_config(wpc_provisioning_config_t & config,
                        const char * uid)
{
    memset(&config, 0, sizeof(config));
    config.method = 1;
    config.retries = 3;
    config.timeout_s = 60;

    for (int i = 0; i < 16; i++)
    {
        config.encryption_key[i] = (uint8_t) (0x00 + i);
        config.authentication_key[i] = (uint8_t) (0xf0 + i);
    }

    config.uid_len = (uint8_t) strlen(uid);
    memcpy(config.uid, uid, config.uid_len);
}

class WpcProvisioningTestStackOff : public WpcTest
{
public:
    static void SetUpTestSuite()
    {
        ASSERT_NO_FATAL_FAILURE(WpcTest::SetUpTestSuite());
        ASSERT_NO_FATAL_FAILURE(WpcTest::StopStack());
    }
};

class WpcProvisioningTestStackOn : public WpcTest
{
public:
    static void SetUpTestSuite()
    {
        ASSERT_NO_FATAL_FAILURE(WpcTest::SetUpTestSuite());

        // Minimal configuration so the stack can start. The stack must be
        // stopped in order to set them.
        ASSERT_NO_FATAL_FAILURE(WpcTest::StopStack());
        ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));
        ASSERT_EQ(APP_RES_OK, WPC_set_node_address(1234));
        ASSERT_EQ(APP_RES_OK, WPC_set_network_address(0x654321));
        ASSERT_EQ(APP_RES_OK, WPC_set_network_channel(38));
        ASSERT_NO_FATAL_FAILURE(WpcTest::StartStack());
    }
};

// Write a short-UID configuration, read it back and verify every field.
// Keys are write-only: only their CRC16 is readable.
TEST_F(WpcProvisioningTestStackOff, testProvisioningConfigRoundTrip)
{
    wpc_provisioning_config_t config;
    fill_config(config, "node-fw5902-test");
    ASSERT_EQ(APP_RES_OK, WPC_set_provisioning_config(&config));

    wpc_provisioning_config_read_t read_back;
    memset(&read_back, 0, sizeof(read_back));
    ASSERT_EQ(APP_RES_OK, WPC_get_provisioning_config(&read_back));

    EXPECT_EQ(config.method, read_back.method);
    EXPECT_EQ(config.retries, read_back.retries);
    EXPECT_EQ(config.timeout_s, read_back.timeout_s);
    EXPECT_EQ(expected_keys_crc16(config), read_back.key_crc16);
    EXPECT_EQ(config.uid_len, read_back.uid_len);
    EXPECT_EQ(0, memcmp(config.uid, read_back.uid, read_back.uid_len));

    // Rewrite with the maximum UID and verify again. method stays 1 (SECURED;
    // valid methods are 0, 1, 3, 7 per provisioning_method_e on the node side;
    // anything else is rejected with attr result 4 = APP_RES_INVALID_VALUE).
    char max_uid[WPC_PROVISIONING_UID_MAX_LEN];
    for (int i = 0; i < WPC_PROVISIONING_UID_MAX_LEN; i++)
    {
        max_uid[i] = (char) ('a' + (i % 26));
    }
    fill_config(config, max_uid);
    config.method = 1;
    config.retries = 0;
    config.timeout_s = 65535;
    ASSERT_EQ(APP_RES_OK, WPC_set_provisioning_config(&config));

    ASSERT_EQ(APP_RES_OK, WPC_get_provisioning_config(&read_back));
    EXPECT_EQ(config.method, read_back.method);
    EXPECT_EQ(config.retries, read_back.retries);
    EXPECT_EQ(config.timeout_s, read_back.timeout_s);
    EXPECT_EQ(expected_keys_crc16(config), read_back.key_crc16);
    EXPECT_EQ(WPC_PROVISIONING_UID_MAX_LEN, read_back.uid_len);
    EXPECT_EQ(0, memcmp(config.uid, read_back.uid, read_back.uid_len));
}

// The library validates uid_len locally: 0 and > WPC_PROVISIONING_UID_MAX_LEN
// are rejected without touching the device.
TEST_F(WpcProvisioningTestStackOff, testProvisioningConfigInvalidUidLength)
{
    wpc_provisioning_config_t config;
    fill_config(config, "node-fw5902-test");

    config.uid_len = 0;
    ASSERT_EQ(APP_RES_INVALID_VALUE, WPC_set_provisioning_config(&config));

    config.uid_len = WPC_PROVISIONING_UID_MAX_LEN + 1;
    ASSERT_EQ(APP_RES_INVALID_VALUE, WPC_set_provisioning_config(&config));
}

// Writing the attribute while the stack is running is not supported by the
// node: the attribute result (2) maps to APP_RES_STACK_NOT_STOPPED.
TEST_F(WpcProvisioningTestStackOn, testProvisioningConfigWriteWhileRunning)
{
    wpc_provisioning_config_t config;
    fill_config(config, "node-fw5902-test");
    ASSERT_EQ(APP_RES_STACK_NOT_STOPPED, WPC_set_provisioning_config(&config));
}
