#include "wpc_test.hpp"
#include <gtest/gtest.h>

class WpcCddApiTest : public WpcTest
{
public:
    static void SetUpTestSuite()
    {
        ASSERT_NO_FATAL_FAILURE(WpcTest::SetUpTestSuite());

        // Stack should be stopped for factory reset, and it's not needed to be
        // started for these tests.
        ASSERT_NO_FATAL_FAILURE(WpcTest::StopStack());
        ASSERT_EQ(APP_RES_OK, WPC_do_factory_reset());
    }

protected:
    void SetUp() override
    {
        ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));
    }

    void TearDown() override
    {
        ASSERT_EQ(APP_RES_OK, WPC_do_factory_reset());
    }

    struct __attribute__((__packed__)) CddScratchpadDataItem
    {
        uint8_t seq;
        uint16_t crc;
        uint8_t action;
        uint8_t param;
    };

#define STACK_PROFILE ISM_24
#if STACK_PROFILE == ISM_24
    const static uint16_t CDD_DIAG_INTERVAL_EP   = 0xF7FF;
    const static uint16_t CDD_APP_CONFIG_EP      = 0xF9FF;
    const static uint16_t CDD_SCRATCHPAD_DATA_EP = 0xFAFA;
#elif STACK_PROFILE == DECT_NR
    const static uint16_t CDD_DIAG_INTERVAL_EP   = 0xA029;
    const static uint16_t CDD_APP_CONFIG_EP      = 0x4004;
    const static uint16_t CDD_SCRATCHPAD_DATA_EP = 0x4002;
#endif
};


TEST_F(WpcCddApiTest, testReadingAppConfigAndDiagWithCddApi)
{
    const uint8_t TEST_CONFIG[]               = { 0x11, 0x22, 0x33, 0x44, 0x55 };
    const uint16_t TEST_DIAG_INTERVAL_SECONDS = 600;
    const uint8_t EXPECTED_RAW_DIAG_INTERVAL  = 12;

    ASSERT_EQ(APP_RES_OK,
              WPC_set_app_config_data(1, TEST_DIAG_INTERVAL_SECONDS, TEST_CONFIG, sizeof(TEST_CONFIG)));

    uint8_t app_config_size = 0;
    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&app_config_size));
    ASSERT_GT(app_config_size, 0);

    {
        uint8_t payload[UINT8_MAX] = { 0 };
        uint8_t payload_size = 0;
        ASSERT_EQ(APP_RES_OK, WPC_get_config_data_item(CDD_APP_CONFIG_EP, payload, sizeof(payload), &payload_size));
        ASSERT_EQ(app_config_size, payload_size);
        ASSERT_EQ_ARRAY(TEST_CONFIG, payload, sizeof(TEST_CONFIG));
    }

    {
        uint8_t payload[UINT8_MAX] = { 0 };
        uint8_t payload_size = 0;
        ASSERT_EQ(APP_RES_OK, WPC_get_config_data_item(CDD_DIAG_INTERVAL_EP, payload, sizeof(payload), &payload_size));
        ASSERT_EQ(1, payload_size);
        ASSERT_EQ(EXPECTED_RAW_DIAG_INTERVAL, payload[0]);
    }
}

TEST_F(WpcCddApiTest, testWritingAppConfigAndDiagWithCddApi)
{
    uint8_t app_config_size = 0;
    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&app_config_size));
    ASSERT_GT(app_config_size, 0);

    const uint8_t TEST_CONFIG[UINT8_MAX] = { 0xDD, 0xAB, 0xCD, 0x99, 0x42 };
    ASSERT_GE(sizeof(TEST_CONFIG), app_config_size);
    const uint8_t RAW_DIAG_INTERVAL = 10;
    const uint16_t EXPECTED_DIAG_INTERVAL_SECONDS = 120;

    const uint8_t PAYLOAD[] = { RAW_DIAG_INTERVAL };
    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(CDD_DIAG_INTERVAL_EP, PAYLOAD, sizeof(PAYLOAD)));

    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(CDD_APP_CONFIG_EP, TEST_CONFIG, app_config_size));

    uint8_t seq = 0;
    uint16_t interval = 0;
    uint8_t config[UINT8_MAX] = { 0 };
    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data(&seq, &interval, config, sizeof(config)));
    ASSERT_EQ(EXPECTED_DIAG_INTERVAL_SECONDS, interval);
    ASSERT_EQ_ARRAY(TEST_CONFIG, config, app_config_size);
}

