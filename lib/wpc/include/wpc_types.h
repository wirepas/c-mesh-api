/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef WPC_TYPES_H__
#define WPC_TYPES_H__

#include <stdint.h>

#include "csap.h"
#include "dsap.h"
#include "msap.h"
#include "attribute.h"
#include "wpc_constants.h"

typedef struct __attribute__((__packed__))
{
    uint8_t indication_status;
} generic_ind_pl_t;

/**
 * Payload definition for confirm primitives
 */
typedef struct __attribute__((__packed__))
{
    uint8_t result;
} sap_generic_conf_pl_t;

/**
 * Payload definition for response primitives
 */
typedef struct __attribute__((__packed__))
{
    uint8_t result;
} sap_resp_pl_t;

/*
 * Frame type definition
 */
typedef struct __attribute__((__packed__))
{
    // Id of the primitive
    uint8_t primitive_id;
    // Id of the frame
    uint8_t frame_id;
    // Lenght of the payload in bytes
    uint8_t payload_length;
    // Payload of the frame
    union {
        // Request
        dsap_data_tx_req_pl_t dsap_data_tx_request_payload;
        dsap_data_tx_tt_req_pl_t dsap_data_tx_tt_request_payload;
        msap_stack_start_req_pl_t msap_stack_start_request_payload;
        msap_app_config_data_write_req_pl_t msap_app_config_data_write_request_payload;
        msap_attribute_write_req_pl_t sap_attribute_write_request_payload;
        msap_attribute_read_req_pl_t msap_attribute_read_request_payload;
        msap_image_start_req_pl_t msap_image_start_request_payload;
        msap_image_block_req_pl_t msap_image_block_request_payload;
        msap_image_remote_status_req_pl_t msap_image_remote_status_request_payload;
        msap_image_remote_update_req_pl_t msap_image_remote_update_request_payload;
        msap_sink_cost_write_req_pl_t msap_sink_cost_write_request_payload;
        attribute_write_req_pl_t attribute_write_request_payload;
        attribute_read_req_pl_t attribute_read_request_payload;
        csap_factory_reset_req_pl_t csap_factory_reset_request_payload;
        msap_scratchpad_write_req_pl_t msap_scratchpad_target_write_request_payload;
        // Indication
        dsap_data_tx_ind_pl_t dsap_data_tx_indication_payload;
        dsap_data_rx_ind_pl_t dsap_data_rx_indication_payload;
        msap_stack_state_ind_pl_t msap_stack_state_indication_payload;
        msap_app_config_data_rx_ind_pl_t msap_app_config_data_rx_indication_payload;
        msap_image_remote_status_ind_pl_t msap_image_remote_status_indication_payload;
        msap_scan_nbors_ind_pl_t msap_scan_nbors_indication_payload;
        generic_ind_pl_t generic_indication_payload;
        // Confirm
        sap_generic_conf_pl_t sap_generic_confirm_payload;
        dsap_data_tx_conf_pl_t dsap_data_tx_confirm_payload;
        msap_app_config_data_read_conf_pl_t msap_app_config_data_read_confirm_payload;
        msap_attribute_read_conf_pl_t msap_attribute_read_confirm_payload;
        msap_sink_cost_read_conf_pl_t msap_sink_cost_read_confirm_payload;
        msap_get_nbors_conf_pl_t msap_get_nbors_confirm_payload;
        msap_scratchpad_status_conf_pl_t msap_scratchpad_status_confirm_payload;
        attribute_read_conf_pl_t attribute_read_confirm_payload;
        msap_scratchpad_read_conf_pl_t msap_scratchpad_target_read_confirm_payload;
        // Response
        sap_resp_pl_t sap_response_payload;
    } payload;
} wpc_frame_t;

// Size of a given frame
#define FRAME_SIZE(__frame_ptr__) ((__frame_ptr__)->payload_length + 3)

// Max frame size (including crc)
#define MAX_FRAME_SIZE (sizeof(wpc_frame_t) + 2)

// Max slip frame size (can be 2 times bigger than a frame if every bytes are
// escaped)
#define MAX_SLIP_FRAME_SIZE (2 * MAX_FRAME_SIZE)

// Define the function type to handle a received frame from the stack (confirm
// or indication)
typedef int (*frame_handler)(wpc_frame_t * frame);

#endif
