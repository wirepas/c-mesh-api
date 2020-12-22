/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef MSAP_H_
#define MSAP_H_

#include <stdint.h>
#include <string.h>
#include "wpc.h"  //For onAppConfigDataReceived_cb_f
#include "wpc_constants.h"
#include "attribute.h"
#include "util.h"

/* Attributes ID */
#define MSAP_STACK_STATUS 1
#define MSAP_PDU_BUFFER_USAGE 2
#define MSAP_PDU_BUFFER_CAPACITY 3
#define MSAP_ENERGY 5
#define MSAP_AUTOSTART 6
#define MSAP_ROUTE_COUNT 7
#define MSAP_SYSTEM_TIME 8
#define MSAP_ACCESS_CYCLE_RANGE 9
#define MSAP_ACCESS_CYCLE_LIMITS 10
#define MSAP_CURRENT_ACCESS_CYCLE 11
#define MSAP_SCRATCHPAD_BLOCK_MAX 12

/* General constant */
#define MAXIMUM_SCRATCHPAD_BLOCK_SIZE 112

/* Maximum number of neighbors in a */
#define MAXIMUM_NUMBER_OF_NEIGHBOR 8

#ifdef LEGACY_APP_CONFIG
#    define MAXIMUM_APP_CONFIG_SIZE 16
#else
#    define MAXIMUM_APP_CONFIG_SIZE 80
#endif  // LEGACY_APP_CONFIG

typedef struct __attribute__((__packed__))
{
    uint8_t start_option;
} msap_stack_start_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t sequence_number;
    uint16_t diag_data_interval;
    uint8_t app_config_data[MAXIMUM_APP_CONFIG_SIZE];
} msap_app_config_data_write_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint16_t attribute_id;
    uint8_t attribute_length;
    uint8_t attribute_value[16];
} msap_attribute_write_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint16_t attribute_id;
} msap_attribute_read_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint32_t scratchpad_length;
    uint8_t scratchpad_sequence_number;
} msap_image_start_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint32_t start_add;
    uint8_t number_of_bytes;
    uint8_t bytes[MAXIMUM_SCRATCHPAD_BLOCK_SIZE];
} msap_image_block_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t target_sequence;
    uint16_t target_crc;
    uint8_t action;
    uint8_t param;
} msap_scratchpad_write_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint32_t target;
} msap_image_remote_status_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint32_t target;
    uint8_t seq;
    uint16_t delay_s;

} msap_image_remote_update_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint32_t add;
    uint8_t link_rel;
    uint8_t norm_rssi;
    uint8_t cost;
    uint8_t channel;
    uint8_t nbor_type;
    uint8_t tx_power;
    uint8_t rx_power;
    uint16_t last_update;
} neighbor_info_t;

typedef struct __attribute__((__packed__))
{
    uint8_t number_of_neighbors;
    neighbor_info_t nbors[MAXIMUM_NUMBER_OF_NEIGHBOR];
} msap_get_nbors_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t indication_status;
    uint8_t scan_ready;
} msap_scan_nbors_ind_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t cost;
} msap_sink_cost_write_req_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    uint8_t cost;
} msap_sink_cost_read_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t indication_status;
    uint8_t status;
} msap_stack_state_ind_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t indication_status;
    uint8_t sequence_number;
    uint16_t diag_data_interval;
    uint8_t app_config_data[MAXIMUM_APP_CONFIG_SIZE];
} msap_app_config_data_rx_ind_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    uint8_t sequence_number;
    uint16_t diag_data_interval;
    uint8_t app_config_data[MAXIMUM_APP_CONFIG_SIZE];
} msap_app_config_data_read_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    uint16_t attribute_id;
    uint8_t attribute_length;
    uint8_t attribute_value[16];
} msap_attribute_read_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    uint32_t scrat_len;
    uint16_t scrat_crc;
    uint8_t scrat_seq_number;
    uint8_t scrat_type;
    uint8_t scrat_status;
    uint32_t processed_scrat_len;
    uint16_t processed_scrat_crc;
    uint8_t processed_scrat_seq_number;
    uint32_t firmware_memory_area_id;
    uint8_t firmware_major_ver;
    uint8_t firmware_minor_ver;
    uint8_t firmware_maint_ver;
    uint8_t firmware_dev_ver;
} msap_scratchpad_status_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t result;
    uint8_t target_sequence;
    uint16_t target_crc;
    uint8_t action;
    uint8_t param;
} msap_scratchpad_read_conf_pl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t indication_status;
    uint32_t source_address;
    msap_scratchpad_status_conf_pl_t status;
    uint16_t update_timeout;
} msap_image_remote_status_ind_pl_t;