TEST_F(WpcCddApiTest, testReadingScratchpatTargetWithCddApi)
{
    const uint8_t TEST_SEQ    = 53;
    const uint16_t TEST_CRC   = 0xABCD;
    const uint8_t TEST_ACTION = 3;
    const uint8_t TEST_PARAM  = 0x4A;

    ASSERT_EQ(APP_RES_OK, WPC_write_target_scratchpad(TEST_SEQ, TEST_CRC, TEST_ACTION, TEST_PARAM));

    uint8_t payload[255] = {0};
    uint8_t payload_size = 0;
    ASSERT_EQ(APP_RES_OK, WPC_get_config_data_item(CDD_SCRATCHPAD_DATA_EP, payload, sizeof(payload), &payload_size));
    ASSERT_EQ(sizeof(CddScratchpadDataItem), payload_size);
    CddScratchpadDataItem* scratchpad_data = (CddScratchpadDataItem*) payload;
    ASSERT_EQ(TEST_SEQ, scratchpad_data->seq);
    ASSERT_EQ(TEST_CRC, scratchpad_data->crc);
    ASSERT_EQ(TEST_ACTION, scratchpad_data->action);
    ASSERT_EQ(TEST_PARAM, scratchpad_data->param);
}

TEST_F(WpcCddApiTest, testWritingScratchpatTargetWithCddApi)
{
    const CddScratchpadDataItem SCRATCHPAD_DATA = {
        .seq = 71,
        .crc = 0xFEEB,
        .action = 3,
        .param = 0xC2
    };

    ASSERT_EQ(APP_RES_OK,
              WPC_set_config_data_item(CDD_SCRATCHPAD_DATA_EP,
                                       (uint8_t const*) &SCRATCHPAD_DATA,
                                       sizeof(SCRATCHPAD_DATA)));

    uint8_t seq    = 0;
    uint16_t crc   = 0;
    uint8_t action = 0;
    uint8_t param  = 0;
    ASSERT_EQ(APP_RES_OK, WPC_read_target_scratchpad(&seq, &crc, &action, &param));
    ASSERT_EQ(SCRATCHPAD_DATA.seq, seq);
    ASSERT_EQ(SCRATCHPAD_DATA.crc, crc);
    ASSERT_EQ(SCRATCHPAD_DATA.action, action);
    ASSERT_EQ(SCRATCHPAD_DATA.param, param);
}

TEST_F(WpcCddApiTest, testWritingInvalidScratchpatTargetWithCddApi)
{
    GTEST_SKIP() << "TODO re-enable once implemented on stack";

    const CddScratchpadDataItem SCRATCHPAD_DATA = {
        .seq = 71,
        .crc = 0xFEEB,
        .action = 250, // Invalid value
        .param = 0xC2
    };

    ASSERT_EQ(APP_RES_DATA_ERROR,
              WPC_set_config_data_item(CDD_SCRATCHPAD_DATA_EP,
                                       (uint8_t const*) &SCRATCHPAD_DATA,
                                       sizeof(SCRATCHPAD_DATA)));
}

TEST_F(WpcCddApiTest, testWritingInvalidDiagIntervalWithCddApi)
{
    GTEST_SKIP() << "TODO re-enable once implemented on stack";

    const uint8_t TEST_DIAG_INTERVAL[] = { 0xFE };
    ASSERT_EQ(APP_RES_DATA_ERROR,
              WPC_set_config_data_item(CDD_DIAG_INTERVAL_EP, TEST_DIAG_INTERVAL, 1));
}

TEST_F(WpcCddApiTest, testWritingInvalidScratchpadTargetSizeWithCddApi)
{
    const uint8_t TEST_PAYLOAD[10] = { 0 };
    ASSERT_EQ(APP_RES_DATA_ERROR,
              WPC_set_config_data_item(CDD_SCRATCHPAD_DATA_EP, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));
}

TEST_F(WpcCddApiTest, testWritingInvalidDiagIntervalSizeWithCddApi)
{
    const uint8_t TEST_PAYLOAD[10] = { 0 };
    ASSERT_EQ(APP_RES_DATA_ERROR,
              WPC_set_config_data_item(CDD_DIAG_INTERVAL_EP, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));
}

