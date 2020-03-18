/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdio.h>
#include <string.h>

#define LOG_MODULE_NAME "Test-mode-tests"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

#include "wpc.h"
#include "tsap.h"
#include "test_mode.h"
#include "platform.h"

#define DATA_READ_TIMEOUT 30   // data read timeout in seconds for RX test
#define PER_PRINT_INTERVAL 10  // printing interval in seconds for RX test

/*
 * Define common radio settings for TX and RX modes.
 */
typedef struct
{
    /* Radio channel for radio tests */
    uint8_t channel;
    /* On-air radio address for radio tests */
    uint32_t nwkAddress;
} commonSettings_t;

static const commonSettings_t m_radio = {
    /* First channel, check channel numbers from radio profile document */
    .channel = 1,
    /* Some random address*/
    .nwkAddress = 0x1A2B3C,
};

/* Define TX specific settings */
typedef struct
{
    /* Parameters for test data transmission */
    tsap_radio_transmitter_control_t ctrl;
    /* Transmitter's power level */
    int8_t dbm;
} txSettings_t;

static const txSettings_t m_tx = {
    /* When bursts (ctrl.bursts) value > 1 and CCA is not used
     * (ctrl.ccaDuration=0) the txInterval defines the delay between TX bursts
     * in us. Typical value is between 0 to hundreds of milliseconds.
     * Recommended minimum value for EFR32FG is 0.5ms (ctrl.txInterval=500) in
     * PER tests to avoid missed bursts by the RX node.
     * */
    .ctrl.txInterval = 500,
    /* No clear channel assessment used */
    .ctrl.ccaDuration = 0,
    /* Send 10 packages per request */
    .ctrl.bursts = 10,
    /* Power level to be used in the test */
    .dbm = 8,
};

/* RX data for Packet Error Rate (PER) calculation. */
typedef struct
{
    /* First sequence number for PER calculation.*/
    uint32_t startSeq;
    /* First number of received messages for PER calculation. */
    uint32_t startRXCntr;
    /* Last received sequence number for PER calculation. */
    uint32_t lastSeq;
    /* Last number of received messages for PER calculation. */
    uint32_t lastRXCntr;
    /* Number of duplicate messages, removed from PER calculation. */
    uint32_t dup;
    /* Last calculated PER % */
    float per;
    /* Integer and decimal parts of PER % for printing purpose */
    uint8_t perInt;
    uint8_t perDec;
    /* Absolute signal strength in dBm  in resolution of 1dBm */
    int8_t rssi;
} per_data_t;

static per_data_t m_rx = {
    .startSeq = 0,
    .startRXCntr = 0,
    .per = 0,
    .rssi = -127,
};

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

/**
 * \brief   Packet Error Rate (PER %) calculation
 *
 * This function calculates PER % i.e. number of missed messages in
 * relation to number of sent messages. PER value is needed in some
 * certification measurements.
 *
 * PER% = (seq - (rxCntr - dup)) / seq
 *
 * \param   seq
 *          Number of sent messages
 * \param   rxCntr
 *          Number of received received message
 * \param   dup
 *          Number of duplicate messages. Incremented if two consecutive
 *          has the same sequence number
 * \return  per
 *          Calculated Packet Error Rate percentage
 */
static float calculate_per(app_test_mode_data_received_t * rxdata)
{
    uint32_t seq = rxdata->hdr.seq;
    uint32_t dup = rxdata->rxCntrs.dup;
    uint32_t rxCntr = rxdata->rxCntrs.cntr;

    if (m_rx.startRXCntr == 0 || m_rx.startSeq >= seq)
    {
        LOGI("Re-started RX counters: rxCntr = %u, seq = %u\n", rxCntr, seq);
        m_rx.startRXCntr = rxCntr;
        m_rx.startSeq = seq;
    }
    m_rx.lastRXCntr = rxCntr;
    m_rx.lastSeq = seq;
    rxCntr = rxCntr - m_rx.startRXCntr;
    seq = seq - m_rx.startSeq;
    if (seq > 0)
    {
        m_rx.per = ((float) (seq - (rxCntr - dup)) / (float) (seq)) * 100;
    }

    // store RSSI value of last received package
    m_rx.rssi = rxdata->rssi;

    return m_rx.per;
}

/**
 * \brief   Print packet error rate and RSSI values
 *
 * This function prints PER % calculated by calculate_per() and RSSI value of
 * the last received message.
 *
 */