static inline void
convert_internal_to_app_scratchpad_status(app_scratchpad_status_t * status_p,
                                          msap_scratchpad_status_conf_pl_t * internal_status_p)
{
    // Copy internal payload to app return code. Cannot use memcpy as structures
    // may have different alignment
    status_p->scrat_len = uint32_decode_le((uint8_t *) &internal_status_p->scrat_len);
    status_p->scrat_crc = uint16_decode_le((uint8_t *) &internal_status_p->scrat_crc);
    status_p->scrat_seq_number = internal_status_p->scrat_seq_number;
    status_p->scrat_type = internal_status_p->scrat_type;
    status_p->scrat_status = internal_status_p->scrat_status;
    status_p->processed_scrat_len =
        uint32_decode_le((uint8_t *) &internal_status_p->processed_scrat_len);
    status_p->processed_scrat_crc =
        uint16_decode_le((uint8_t *) &internal_status_p->processed_scrat_crc);
    status_p->processed_scrat_seq_number = internal_status_p->processed_scrat_seq_number;
    status_p->firmware_memory_area_id =
        uint32_decode_le((uint8_t *) &internal_status_p->firmware_memory_area_id);
    status_p->firmware_major_ver = internal_status_p->firmware_major_ver;
    status_p->firmware_minor_ver = internal_status_p->firmware_minor_ver;
    status_p->firmware_maint_ver = internal_status_p->firmware_maint_ver;
    status_p->firmware_dev_ver = internal_status_p->firmware_dev_ver;
}

/**
 * \brief   Convert internal neighbors list returned by get_neighbors to API
 * representation \param   neighbors_p Pointer to public API neighbor list
 * structure \param   internal_neighbors_p Pointer to internal msap neighbor
 * list structure
 */
static inline void
convert_internal_to_app_neighbors_status(app_nbors_t * neighbors_p,
                                         msap_get_nbors_conf_pl_t * internal_neighbors_p)
{
    // Copy internal payload to app return code. Cannot use memcpy as structures
    // may have different alignment
    memset(neighbors_p, 0, sizeof(app_nbors_t));

    neighbors_p->number_of_neighbors = internal_neighbors_p->number_of_neighbors;
    for (uint8_t i = 0; i < neighbors_p->number_of_neighbors; i++)
    {
        neighbors_p->nbors[i].add =
            uint32_decode_le((uint8_t *) &internal_neighbors_p->nbors[i].add);
        neighbors_p->nbors[i].channel = internal_neighbors_p->nbors[i].channel;
        neighbors_p->nbors[i].cost = internal_neighbors_p->nbors[i].cost;
        neighbors_p->nbors[i].link_rel = internal_neighbors_p->nbors[i].link_rel;
        neighbors_p->nbors[i].nbor_type = internal_neighbors_p->nbors[i].nbor_type;
        neighbors_p->nbors[i].norm_rssi = internal_neighbors_p->nbors[i].norm_rssi;
        neighbors_p->nbors[i].rx_power = internal_neighbors_p->nbors[i].rx_power;
        neighbors_p->nbors[i].tx_power = internal_neighbors_p->nbors[i].tx_power;
        neighbors_p->nbors[i].last_update =
            uint16_decode_le((uint8_t *) &internal_neighbors_p->nbors[i].last_update);
    }
}