TEST_F(WpcCddApiTest, testWritingInvalidAppConfigDataWithCddApi)
{
    // App config data contents can be anything, but the size should be correct

    uint8_t app_config_size = 0;
    ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&app_config_size));
    ASSERT_GT(app_config_size, 2);
    const uint8_t INVALID_APP_CONFIG_SIZE = app_config_size - 1;

    const uint8_t PAYLOAD[UINT8_MAX] = { 0 };
    ASSERT_EQ(APP_RES_DATA_ERROR, WPC_set_config_data_item(CDD_APP_CONFIG_EP, PAYLOAD, INVALID_APP_CONFIG_SIZE));
}

TEST_F(WpcCddApiTest, settingItemShouldFailOnNonSinkNode)
{
    ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_HEADNODE));

    const uint8_t PAYLOAD[] = { 0xAA, 0xBB };
    ASSERT_EQ(APP_RES_NODE_NOT_A_SINK, WPC_set_config_data_item(100, PAYLOAD, sizeof(PAYLOAD)));
}

TEST_F(WpcCddApiTest, gettingItemShouldWorkOnNonSinkNode)
{
    const uint8_t TEST_PAYLOAD[] = { 0x55, 0x11, 0xFA };
    const uint16_t TEST_EP = 0x100;

    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_EP, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));

    ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_HEADNODE));

    uint8_t payload[UINT8_MAX] = { 0 };
    uint8_t payload_size = 0;
    ASSERT_EQ(APP_RES_OK, WPC_get_config_data_item(TEST_EP, payload, sizeof(payload), &payload_size));
    ASSERT_EQ(sizeof(TEST_PAYLOAD), payload_size);
    ASSERT_EQ_ARRAY(TEST_PAYLOAD, payload, payload_size);
}

TEST_F(WpcCddApiTest, testSettingAndDeletingOptionalItem)
{
    const uint8_t TEST_PAYLOAD[] = { 0xF1, 0xD0, 0xA9, 0xB4 };
    const uint16_t TEST_EP = 0x100;

    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_EP, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));

    {
        uint8_t payload[UINT8_MAX] = { 0 };
        uint8_t payload_size = 0;
        ASSERT_EQ(APP_RES_OK, WPC_get_config_data_item(TEST_EP, payload, sizeof(payload), &payload_size));
        ASSERT_EQ(sizeof(TEST_PAYLOAD), payload_size);
        ASSERT_EQ_ARRAY(TEST_PAYLOAD, payload, payload_size);
    }

    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_EP, NULL, 0));

    {
        uint8_t payload[UINT8_MAX] = { 0 };
        uint8_t payload_size = 0;
        ASSERT_EQ(APP_RES_INVALID_VALUE, WPC_get_config_data_item(TEST_EP, payload, sizeof(payload), &payload_size));
    }
}

TEST_F(WpcCddApiTest, settingTooLargePayloadShouldFail)
{
    const uint8_t TEST_PAYLOAD[250] = { 0 };
    ASSERT_EQ(APP_RES_INTERNAL_ERROR, WPC_set_config_data_item(100, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));
}

TEST_F(WpcCddApiTest, testReplacingOptionalItem)
{
    const uint16_t TEST_EP = 0x100;
    const uint8_t TEST_PAYLOAD_1[] = { 0xA1, 0xB1, 0xC1, 0xD1 };
    const uint8_t TEST_PAYLOAD_2[] = { 0xA0, 0xB0 };

    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_EP, TEST_PAYLOAD_1, sizeof(TEST_PAYLOAD_1)));
    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_EP, TEST_PAYLOAD_2, sizeof(TEST_PAYLOAD_2)));

    uint8_t payload[UINT8_MAX] = { 0 };
    uint8_t payload_size = 0;
    ASSERT_EQ(APP_RES_OK, WPC_get_config_data_item(TEST_EP, payload, sizeof(payload), &payload_size));
    ASSERT_EQ(sizeof(TEST_PAYLOAD_2), payload_size);
    ASSERT_EQ_ARRAY(TEST_PAYLOAD_2, payload, payload_size);
}

