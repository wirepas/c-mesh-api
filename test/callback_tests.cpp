#include "wpc_test.hpp"

#include <cstring>
#include <mutex>
#include <chrono>
#include <thread>

class WpcCallbackTest : public WpcTest
{
public:
    static void SetUpTestSuite()
    {
        ASSERT_NO_FATAL_FAILURE(WpcTest::SetUpTestSuite());

        // Following parameters should be set in order to be able to start the
        // stack, and stack should be stopped in order to be able to set them.
        ASSERT_NO_FATAL_FAILURE(WpcTest::StopStack());
        ASSERT_EQ(APP_RES_OK, WPC_set_role(APP_ROLE_SINK));
        ASSERT_EQ(APP_RES_OK, WPC_set_node_address(2345));
        ASSERT_EQ(APP_RES_OK, WPC_set_network_address(0x65432));
        ASSERT_EQ(APP_RES_OK, WPC_set_network_channel(38));

        // Set diagnostics interval to zero, so that data callbacks are not called.
        const uint8_t TEST_CONFIG[] = { 0 };
        ASSERT_EQ(APP_RES_OK, WPC_set_app_config_data(1, 0, TEST_CONFIG, sizeof(TEST_CONFIG)));

        ASSERT_EQ(APP_RES_OK, WPC_start_stack());

        // Wait a bit for possible earlier status messages to be ignored
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

protected:
    struct BaseCallbackHandler
    {
        void WaitForCallback(size_t max_seconds = 5)
        {
            for (size_t i = 0; i < 20 * max_seconds; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::lock_guard<std::mutex> lock(mutex);
                if (callback_called) {
                    return;
                }
            }

            ADD_FAILURE() << "Callback was not called";
        }

        std::mutex mutex;
        bool callback_called;
    };

    struct StackStatusCb : public BaseCallbackHandler
    {
        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_called = false;
            status = 0;
        }

        uint8_t status;
    };

    struct ScanNeighborsCb : public BaseCallbackHandler
    {
        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_called = false;
            status = 0;
        }

        uint8_t status;
    };

    struct AppConfigCb : public BaseCallbackHandler
    {
        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_called = false;
            interval = 0;
            std::memset(config, 0, sizeof(config));
            app_config_size = 0;
        }

        uint16_t interval;
        uint8_t config[UINT8_MAX];

        uint8_t app_config_size;
    };

    struct DataSentCb : public BaseCallbackHandler
    {
        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_called = false;
            pduid = 0;
            result = 0;
        }

        uint16_t pduid;
        uint8_t result;
    };

    struct DataReceivedCb : public BaseCallbackHandler
    {
        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_called = false;
            bytes.clear();
            qos = APP_QOS_NORMAL;
            src_addr = 0;
            dst_addr = 0;
            expected_src_ep = 0;
            expected_dst_ep = 0;
        }

        std::vector<uint8_t> bytes;
        app_qos_e qos;
        app_addr_t src_addr;
        app_addr_t dst_addr;

        uint8_t expected_src_ep;
        uint8_t expected_dst_ep;
    };

    struct ConfigDataItemCb : public BaseCallbackHandler
    {
        void reset()
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_called = false;
            payload.clear();
            expected_endpoint = 0;
        }

        std::vector<uint8_t> payload;

        uint16_t expected_endpoint;
    };

    inline static StackStatusCb stack_status_cb;
    inline static ScanNeighborsCb scan_neighbors_cb;
    inline static AppConfigCb app_config_cb;
    inline static DataSentCb data_sent_cb;
    inline static DataReceivedCb data_received_cb;
    inline static ConfigDataItemCb config_data_item_cb;

    static void onStackStatusReceived(uint8_t status)
    {
        std::lock_guard<std::mutex> lock(stack_status_cb.mutex);
        stack_status_cb.callback_called = true;

        stack_status_cb.status = status;
    }

    static void onScanNeighborsDone(uint8_t status)
    {
        std::lock_guard<std::mutex> lock(scan_neighbors_cb.mutex);
        scan_neighbors_cb.callback_called = true;

        scan_neighbors_cb.status = status;
    }

    static void onAppConfigDataReceived(uint8_t /* seq */, uint16_t interval, uint8_t* config)
    {
        std::lock_guard<std::mutex> lock(app_config_cb.mutex);
        app_config_cb.callback_called = true;

        app_config_cb.interval = interval;
        std::memcpy(app_config_cb.config, config, app_config_cb.app_config_size);
    }

    static void onDataSent(uint16_t pduid, uint32_t /* buffering_delay */, uint8_t result)
    {
        std::lock_guard<std::mutex> lock(data_sent_cb.mutex);
        data_sent_cb.callback_called = true;

        data_sent_cb.pduid = pduid;
        data_sent_cb.result = result;
    }

    static bool onDataReceived(const uint8_t* bytes,
                               size_t num_bytes,
                               app_addr_t src_addr,
                               app_addr_t dst_addr,
                               app_qos_e qos,
                               uint8_t src_ep,
                               uint8_t dst_ep,
                               uint32_t /* travel_time */,
                               uint8_t /* hop_count */,
                               unsigned long long /* timestamp_ms_epoch */)
    {
        if (src_ep != data_received_cb.expected_src_ep || dst_ep != data_received_cb.expected_dst_ep) {
            return false;
        }

        std::lock_guard<std::mutex> lock(data_received_cb.mutex);
        data_received_cb.callback_called = true;

        data_received_cb.bytes.resize(num_bytes);
        std::memcpy(data_received_cb.bytes.data(), bytes, num_bytes);
        data_received_cb.qos = qos;
        data_received_cb.src_addr = src_addr;
        data_received_cb.dst_addr = dst_addr;

        return true;
    }

    static void onConfigDataItemReceived(const uint16_t endpoint,
                                         const uint8_t* const payload,
                                         const uint8_t size)
    {
        if (endpoint != config_data_item_cb.expected_endpoint) {
            return;
        }

        std::lock_guard<std::mutex> lock(config_data_item_cb.mutex);
        config_data_item_cb.callback_called = true;

        config_data_item_cb.payload.resize(size);
        std::memcpy(config_data_item_cb.payload.data(), payload, size);
    }

    void SetUp() override
    {
        stack_status_cb.reset();
        scan_neighbors_cb.reset();
        app_config_cb.reset();
        data_sent_cb.reset();
        data_received_cb.reset();
        config_data_item_cb.reset();
    }

    void TearDown() override
    {
        WPC_unregister_from_stack_status();
        WPC_unregister_from_scan_neighbors_done();
        WPC_unregister_from_app_config_data();
        WPC_unregister_for_data();
        WPC_unregister_from_config_data_item();
    }
};

