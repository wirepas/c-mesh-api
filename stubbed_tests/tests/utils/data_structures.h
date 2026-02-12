#pragma once

extern "C" {
    #include <wpc.h>
}
#include <array>
#include <cstdint>

/**
 * \brief DSAP DATA_RX.indication frame
 */
struct [[gnu::packed]] DataRxIndication
{
    uint8_t indication_status;
    uint32_t src_add;
    uint8_t src_endpoint;
    uint32_t dest_add;
    uint8_t dest_endpoint;
    uint8_t qos_hop_count;
    uint32_t travel_time;
    uint8_t apdu_length;
    std::array<uint8_t, 102> apdu;
};

/**
 * \bried Arguments for onDataReceived_cb_f
 */
struct OnDataReceivedArgs
{
    std::vector<uint8_t> bytes;
    app_addr_t src_addr;
    app_addr_t dst_addr;
    app_qos_e qos;
    uint8_t src_ep;
    uint8_t dst_ep;
    uint32_t travel_time;
    uint8_t hop_count;
    unsigned long long timestamp_ms_epoch;
};

/**
 * \brief CSAP/DSAP/MSAP-ATTRIBUTE_READ.confirm frame
 */
struct [[gnu::packed]] AttributeReadConfirm
{
    uint8_t result;
    uint16_t attribute_id;
    uint8_t attribute_length;
    std::array<uint8_t, 16> attribute_value;
};