TEST_F(WpcCddApiTest, gettingNonExistingEndpointShouldFail)
{
    const uint16_t TEST_EP = 0xE999;

    uint8_t payload[UINT8_MAX] = { 0 };
    uint8_t payload_size = 0;
    ASSERT_EQ(APP_RES_INVALID_VALUE, WPC_get_config_data_item(TEST_EP, payload, sizeof(payload), &payload_size));
}

TEST_F(WpcCddApiTest, factoryResetShouldRemoveOptionalItems)
{
    const uint8_t TEST_PAYLOAD[] = { 0xAA, 0xBB };
    const uint16_t TEST_EP = 0x100;

    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_EP, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));

    ASSERT_EQ(APP_RES_OK, WPC_do_factory_reset());

    uint8_t payload[UINT8_MAX] = { 0 };
    uint8_t payload_size = 0;
    ASSERT_EQ(APP_RES_INVALID_VALUE, WPC_get_config_data_item(TEST_EP, payload, sizeof(payload), &payload_size));
    ASSERT_EQ(0, payload_size);
}

TEST_F(WpcCddApiTest, settingTooManyOptionalItemsShouldFail)
{
    const uint16_t MAX_ATTEMPTS = 50;
    uint16_t detected_max_item_count = 0;
    for (uint16_t ep = 1; ep <= MAX_ATTEMPTS; ep++)
    {
        const uint8_t TEST_PAYLOAD[] = { 0x43 };
        const app_res_e res = WPC_set_config_data_item(ep, TEST_PAYLOAD, sizeof(TEST_PAYLOAD));
        if (APP_RES_OK == res) {
            detected_max_item_count++;
        } else if (APP_RES_OUT_OF_MEMORY == res) {
            break;
        } else {
            FAIL() << "Unexpected error occured";
        }
    }

    ASSERT_LT(detected_max_item_count, MAX_ATTEMPTS);
    ASSERT_GT(detected_max_item_count, 0);
}

TEST_F(WpcCddApiTest, gettingItemShouldFailIfBufferIsTooSmall)
{
    const uint8_t TEST_PAYLOAD[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
    const uint16_t TEST_ENDPOINT = 0x1001;
    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_ENDPOINT, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));

    uint8_t payload[4] = { 0 };
    uint8_t payload_size = 0;
    ASSERT_EQ(APP_RES_INTERNAL_ERROR,
              WPC_get_config_data_item(TEST_ENDPOINT, payload, sizeof(payload), &payload_size));
}

TEST_F(WpcCddApiTest, gettingEndpointsListShouldFailIfBufferIsTooSmall)
{
    const uint8_t TEST_PAYLOAD[] = { 0x10 };
    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(100, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));
    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(200, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));

    uint16_t endpoints_list[1] = { 0 };
    uint8_t endpoints_count = 0xFF;
    ASSERT_EQ(APP_RES_INTERNAL_ERROR,
              WPC_get_config_data_item_list(endpoints_list, sizeof(endpoints_list), &endpoints_count));
}

TEST_F(WpcCddApiTest, shouldReturnEmptyListIfNoOptionalItemIsSet)
{
    uint16_t endpoints_list[10] = { 0 };
    uint8_t endpoints_count = 0xFF;
    ASSERT_EQ(APP_RES_OK,
              WPC_get_config_data_item_list(endpoints_list, sizeof(endpoints_list), &endpoints_count));
    ASSERT_EQ(0, endpoints_count);
}

TEST_F(WpcCddApiTest, gettingListOfOptionalEndpointsShouldWork)
{
    const std::set<uint16_t> endpoints = { 0xAABB, 0x1234, 0xD0B0 };
    for (const auto& endpoint : endpoints) {
        const uint8_t TEST_PAYLOAD[] = { 0x10 };
        ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(endpoint, TEST_PAYLOAD, sizeof(TEST_PAYLOAD)));
    }

    uint16_t endpoints_list[3] = { 0 };
    uint8_t endpoints_count = 0xFF;
    ASSERT_EQ(APP_RES_OK,
              WPC_get_config_data_item_list(endpoints_list, sizeof(endpoints_list), &endpoints_count));

    ASSERT_EQ(endpoints.size(), endpoints_count);
    for (uint8_t i = 0; i < endpoints_count; i++){
        ASSERT_EQ(1, endpoints.count(endpoints_list[i]));
    }
}