TEST_F(WpcCallbackTest, testStackStatus)
{
    // Only testing stack stopping, because in some DualMCU versions indication
    // for stack starting is not sent.

    ASSERT_EQ(APP_RES_OK, WPC_register_for_stack_status(onStackStatusReceived));

    ASSERT_EQ(APP_RES_OK, WPC_stop_stack());

    ASSERT_NO_FATAL_FAILURE(stack_status_cb.WaitForCallback());

    {
        std::lock_guard<std::mutex> lock(stack_status_cb.mutex);
        // bit 0: 0 = running, 1 = sopped
        ASSERT_EQ(1, (stack_status_cb.status & 1));
    }

    ASSERT_EQ(APP_RES_OK, WPC_start_stack());
}

TEST_F(WpcCallbackTest, testScanNeighbors)
{
    ASSERT_EQ(APP_RES_OK, WPC_register_for_scan_neighbors_done(onScanNeighborsDone));

    ASSERT_EQ(APP_RES_OK, WPC_start_scan_neighbors());

    ASSERT_NO_FATAL_FAILURE(scan_neighbors_cb.WaitForCallback());

    {
        std::lock_guard<std::mutex> lock(scan_neighbors_cb.mutex);
        // 1 = scan done
        ASSERT_EQ(1, scan_neighbors_cb.status);
    }

    app_nbors_t neighbors_list;
    ASSERT_EQ(APP_RES_OK, WPC_get_neighbors(&neighbors_list));
}