/**
 * \brief    Request to start the stack
 * \param    start_option
 *           start option
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_stack_start_request(uint8_t start_option);

/**
 * \brief    Request to stop the stack
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_stack_stop_request();

/**
 * \brief    Request to write data config
 * \param    seq
 *           The app config sequence
 * \param    interval
 *           The diagnostics interval
 * \param    config_p
 *           Data config value
 * \param    size
 *           Size to app config to set
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_app_config_data_write_request(uint8_t seq, uint16_t interval, uint8_t * config_p, uint8_t size);

/**
 * \brief    Request to read data config
 * \param    seq
 *           The app config sequence
 * \param    interval
 *           The diagnostics interval
 * \param    config_p
 *           Data config value
 * \param    size
 *           Size to app config to get
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_app_config_data_read_request(uint8_t * seq, uint16_t * interval, uint8_t * config_p, uint8_t size);

/**
 * \brief    Request to write sink cost
 * \param    cost
 *           The new initial cost in 0-254 range
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise (0 OK, 1 not a sink)
 */
int msap_sink_cost_write_request(uint8_t cost);

/**
 * \brief    Request to read sink cost
 * \param    cost
 *           pointer to cost. Updated only if result is 0
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise (0 OK, 1 not a sink)
 */
int msap_sink_cost_read_request(uint8_t * cost_p);

/**
 * \brief    Request to get neighbors list
 * \param    neighbors_p
 *           List of neighbors
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_get_nbors_request(msap_get_nbors_conf_pl_t * neigbors_p);

/**
 * \brief    Request to start a scan
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scan_nbors_request();

/**
 * \brief    Request to start a scratchpad update
 * \param    length
 *           Total number of byte for scratchpad data.
 *           Length must be divisible by 16
 * \param    seq
 *           Sequence number of the new scratchpad
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scratchpad_start_request(uint32_t length, uint8_t seq);

/**
 * \brief    Request to send a scratchpad block
 * \param    start_address
 *           Start address of block (relative to beginning of scratchpad)
 * \param    number_of_bytes
 *           Number of bytes in the block
 *           Between 1 and 112
 * \param    bytes
 *           Bytes of scratchpad data
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scratchpad_block_request(uint32_t start_address, uint8_t number_of_bytes, uint8_t * bytes);

/**
 * \brief    Get the status of currently stored and processed scratchpad
 * \param    status_p
 *           Pointer to store the queried the status
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scratchpad_status_request(msap_scratchpad_status_conf_pl_t * status_p);

/**
 * \brief    Update the scratchpad by bootloader
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scratchpad_update_request();

/**
 * \brief    Clear the stored scratchpad
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scratchpad_clear_request();

/**
 * \brief    Request to write target scratchpad and action
 * \param    target_sequence
 *           The target sequence to set
 * \param    target_crc
 *           The target crc to set
 * \param    action
 *           The action to perform with this scratchpad
 * \param    param
 *           Parameter for the associated action (if relevant)
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scratchpad_target_write_request(uint8_t target_sequence,
                                         uint16_t target_crc,
                                         uint8_t action,
                                         uint8_t param);

/**
 * \brief    Request to read target scratchpad and action
 * \param    target_sequence_p
 *           Pointer to store the target sequence to set
 * \param    target_crc_p
 *           Pointer to store the crc to set
 * \param    action_p
 *           Pointer to store the action to perform with this scratchpad
 * \param    param_p
 *           Pointer to store the parameter for the associated action (if relevant)
 * \return   negative value if the request fails,
 *           a Mesh positive result otherwise
 */
int msap_scratchpad_target_read_request(uint8_t * target_sequence_p,
                                        uint16_t * target_crc_p,
                                        uint8_t * action_p,
                                        uint8_t * param_p);

