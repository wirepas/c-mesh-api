/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#define LOG_MODULE_NAME "tsap"
#define MAX_LOG_LEVEL NO_LOG_LEVEL
#include "logger.h"
#include "wpc_types.h"
#include "wpc_internal.h"
#include "util.h"
#include "platform.h"

#include "string.h"

int tsap_setTestMode_request(const uint32_t addr)
{
    LOGI("tsap_setTestMode_request:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_TEST_MODE_ENTER_REQ;
    request.payload_length = sizeof(addr);
    memcpy(&request.payload.tsap_testModeSet_request_payload, &addr, sizeof(addr));
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    LOGI("result = %d \n", confirm.payload.sap_response_payload.result);
    return confirm.payload.sap_response_payload.result;
}

int tsap_exitTestMode_request(void)
{
    LOGI("tsap_exitTestMode_request:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_TEST_MODE_EXIT_REQ;
    request.payload_length = 0;
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    LOGI("result = %d \n", confirm.payload.sap_response_payload.result);
    return confirm.payload.sap_response_payload.result;
}

int tsap_setRadioChannel_request(const uint8_t radio_channel)
{
    LOGI("tsap_setRadioChannel_request:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_RADIO_CHANNEL_REQ;
    request.payload_length = sizeof(radio_channel);
    memcpy(&request.payload.tsap_radioChannel_request_payload, &radio_channel, sizeof(radio_channel));
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    LOGI("result = %d \n", confirm.payload.sap_response_payload.result);
    return confirm.payload.sap_response_payload.result;
}

int tsap_setRadioPower_request(const int8_t dbm)
{
    LOGI("tsap_setRadioPower_request:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_RADIO_TX_POWER_REQ;
    request.payload_length = sizeof(dbm);
    memcpy(&request.payload.tsap_radioTXpower_request_payload, &dbm, sizeof(dbm));
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    LOGI("result = %d \n", confirm.payload.sap_response_payload.result);
    return confirm.payload.sap_response_payload.result;
}

int tsap_sendRadioData(const tsap_radio_tx_data_req_pl_t * data, uint32_t * sentBursts)
{
    LOGI("tsap_sendRadioData:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_RADIO_DATA_TX_REQ;
    request.payload_length = sizeof(data->hdr.len) + sizeof(data->hdr.seq) +
                             sizeof(data->txCtrl.bursts) +
                             sizeof(data->txCtrl.ccaDuration) +
                             sizeof(data->txCtrl.txInterval) + data->hdr.len;
    request.payload.tsap_radioSendData_request_payload.txCtrl.txInterval =
        data->txCtrl.txInterval;
    request.payload.tsap_radioSendData_request_payload.txCtrl.bursts =
        data->txCtrl.bursts;
    request.payload.tsap_radioSendData_request_payload.txCtrl.ccaDuration =
        data->txCtrl.ccaDuration;
    request.payload.tsap_radioSendData_request_payload.hdr.len = data->hdr.len;
    request.payload.tsap_radioSendData_request_payload.hdr.seq = data->hdr.seq;
    memcpy(&request.payload.tsap_radioSendData_request_payload.data,
           data->data,
           request.payload_length);
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    *sentBursts = confirm.payload.tsap_radioSendData_confirm_payload.sentBursts;
    LOGI("result = %d \n", confirm.payload.tsap_radioSendData_confirm_payload.result);
    return confirm.payload.tsap_radioSendData_confirm_payload.result;
}

int tsap_getRadioMaxDataSize(uint8_t * size)
{
    LOGI("tsap_getRadioMaxDataSize:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_RADIO_GET_MAX_DATA_SIZE_REQ;
    request.payload_length = 0;
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    *size = confirm.payload.tsap_radioMaxDataSize_confirm_payload.size;
    LOGI("result = %d \n", confirm.payload.tsap_radioMaxDataSize_confirm_payload.result);
    return confirm.payload.tsap_radioMaxDataSize_confirm_payload.result;
}

int tsap_sendRadioTestSignal(const tsap_radio_tx_test_signal_req_pl_t * data)
{
    LOGI("tsap_sendRadioTestSignal:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_RADIO_TEST_SIGNAL_TX_REQ;
    request.payload_length =
        sizeof(data->signalType) + sizeof(data->txCtrl.bursts) +
        sizeof(data->txCtrl.ccaDuration) + sizeof(data->txCtrl.txInterval);
    // If parameters are right FW enters into continuus TX loop and does not
    // send confirmation
    request.payload.tsap_radioSendTestSignal_request_payload.txCtrl.txInterval =
        data->txCtrl.txInterval;
    request.payload.tsap_radioSendTestSignal_request_payload.txCtrl.bursts =
        data->txCtrl.bursts;
    request.payload.tsap_radioSendTestSignal_request_payload.txCtrl.ccaDuration =
        data->txCtrl.ccaDuration;
    request.payload.tsap_radioSendTestSignal_request_payload.signalType = data->signalType;
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    LOGI("result = %d \n", confirm.payload.tsap_radioSendData_confirm_payload.result);
    return confirm.payload.tsap_radioSendData_confirm_payload.result;
}

int tsap_allowRadioReception(const bool rxEnable, const bool dataIndiEnable)
{
    LOGI("tsap_allowRadioReception:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_RADIO_DATA_RX_REQ;
    request.payload_length = sizeof(rxEnable) + sizeof(dataIndiEnable);
    request.payload.tsap_radioReceive_request_payload.rxEnable = rxEnable;
    request.payload.tsap_radioReceive_request_payload.dataIndiEnable = dataIndiEnable;
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    LOGI("result = %d \n", confirm.payload.sap_response_payload.result);
    return confirm.payload.sap_response_payload.result;
}

int tsap_readRadioData(tsap_radio_rx_data_t * data)
{
    LOGI("tsap_readRadioData:\n");
    wpc_frame_t request, confirm;
    int res;
    request.primitive_id = TSAP_RADIO_DATA_READ_REQ;
    request.payload_length = 0;
    res = WPC_Int_send_request(&request, &confirm);
    LOGI("res = %d \n", res);
    if (res < 0)
        return res;
    LOGI("result = %d \n", confirm.payload.tsap_radioDataRead_confirm_payload.result);
    memcpy(data, &confirm.payload.tsap_radioDataRead_confirm_payload.rx, sizeof(tsap_radio_rx_data_t));
    return confirm.payload.tsap_radioDataRead_confirm_payload.result;
}