static void print_per(void)
{
    LOGI("PER = %.2f, RSSI = %d [dBm]\n", m_rx.per, m_rx.rssi);
}

int Set_Test_Mode()
{
    if (WPC_setTestMode(m_radio.nwkAddress) != APP_RES_OK)
    {
        return false;
    }
    return true;
}
int Exit_Test_Mode()
{
    // Exit from testmode
    if (WPC_exitTestMode() != APP_RES_OK)
    {
        return false;
    }
    // Wait that node boots
    Platform_usleep(500000);
    return true;
}

int Set_Radio_Channel()
{
    if (WPC_setRadioChannel(m_radio.channel) != APP_RES_OK)
    {
        return false;
    }
    return true;
}

int Set_Radio_Power()
{
    if (WPC_setRadioPower(m_tx.dbm) != APP_RES_OK)
    {
        return false;
    }
    return true;
}

int Get_Radio_Max_Data_Size(uint8_t * maxLen)
{
    if (WPC_getRadioMaxDataSize(maxLen) != APP_RES_OK)
    {
        return false;
    }
    LOGI("Data maxLen: %d\n", *maxLen);
    return true;
}

int Send_Radio_Data()
{
    uint8_t dataMaxLen;

    app_test_mode_data_transmit_t TXData;
    uint32_t sentBursts = 0;
    TXData.txPayload.hdr.seq = 1;
    TXData.txCtrl.bursts = m_tx.ctrl.bursts;
    TXData.txCtrl.ccaDuration = m_tx.ctrl.ccaDuration;
    TXData.txCtrl.txInterval = m_tx.ctrl.txInterval;

    Get_Radio_Max_Data_Size(&dataMaxLen);
    // Prepare free form test data to be sent over the air.
    uint8_t data[255];
    for (uint8_t i = 0; i < dataMaxLen; i++)
    {
        data[i] = i;
    }
    TXData.txPayload.data = &data[0];
    // Header is sent over the air, so subtract its size from space
    // reserved for test data in txPayload.
    TXData.txPayload.hdr.len = dataMaxLen - sizeof(TXData.txPayload.hdr.len) -
                               sizeof(TXData.txPayload.hdr.seq);

    // send package over the air every 1 s
    while (true)
    {
        if (WPC_sendRadioData(&TXData, &sentBursts) != APP_RES_OK)
        {
            LOGE("Sending failed\n");
            return false;
        }
        TXData.txPayload.hdr.seq += sentBursts;
        Platform_usleep(1000000);
        LOGI("sent %d bursts\n", sentBursts);
    }
    return true;
}

int Enable_Radio_Reception()
{
    // Enable receiver
    if (WPC_allowRadioReception(true, false) != APP_RES_OK)
    {
        return false;
    }
    return true;
}

int Read_Radio_Data()
{
    app_test_mode_data_received_t rxData;
    app_res_e resp;
    uint32_t lastRXCntr = m_rx.lastRXCntr;
    uint8_t dataTimeout = 0;
    uint8_t printInterval = 0;

    while (dataTimeout < DATA_READ_TIMEOUT)
    {
        resp = WPC_readRadioData(&rxData);
        dataTimeout++;
        if (resp == APP_RES_OK)
        {
            dataTimeout = 0;
            calculate_per(&rxData);
        }
        printInterval++;
        if (printInterval >= PER_PRINT_INTERVAL)
        {
            if (m_rx.lastRXCntr == lastRXCntr)
            {
                LOGI("No messages in %d seconds\n", printInterval);
            }
            else
            {
                lastRXCntr = m_rx.lastRXCntr;
                print_per();
            }
            printInterval = 0;
        }
        Platform_usleep(1000000);
    }
    LOGI("No data received for %d seconds.\n", DATA_READ_TIMEOUT);

    return true;
}

int Send_Radio_Signal()
{
    // Ask FW to create and send random data over the air
    // Note FW enters continuous TX loop and does not respond to request
    // Device needs to be reset or power boot to exit TX mode
    app_test_mode_signal_transmit_t signal;
    signal.signalType = TSAP_RADIO_TEST_SIGNAL_RANDOM;
    signal.txCtrl.bursts = m_tx.ctrl.bursts;
    signal.txCtrl.ccaDuration = m_tx.ctrl.ccaDuration;
    signal.txCtrl.txInterval = m_tx.ctrl.txInterval;
    if (WPC_sendRadioTestSignal(&signal) != APP_RES_OK)
    {
        return false;
    }

    return true;
}