TEST_F(WpcCallbackTest, testAppConfigData)
{
    {
        std::lock_guard<std::mutex> lock(app_config_cb.mutex);
        ASSERT_EQ(APP_RES_OK, WPC_get_app_config_data_size(&app_config_cb.app_config_size));
    }

    ASSERT_EQ(APP_RES_OK, WPC_register_for_app_config_data(onAppConfigDataReceived));

    const uint8_t TEST_CONFIG[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    ASSERT_EQ(APP_RES_OK, WPC_set_app_config_data(1, 600, TEST_CONFIG, sizeof(TEST_CONFIG)));

    ASSERT_NO_FATAL_FAILURE(app_config_cb.WaitForCallback());

    {
        std::lock_guard<std::mutex> lock(app_config_cb.mutex);
        ASSERT_EQ(600, app_config_cb.interval);
        ASSERT_EQ_ARRAY(TEST_CONFIG, app_config_cb.config, sizeof(TEST_CONFIG));
    }
}

TEST_F(WpcCallbackTest, testSendDataWithSendCallback)
{
    const uint8_t TEST_DATA[] = { 0x10, 0x12, 0x14, 0x16, 0x18 };

    // Send data to the sink itself
    ASSERT_EQ(APP_RES_OK,
              WPC_send_data(TEST_DATA, sizeof(TEST_DATA), 106, APP_ADDR_ANYSINK, APP_QOS_HIGH, 50, 50, onDataSent, 0));

    ASSERT_NO_FATAL_FAILURE(data_sent_cb.WaitForCallback());

    {
        std::lock_guard<std::mutex> lock(data_sent_cb.mutex);
        // 0 = success, 1 = failure
        ASSERT_EQ(0, data_sent_cb.result);
        ASSERT_EQ(106, data_sent_cb.pduid);
    }
}

TEST_F(WpcCallbackTest, testSendDataWithReceiveCallback)
{
    const uint8_t TEST_SRC_EP = 51;
    const uint8_t TEST_DST_EP = 52;
    const uint8_t TEST_DATA[] = { 0x10, 0x12, 0x14, 0x16, 0x18 };
    const app_qos_e TEST_QOS = APP_QOS_HIGH;

    net_addr_t address;
    ASSERT_EQ(APP_RES_OK, WPC_get_node_address(&address));

    {
        std::lock_guard<std::mutex> lock(data_received_cb.mutex);
        data_received_cb.expected_src_ep = TEST_SRC_EP;
        data_received_cb.expected_dst_ep = TEST_DST_EP;
    }

    ASSERT_EQ(APP_RES_OK, WPC_register_for_data(onDataReceived));


    // Send data to the sink itself
    ASSERT_EQ(APP_RES_OK,
              WPC_send_data(TEST_DATA,
                            sizeof(TEST_DATA),
                            10,
                            APP_ADDR_ANYSINK,
                            TEST_QOS,
                            TEST_SRC_EP,
                            TEST_DST_EP,
                            NULL,
                            0));

    ASSERT_NO_FATAL_FAILURE(data_received_cb.WaitForCallback());

    {
        std::lock_guard<std::mutex> lock(data_received_cb.mutex);
        ASSERT_EQ(sizeof(TEST_DATA), data_received_cb.bytes.size());
        ASSERT_EQ_ARRAY(TEST_DATA, (uint8_t*)data_received_cb.bytes.data(), sizeof(TEST_DATA));
        ASSERT_EQ(TEST_QOS, data_received_cb.qos);
        ASSERT_EQ(address, data_received_cb.src_addr);
        ASSERT_EQ(address, data_received_cb.dst_addr);
    }
}

TEST_F(WpcCallbackTest, testSendFragmentedDataWithReceiveCallback)
{
    const uint8_t TEST_SRC_EP = 73;
    const uint8_t TEST_DST_EP = 81;
    uint8_t TEST_DATA[500] = { 0 };
    std::memset(TEST_DATA, 0xF1, sizeof(TEST_DATA));
    const app_qos_e TEST_QOS = APP_QOS_HIGH;

    net_addr_t address;
    ASSERT_EQ(APP_RES_OK, WPC_get_node_address(&address));

    {
        std::lock_guard<std::mutex> lock(data_received_cb.mutex);
        data_received_cb.expected_src_ep = TEST_SRC_EP;
        data_received_cb.expected_dst_ep = TEST_DST_EP;
    }

    ASSERT_EQ(APP_RES_OK, WPC_register_for_data(onDataReceived));

    // Send data to the sink itself
    ASSERT_EQ(APP_RES_OK,
              WPC_send_data(TEST_DATA,
                            sizeof(TEST_DATA),
                            20,
                            APP_ADDR_ANYSINK,
                            TEST_QOS,
                            TEST_SRC_EP,
                            TEST_DST_EP,
                            NULL,
                            0));

    ASSERT_NO_FATAL_FAILURE(data_received_cb.WaitForCallback());

    {
        std::lock_guard<std::mutex> lock(data_received_cb.mutex);
        ASSERT_EQ(sizeof(TEST_DATA), data_received_cb.bytes.size());
        ASSERT_EQ_ARRAY(TEST_DATA, (uint8_t*)data_received_cb.bytes.data(), sizeof(TEST_DATA));
        ASSERT_EQ(TEST_QOS, data_received_cb.qos);
        ASSERT_EQ(address, data_received_cb.src_addr);
        ASSERT_EQ(address, data_received_cb.dst_addr);
    }
}

TEST_F(WpcCallbackTest, testConfigDataItemCallback)
{
    const uint16_t TEST_ENDPOINT = 0x4156;
    const uint8_t TEST_DATA[] = { 0x95, 0x85, 0x75 };

    {
        std::lock_guard<std::mutex> lock(data_received_cb.mutex);
        config_data_item_cb.expected_endpoint = TEST_ENDPOINT;
    }

    ASSERT_EQ(APP_RES_OK, WPC_register_for_config_data_item(onConfigDataItemReceived));

    ASSERT_EQ(APP_RES_OK, WPC_set_config_data_item(TEST_ENDPOINT, TEST_DATA, sizeof(TEST_DATA)));

    ASSERT_NO_FATAL_FAILURE(config_data_item_cb.WaitForCallback());

    {
        std::lock_guard<std::mutex> lock(config_data_item_cb.mutex);
        ASSERT_EQ(sizeof(TEST_DATA), config_data_item_cb.payload.size());
        ASSERT_EQ_ARRAY(TEST_DATA, (uint8_t*)config_data_item_cb.payload.data(), sizeof(TEST_DATA));
    }
}