/**
 * \brief   Get status of a remote node scratchpad status
 * \param   destination_address
 *          Node address of the node to query the status
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int msap_scratchpad_remote_status(app_addr_t destination_address);

/**
 * \brief   Start the scratchpad update on a remote node
 * \param   destination_address
 *          Node address of the node to start the update
 * \param   sequence
 *          Sequence number of the scratchpad to proceed
 * \param   delay_s
 *          Number of seconds before the OTAP scratchpad
 *          is marked for processing and the node is rebooted
 *          Setting the reboot delay to 0 cancels an ongoing
 *          update request
 * \return  negative value if the request fails,
 *          a Mesh positive result otherwise
 */
int msap_scratchpad_remote_update(app_addr_t destination_address, uint8_t sequence, uint16_t delay_s);

static inline int msap_attribute_write_request(uint16_t attribute_id,
                                               uint8_t attribute_length,
                                               uint8_t * attribute_value_p)
{
    return attribute_write_request(MSAP_ATTRIBUTE_WRITE_REQUEST, attribute_id, attribute_length, attribute_value_p);
}

static inline int msap_attribute_read_request(uint16_t attribute_id,
                                              uint8_t attribute_length,
                                              uint8_t * attribute_value_p)
{
    return attribute_read_request(MSAP_ATTRIBUTE_READ_REQUEST, attribute_id, attribute_length, attribute_value_p);
}

/**
 * \brief   Handler for stack state indication
 * \param   payload
 *          pointer to payload
 */
void msap_stack_state_indication_handler(msap_stack_state_ind_pl_t * payload);

/**
 * \brief   Handler for app config data receive
 * \param   payload
 *          pointer to payload
 */
void msap_app_config_data_rx_indication_handler(msap_app_config_data_rx_ind_pl_t * payload);

/**
 * \brief   Handler for image remote status indication
 * \param   payload
 *          pointer to payload
 */
void msap_image_remote_status_indication_handler(msap_image_remote_status_ind_pl_t * payload);

/**
 * \brief   Handler for scan neighbors indication
 * \param   payload
 *          pointer to payload
 */
void msap_scan_nbors_indication_handler(msap_scan_nbors_ind_pl_t * payload);

/**
 * \brief   Register for app config data
 * \param   cb
 *          Callback to invoke when app config is received
 * \return  True if registered successfully, false otherwise
 */
bool msap_register_for_app_config(onAppConfigDataReceived_cb_f cb);

/**
 * \brief   Unregister for app config data
 * \return  True if unregistered successfully, false otherwise
 */
bool msap_unregister_from_app_config();

/**
 * \brief   Register for remote status
 * \param   cb
 *          Callback to invoke when remote status is received
 * \return  True if registered successfully, false otherwise
 */
bool msap_register_for_remote_status(onRemoteStatus_cb_f cb);

/**
 * \brief   Unregister from remote status
 * \return  True if unregistered successfully, false otherwise
 */
bool msap_unregister_from_remote_status();

/**
 * \brief   Register for scan neighbors status
 * \param   cb
 *          Callback to invoke when neighbor scan is finished
 * \return  True if registered successfully, false otherwise
 */
bool msap_register_for_scan_neighbors_done(onScanNeighborsDone_cb_f cb);

/**
 * \brief   Unregister from scan neighbors status
 * \return  True if unregistered successfully, false otherwise
 */
bool msap_unregister_from_scan_neighbors_done();

/**
 * \brief   Register for stack status
 * \param   cb
 *          Callback to invoke when stack status is received
 * \return  True if registered successfully, false otherwise
 */
bool msap_register_for_stack_status(onStackStatusReceived_cb_f cb);

/**
 * \brief   Unregister from stack status
 * \return  True if unregistered successfully, false otherwise
 */
bool msap_unregister_from_stack_status();

#endif /* MSAP_H_ */
