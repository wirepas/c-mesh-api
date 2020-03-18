/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#ifndef TSAP_H_
#define TSAP_H_

#include <stdint.h>

/**
 *  \brief Reservation for maximum test data size in radio interface
 *
 *  Test data header includes length (1 Byte) and sequence
 *  number (4 Bytes) which allocates fixed 5 bytes from over the air
 *  payload, remaining bytes can be used for free form data.
 *
 *  Maximum data size depends on used radio hardware. Here we allocate
 *  big enough buffer to cover all supported radio hardwares and
 *  some extra bytes for future needs.
 *
 *  True size of the radio payload can be read with tsap_getRadioMaxDataSize().
 */
#define APDU_TSAP_MAX_SIZE (180 - 1 - 4)

typedef enum
{
    /** Transmits random symbols */
    TSAP_RADIO_TEST_SIGNAL_RANDOM = 0,
    /** Transmits all symbols (0 -255) in the signaling alphabet 0-255,
     *  ref. FCC 558074 D01 15.247 Meas Guidance v05.*/
    TSAP_RADIO_TEST_SIGNAL_ALL_SYMBOLS = 1,
} tsap_radio_test_signal_type_e;

typedef struct __attribute__((__packed__))
{
    uint8_t channel;
} tsap_radio_channel_req_pl_t;

typedef struct __attribute__((__packed__))
{
    /** Radio network address to be used in data transmission */
    uint32_t network_address;
} tsap_set_test_mode_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t txpower;
} tsap_radio_tx_power_req_pl_t;

typedef struct __attribute__((__packed__))
{
    /* Sequence number of the message  */
    uint32_t seq;
    /* Length of the test data in payload  */
    uint8_t len;
} tsap_test_data_header_t;

typedef struct __attribute__((__packed__))
{
    /** Number of data bursts to be sent.
     *  Note! Data transmission is atomic operation in test mode.
     *  Bigger the bursts value is longer the transmission takes and delays
     *  response to caller.  **/
    uint32_t bursts;
    /* CCA duration in ms. If 0, no CCA done */
    uint32_t ccaDuration;
    /** When CCA is not used (ccaDuration=0) transmit interval defines the delay between
     *  TX bursts in us. If 0, sending done as soon as possible. */
    uint32_t txInterval;
} tsap_radio_transmitter_control_t;

typedef struct __attribute__((__packed__))
{
    tsap_radio_transmitter_control_t txCtrl;
    /* Header of received test data */
    tsap_test_data_header_t hdr;
    /** Test data to be transmitted */
    uint8_t data[APDU_TSAP_MAX_SIZE];
} tsap_radio_tx_data_req_pl_t;

typedef struct __attribute__((__packed__))
{
    tsap_radio_transmitter_control_t txCtrl;
    /** Test signal type to be trasmitted */
    //tsap_radio_test_signal_type_e  signalType;
    uint8_t signalType;
} tsap_radio_tx_test_signal_req_pl_t;

typedef struct __attribute__((__packed__))
{
    /* Number of received messages since test mode started.
     * Can be used e.g. for PER% calculation together with rxSeq */
    uint32_t cntr;
    /* Number of duplicate messages.
     * Incremented if received message has the same sequence number than
     * previous test data package. */
    uint32_t dup;
} tsap_test_data_counters_t;

typedef struct __attribute__((__packed__))
{
    /** Received signal strength in dBm with resolution of 1 dBm. */
    int8_t rssi;
    /** Received data counters. */
    tsap_test_data_counters_t rxCntrs;
    /* Header of received test data */
    tsap_test_data_header_t hdr;
    /** Received test data */
    uint8_t data[APDU_TSAP_MAX_SIZE];
} tsap_radio_rx_data_t;

typedef struct __attribute__((__packed__))
{
    uint8_t queued_indications;
    tsap_radio_rx_data_t rx;
} tsap_radio_data_rx_ind_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    tsap_radio_rx_data_t rx;
} tsap_radio_data_read_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    bool rxEnable;
    bool dataIndiEnable;
} tsap_radio_receive_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    uint8_t size;
} tsap_radio_max_data_size_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    uint32_t sentBursts;
} tsap_radio_send_data_conf_pl_t;

/**
 * \brief   Activates the test mode
 * \param   addr
 *          On-air radio address to be used in radio tests
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_setTestMode_request(const uint32_t addr);

/**
 * \brief   Exit from the test mode
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_exitTestMode_request(void);

/**
 * \brief   Sets radio channel
 * \param   radio_channel
 *          Logical channel number for transceiver
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_setRadioChannel_request(const uint8_t radio_channel);

/**
 * \brief   Sets radio power level
 * \param   dbm
 *          Power level for transceiver in dBm
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_setRadioPower_request(const int8_t dbm);

/**
 * \brief   Sends test data
 * \param   data
 *          Defines parameters for transmission and test data itself
 * \param   sentBursts
 *          Pointer to location where to store number of sent bursts
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_sendRadioData(const tsap_radio_tx_data_req_pl_t * data, uint32_t * sentBurst);

/**
 * @brief   Sends test signal
 * @param   data
 *          Defines type of the test signal and parameters for transmission
 * \param   sentBursts
 *          Pointer to location where to store number of sent bursts
 * @return  negative value if the request fails
 *
 */
int tsap_sendRadioTestSignal(const tsap_radio_tx_test_signal_req_pl_t * data,
                             uint32_t * sentBursts);

/**
 * \brief   Get the maximum data size radio can handle
 * \param   size
 *          Pointer where to store the size
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_getRadioMaxDataSize(uint8_t * size);

/**
 * \brief   Control of data reception
 * \param   rxEnable
 *          True to enable receiver and calling of reception callback function.
 *          False to disable receiver and calling callback function.
 * \param   dataIndiEnable,
 *          True to enable sending of data indications each time new test data
 *          package is received.
 *          False to disable sending of data indications.
 *          Note! Latest received data package can always be read with
 *          tsap_readRadioData().
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_allowRadioReception(const bool rxEnable, const bool dataIndiEnable);

/**
 * \brief   Reads the last received test data package from buffer
 * \param   data
 *          Pointer to the data structure where data will be copied
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int tsap_readRadioData(tsap_radio_rx_data_t * data);

#endif /* TSAP_H_ */