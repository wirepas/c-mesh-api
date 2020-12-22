/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#ifndef WPC_H__
#define WPC_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * \brief   Default bitrate
 */
#define DEFAULT_BITRATE 115200

/**
 * \brief   Device address definition
 */
typedef uint32_t app_addr_t;

/**
 * \brief   Device role definition
 */
typedef uint8_t app_role_t;

/**
 * \brief   Network address definition
 */
typedef uint32_t net_addr_t;

/**
 * \brief   Network channel definition
 */
typedef uint8_t net_channel_t;

/**
 * \brief   Role of the device
 */
typedef enum
{
    APP_ROLE_SINK = 1,      //!< Node is sink
    APP_ROLE_HEADNODE = 2,  //!< Node is headnode
    APP_ROLE_SUBNODE = 3,   //!< Node is subnode (no routing of other's traffic)
    APP_ROLE_UNKNOWN        //!< Erroneous role
} app_role_e;

#define APP_ROLE_MASK 0xf

/**
 * \brief Role options
 */
typedef enum
{
    APP_ROLE_OPTION_LL = 0x10,       // !< Node is in low latency
    APP_ROLE_OPTION_RELAY = 0x20,    // !< Node is only a relay (Deprecated)
    APP_ROLE_OPTION_AUTOROLE = 0x80  // !< Node is autorole
} app_role_option_e;

#define APP_ROLE_OPTION_MASK 0xf0

/**
 * Various macros to manipulate roles
 */
#define GET_BASE_ROLE(role) (role & APP_ROLE_MASK)
#define GET_ROLE_OPTIONS(role) (role & APP_ROLE_OPTION_MASK)
#define CREATE_ROLE(base, options) \
    ((base & APP_ROLE_MASK) | (options & APP_ROLE_OPTION_MASK))

/**
 * \brief   Application data QoS
 */
typedef enum
{
    APP_QOS_NORMAL = 0,  //!< Normal priority
    APP_QOS_HIGH = 1,    //!< High priority
} app_qos_e;

/**
 * \brief   Reserved addresses
 */
typedef enum
{
    APP_ADDR_ANYSINK = 0,             //!< Address is to any sink
    APP_ADDR_BROADCAST = 0xffffffff,  //!< Address is broadcast to all
} app_special_addr_e;

/**
 * \brief   Stack state flags
 */
typedef enum
{
    APP_STACK_STOPPED = 1,
    APP_STACK_NETWORK_ADDRESS_NOT_SET = 2,
    APP_STACK_NODE_ADDRESS_NOT_SET = 4,
    APP_STACK_NETWORK_CHANNEL_NOT_SET = 8,
    APP_STACK_ROLE_NOT_SET = 16
} app_stack_state_e;

/**
 * \brief   Return code
 */
typedef enum
{
    APP_RES_OK,                     //!< Everything is ok
    APP_RES_STACK_NOT_STOPPED,      //!< Stack is not stopped
    APP_RES_STACK_ALREADY_STOPPED,  //!< Stack is already stopped
    APP_RES_STACK_ALREADY_STARTED,  //!< Stack is already started
    APP_RES_INVALID_VALUE,          //!< A parameter has an invalid value
    APP_RES_ROLE_NOT_SET,           //!< The node role is not set
    APP_RES_NODE_ADD_NOT_SET,       //!< The node address is not set
    APP_RES_NET_ADD_NOT_SET,        //!< The network address is not set
    APP_RES_NET_CHAN_NOT_SET,       //!< The network channel is not set
    APP_RES_STACK_IS_STOPPED,       //!< Stack is stopped
    APP_RES_NODE_NOT_A_SINK,        //!< Node is not a sink
    APP_RES_UNKNOWN_DEST,           //!< Unknown destination address
    APP_RES_NO_CONFIG,              //!< No configuration received/set
    APP_RES_ALREADY_REGISTERED,     //!< Cannot register several times
    APP_RES_NOT_REGISTERED,       //!< Cannot unregister if not registered first
    APP_RES_ATTRIBUTE_NOT_SET,    //!< Attribute is not set yet
    APP_RES_ACCESS_DENIED,        //!< Access denied
    APP_RES_DATA_ERROR,           //!< Error in data
    APP_RES_NO_SCRATCHPAD_START,  //!< No scratchpad start request sent
    APP_RES_NO_VALID_SCRATCHPAD,  //!< No valid scratchpad
    APP_RES_NOT_A_SINK,           //!< Stack is not a sink
    APP_RES_OUT_OF_MEMORY,        //!< Out of memory
    APP_RES_INVALID_DIAG_INTERVAL,    //!< Invalid diag interval
    APP_RES_INVALID_SEQ,              //!< Invalid sequence number
    APP_RES_INVALID_START_ADDRESS,    //!< Start address is invalid
    APP_RES_INVALID_NUMBER_OF_BYTES,  //!< Invalid number of bytes
    APP_RES_INVALID_SCRATCHPAD,       //!< Scratchpad is not valid
    APP_RES_INVALID_REBOOT_DELAY,     //!< Invalid reboot delay
    APP_RES_INTERNAL_ERROR            //!< WPC internal error
} app_res_e;

/**
 * \brief   Scratchpad status
 */
typedef struct
{
    uint32_t scrat_len;            //!< Stored scratchpad length in bytes
    uint16_t scrat_crc;            //!< Stored scratchpad crc
    uint8_t scrat_seq_number;      //!< Stored scratchpad sequence number
    uint8_t scrat_type;            //!< Stored scratchpad type
    uint8_t scrat_status;          //!< Stored scratchpad status
    uint32_t processed_scrat_len;  //!< Processed scratchpad length in bytes
    uint16_t processed_scrat_crc;  //!< Processed scratchpad crc
    uint8_t processed_scrat_seq_number;  //!< Processed scratchpad sequence number
    uint32_t firmware_memory_area_id;    //!< Firmware memory id
    uint8_t firmware_major_ver;  //!< Firmware major version of running firmware
    uint8_t firmware_minor_ver;  //!< Firmware minor version of running firmware
    uint8_t firmware_maint_ver;  //!< Firmware maintenance version of running
                                 //!< firmware
    uint8_t firmware_dev_ver;    //!< Firmware development version of running
                                 //!< firmware
} app_scratchpad_status_t;

/**
 * \brief   Neighbor info type
 */
typedef struct
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
} app_nbor_info_t;

/**
 * \brief   Callback definition to receive data sent status
 * \param   pduid
 *          Pduid set in send_data
 * \param   buffering_delay
 *          Time spent in stack buffers in ms
 * \param   result
 *          Result of the operation: 0 for success, 1 for failure
 */
typedef void (*onDataSent_cb_f)(uint16_t pduid, uint32_t buffering_delay, uint8_t result);

/**
 * \brief   Message to send
 */
typedef struct
{
    const uint8_t * bytes;            //!< Payload
    app_addr_t dst_addr;              //!< Destination address
    onDataSent_cb_f on_data_sent_cb;  //!< Callback to call when message is
                                      //!< sent (can be NULL)
    uint32_t buffering_delay;         //!< Initial buffering delay
    uint16_t pdu_id;                  //!< Pdu id (only needed if cb set)
    uint8_t num_bytes;                //!< Size of payload
    uint8_t src_ep;                   //!< Source endpoint
    uint8_t dst_ep;                   //!< Destination endpoint
    uint8_t hop_limit;                //!< Hop limit for this transmission
    app_qos_e qos;                    //!< QoS to use for transmission
    bool is_unack_csma_ca;            //!< If true, only sent to CB-MAC nodes
} app_message_t;

/**
 * \brief   Maximum number of neighbors (defined in protocol)
 */
#define MAXIMUM_NUMBER_OF_NEIGHBOR 8

/**
 * \brief   Neighbors info list
 */
typedef struct
{
    uint8_t number_of_neighbors;
    app_nbor_info_t nbors[MAXIMUM_NUMBER_OF_NEIGHBOR];
} app_nbors_t;

/**
 * \brief   Intialize the Wirepas Mesh serial communication
 * \param   port_name
 *          the name of the serial port ("/dev/ttyACM0" for example)
 * \param   bitrate
 *          bitrate in bits per second, e.g. \ref DEFAULT_BITRATE
 */
app_res_e WPC_initialize(char * port_name, unsigned long bitrate);

/**
 * \brief   Stop the Wirepas Mesh serial communication
 */
void WPC_close(void);

/**
 * \brief   Set maximum poll fail duration
 * \param   duration_s
 *          the maximum poll fail duration in seconds,
 *          zero equals forever
 * \return  Return code of the operation
 */
app_res_e WPC_set_max_poll_fail_duration(unsigned long duration_s);

/**
 * \brief   Get the role of the node
 * \param   app_role_e
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_role(app_role_t * role_p);

/**
 * \brief   Change the role of the node
 * \param   role
 *          New role
 * \return  Return code of the operation
 */
app_res_e WPC_set_role(app_role_t role);

/**
 * \brief   Get the node address
 * \param   addr_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_node_address(app_addr_t * addr_p);

/**
 * \brief   Set the node address
 * \param   add
 *          Own node address to set
 * \return  Return code of the operation
 */
app_res_e WPC_set_node_address(app_addr_t add);

/**
 * \brief   Get the network address
 * \param   addr_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_network_address(net_addr_t * addr_p);

/**
 * \brief   Set the network address
 * \param   add
 *          Network address to set
 * \return  Return code of the operation
 */
app_res_e WPC_set_network_address(net_addr_t add);

/**
 * \brief   Get network channel
 * \param   channel_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_network_channel(net_channel_t * channel_p);

/**
 * \brief   Set the network channel
 * \param   channel
 *          Network channel to set
 * \return  Return code of the operation
 */
app_res_e WPC_set_network_channel(net_channel_t channel);

/**
 * \brief   Get the maximum transmission unit size
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_mtu(uint8_t * value_p);

/**
 * \brief   Get the maximum size of the stack PDU buffers
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_pdu_buffer_size(uint8_t * value_p);

/**
 * \brief   Get the sequence number of the OTAP scratchpad present in the node
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_scratchpad_sequence(uint8_t * value_p);

/**
 * \brief   Get the mesh API version
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_mesh_API_version(uint16_t * value_p);

/**
 * \brief   Set the cipher key
 * \param   key
 *          The new key to set
 * \return  Return code of the operation
 */
app_res_e WPC_set_cipher_key(uint8_t key[16]);

/**
 * \brief   Check if cipher key is set
 * \param   set_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_is_cipher_key_set(bool * set_p);

/**
 * \brief   Remove the cipher key
 * \return  Return code of the operation
 */
app_res_e WPC_remove_cipher_key();

/**
 * \brief   Set the authentication key
 * \param   key
 *          The new key to set
 * \return  Return code of the operation
 */
app_res_e WPC_set_authentication_key(uint8_t key[16]);

/**
 * \brief   Check if authentication key is set
 * \param   set_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_is_authentication_key_set(bool * set_p);

/**
 * \brief   Remove the authentication key
 * \return  Return code of the operation
 */
app_res_e WPC_remove_authentication_key();

/**
 * \brief   Get the Firmware version
 * \param   value_p
 *          Pointer to store the result
 *          The result is a 8 bytes array with the following format:
 *          Major:Minor:Maintenance:Development (each field is two bytes)
 * \return  Return code of the operation
 */
app_res_e WPC_get_firmware_version(uint16_t version[4]);

/**
 * \brief   Get the allowed range of network channels
 * \param   first_channel_p
 *          Pointer to store the first available channel
 * \param   last_channel_p
 *          Pointer to store the last available channel
 * \return  Return code of the operation
 */
app_res_e WPC_get_channel_limits(uint8_t * first_channel_p, uint8_t * last_channel_p);

/**
 * \brief   Clear all persistent attributes
 * \return  Return code of the operation
 */
app_res_e WPC_do_factory_reset();

/**
 * \brief   Get the radio hardware used
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_hw_magic(uint16_t * value_p);

/**
 * \brief   Get the frequency band used
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_stack_profile(uint16_t * value_p);

/**
 * \brief   Get the channel map of device
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_channel_map(uint32_t * value_p);

/**
 * \brief   Set a new channel map
 * \param   channel_map
 *          New channel map
 * \return  Return code of the operation
 */
app_res_e WPC_set_channel_map(uint32_t channel_map);

/**
 * \brief   Start the stack
 * \return  Return code of the operation
 */
app_res_e WPC_start_stack(void);

/**
 * \brief   Stop the stack
 * \return  Return code of the operation
 */
app_res_e WPC_stop_stack(void);

/**
 * \brief   Get the app config max size
 * \param   value_p
 *          Pointer to store the result
 * \return  Return code of the operation
 */
app_res_e WPC_get_app_config_data_size(uint8_t * value_p);

/**
 * \brief   Set app config data
 * \param   seq
 *          The sequence of the app config data
 * \param   interval
 *          The interval for diagnostics generation
 * \param   config_p
 *          Pointer to new config data
 * \param   size
 *          Size of the app config data to write. It must be equal or
 *          lower to size returned by WPC_get_app_config_data_size
 * \return  Return code of the operation
 * \note    This call can only be made from a sink node
 */
app_res_e WPC_set_app_config_data(uint8_t seq, uint16_t interval, uint8_t * config_p, uint8_t size);

/**
 * \brief   Get app config data
 * \param   seq_p
 *          Pointer to store the sequence of the app config data
 * \param   interval_p
 *          Pointer to store the interval for diagnostics generation
 * \param   config_p
 *          Pointer to the read config data. This table must be able to contain
 *          size bytes.
 * \param   size
 *          Size of the app config data to update to config_p. It must be equal
 *          or lower to size returned by WPC_get_app_config_data_size
 * \return  Return code of the operation
 */
app_res_e
WPC_get_app_config_data(uint8_t * seq_p, uint16_t * interval_p, uint8_t * config_p, uint8_t size);

/**
 * \brief   Set sink cost
 * \param   cost
 *          The new initial cost in 0-254 range
 * \return  Return code of the operation
 * \note    This call can only be made from a sink node
 */
app_res_e WPC_set_sink_cost(uint8_t cost);

/**
 * \brief   Get sink cost
 * \param   cost_p
 *          pointer to read cost. Updated only if result APP_RES_OK
 * \return  Return code of the operation
 * \note    This call can only be made from a sink node
 */
app_res_e WPC_get_sink_cost(uint8_t * cost_p);

/**
 * \brief   Callback definition to register for app config data notification
 * \param   seq_p
 *          Sequence of the app config data received
 * \param   interval
 *          Interval for diagnostics
 * \param   config_p
 *          Pointer to the received config data
 *          Size is equal to returned size by WPC_get_app_config_data_size
 */
typedef void (*onAppConfigDataReceived_cb_f)(uint8_t seq, uint16_t interval, uint8_t * config_p);

/**
 * \brief   Register for receiving app config data
 * \param   onAppConfigDataReceived
 *          The callback to call when app config data is received
 * \note    All the registered callback share the same thread,
 *          so the handling of it must be kept as simple
 *          as possible or dispatched to another thread for long operations.
 */
app_res_e WPC_register_for_app_config_data(onAppConfigDataReceived_cb_f onAppConfigDataReceived);

/**
 * \brief   Unregister for receiving app config data
 * \return  Return code of the operation
 */
app_res_e WPC_unregister_from_app_config_data();

/**
 * \brief   Get the stack status
 * \param   status_p
 *          Pointer to the stack status bit field
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 * \note    See WP-RM-100 document for more details
 */
app_res_e WPC_get_stack_status(uint8_t * status_p);

/**
 * \brief   Get number of PDU buffers currently stored
 *          in the stack
 * \param   usage_p
 *          Pointer to the buffer usage
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_PDU_buffer_usage(uint8_t * usage_p);

/**
 * \brief   Get number of free PDU buffers in the stack
 * \param   capacity_p
 *          Pointer to the buffer capacity
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_PDU_buffer_capacity(uint8_t * capacity_p);

/**
 * \brief   Get the remaining energy (set by application)
 * \param   energy_p
 *          Pointer to the remaining energy
 *          Updated if return is APP_RES_OK
 * \return  Return code of the operation
 * \note    See WP-RM-100 document for more details
 */
app_res_e WPC_get_remaining_energy(uint8_t * energy_p);

/**
 * \brief   Set the remaining energy
 * \param   energy
 *          New energy value in 0 - 255 range
 * \return  Return code of the operation
 * \note    See WP-RM-100 document for more details
 */
app_res_e WPC_set_remaining_energy(uint8_t energy);

/**
 * \brief   Get the autostart state
 * \param   enable_p
 *          Pointer to the autostart status
 *          Updated if return is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_autostart(uint8_t * enable_p);

/**
 * \brief   Set the autostart state
 * \param   enable
 *          True to enable autostart, false otherwise
 * \return  Return code of the operation
 * \note    See WP-RM-100 document for more details
 */
app_res_e WPC_set_autostart(uint8_t enable);

/**
 * \brief   Get the route count
 * \param   count_p
 *          Pointer to the route count
 *          Updated if return is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_route_count(uint8_t * count_p);

/**
 * \brief   Get system time
 * \param   time_p
 *          Pointer to the system time
 *          Updated if return is APP_RES_OK
 * \return  Return time since last startup in second
 */
app_res_e WPC_get_system_time(uint32_t * time_p);

/**
 * \brief   Get the current access cycle range
 * \param   min_ac_p
 *          Pointer to the minimum access cycle
 *          Updated if return code is APP_RES_OK
 * \param   max_ac_p
 *          Pointer to the maximum access cycle
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_access_cycle_range(uint16_t * min_ac_p, uint16_t * max_ac_p);

/**
 * \brief   Set the current access cycle range
 * \param   min_ac
 *          New minimum access cycle
 * \param   max_ac
 *          New maximum access cycle
 * \return  Return code of the operation
 */
app_res_e WPC_set_access_cycle_range(uint16_t min_ac, uint16_t max_ac);

/**
 * \brief   Get the current access cycle limits range
 * \param   min_ac_l_p
 *          Pointer to the minimum access cycle
 *          Updated if return code is APP_RES_OK
 * \param   max_ac_l_p
 *          Pointer to the maximum access cycle
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_access_cycle_limits(uint16_t * min_ac_l_p, uint16_t * max_ac_l_p);

/**
 * \brief   Get the current access cycle
 * \param   cur_ac_p
 *          Pointer to the current access cycle
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_current_access_cycle(uint16_t * cur_ac_p);

/**
 * \brief   Get the maximun number of bytes in a scratchpad block
 * \param   max_size_p
 *          Pointer to the max size
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_scratchpad_block_max(uint8_t * max_size_p);

/**
 * \brief   Get the local scratchpad status
 * \param   status
 *          Status of the currently stored scratchpad
 *          Updated if return code is APP_RES_OK
 * \return  Return code of the operation
 */
app_res_e WPC_get_local_scratchpad_status(app_scratchpad_status_t * status);

/**
 * \brief   Start the process of clearing and rewriting the Scratchpad contents
 *          of this node
 * \param   len
 *          Lenght of the scratchpad to upload
 * \param   seq
 *          Sequence of the scratchpad to upload
 * \return  Return code of the operation
 */
app_res_e WPC_start_local_scratchpad_update(uint32_t len, uint8_t seq);

/**
 * \brief   Upload a scratchpad block
 * \param   len
 *          Lenght of the block (must be a multiple of 4 bytes)
 * \param   bytes
 *          Block content
 * \param   start
 *          Offset of the block relatively to the beginning of scratchpad
 * \return  Return code of the operation
 */
app_res_e WPC_upload_local_block_scratchpad(uint32_t len, uint8_t * bytes, uint32_t start);

/**
 * \brief   Upload a full scratchpad
 * \param   len
 *          Lenght of the scratchpad (must be a multiple of 4 bytes)
 * \param   bytes
 *          scratchpad content
 * \param   seq
 *          Sequence of the scratchpad to upload
 * \return  Return code of the operation
 */
app_res_e WPC_upload_local_scratchpad(uint32_t len, uint8_t * bytes, uint8_t seq);

/**
 * \brief   Clear the local stored scratchpad
 * \return  Return code of the operation
 */
app_res_e WPC_clear_local_scratchpad();

/**
 * \brief   Mark the scratchpad for processing by the bootloader.
 *          The bootloader will process the scratchpad contents on next reboot
 * \return  Return code of the operation
 */
app_res_e WPC_update_local_scratchpad();

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
 * \return  Return code of the operation
 */
app_res_e WPC_write_target_scratchpad(uint8_t target_sequence,
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
 * \return  Return code of the operation
 */
app_res_e WPC_read_target_scratchpad(uint8_t * target_sequence_p,
                                     uint16_t * target_crc_p,
                                     uint8_t * action_p,
                                     uint8_t * param_p);

/**
 * \brief   Query scratchpad status for a remote node
 * \param   destination_address
 *          Destination node address
 * \return  Return code of the operation
 */
app_res_e WPC_get_remote_status(app_addr_t destination_address);

/**
 * \brief   Start a countdown on a remote node to take a stored
 *          scratchpad into use
 * \param   destination_address
 *          Destination node address of the command
 * \param   seq
 *          Scratchpad sequence to proceed
 * \param   reboot_delay_s
 *          Number of seconds before the scratchpad is marked for
 *          processing and the node is rebooted
 *          Setting the reboot delay to 0 cancels an ongoing update
 * \return  Return code of the operation
 */
app_res_e WPC_remote_scratchpad_update(app_addr_t destination_address,
                                       uint8_t seq,
                                       uint16_t reboot_delay_s);

/**
 * \brief   Start a neighbors scan
 * \return  Return code of the operation
 * \note    A callback can be registered to be called once the scan is done.
 *          See WPC_register_for_scan_neighbors_done
 * \note    Doing a scan consumes quite a lot of power and must be used with
 *          care on battery operated devices
 */
app_res_e WPC_start_scan_neighbors();

/**
 * \brief   Get the list of the current neighbors nodes
 * \param   nbors_list_p
 *          Pointer to the neighbors list
 * \return  Return code of the operation
 * \note    This list is regularly updated by the stack for its needs. To force
 *          an update of this list, WPC_start_scan_neighbors can be used
 */
app_res_e WPC_get_neighbors(app_nbors_t * nbors_list_p);

/**
 * \brief   Send application data packet
 * \param   bytes
 *          Data payload
 * \param   num_bytes
 *          Amount of data payload (in bytes)
 * \param   pdu_id
 *          Id of the pdu
 * \param   dst_addr
 *          Destination address
 * \param   qos
 *          Quality of service
 * \param   src_ep
 *          Data source endpoint
 * \param   dst_ep
 *          Data destination endpoint
 * \param   buffering_delay
 *          initial buffering delay in ms
 * \return  Return code of the operation
 */
app_res_e WPC_send_data(const uint8_t * bytes,
                        uint8_t num_bytes,
                        uint16_t pdu_id,
                        app_addr_t dst_addr,
                        app_qos_e qos,
                        uint8_t src_ep,
                        uint8_t dst_ep,
                        onDataSent_cb_f on_data_sent_cb,
                        uint32_t buffering_delay);

/**
 * \brief   Send application data packet
 * \param   message_p
 *          The message to send
 * \return  Return code of the operation
 */
app_res_e WPC_send_data_with_options(app_message_t * message_p);

/**
 * \brief   Callback definition to register for received data
 * \param   bytes
 *          Buffer of received data
 * \param   num_bytes
 *          Number of bytes in the buffer
 * \param   src_addr
 *          Source address
 * \param   dst_addr
 *          Destination address
 * \param   qos
 *          Quality of Service used
 * \param   src_ep
 *          Data source endpoint
 * \param   dst_ep
 *          Data destination endpoint
 * \param   travel_time
 *          Time elapsed since the data sending in ms
 * \param   hop count
 *          How many hops were needed to transfer the data
 * \return  true if data is handled
 */
typedef bool (*onDataReceived_cb_f)(const uint8_t * bytes,
                                    uint8_t num_bytes,
                                    app_addr_t src_addr,
                                    app_addr_t dst_addr,
                                    app_qos_e qos,
                                    uint8_t src_ep,
                                    uint8_t dst_ep,
                                    uint32_t travel_time,
                                    uint8_t hop_count,
                                    unsigned long long timestamp_ms_epoch);

/**
 * \brief   Register for receiving data on a given EP
 * \param   dst_ep
 *          The destination endpoint to register
 * \param   onDataReceived
 *          The callback to call when data is received
 * \note    The callback is called on a dedicated thread.
 *          All the registered callback share the same thread,
 *          so the handling of callback must be kept as simple
 *          as possible or dispatched to another thread for long operations.
 */
app_res_e WPC_register_for_data(uint8_t dst_ep, onDataReceived_cb_f onDataReceived);

/**
 * \brief   Unregister for receiving data
 * \param   dst_ep
 *          The destination endpoint to unregister
 * \return  Return code of the operation
 */
app_res_e WPC_unregister_for_data(uint8_t dst_ep);

/**
 * \brief   Callback definition to register for remote status update
 * \param   source_address
 *          Source node address
 * \param   status
 *          Remote node status
 * \param   request_timeout
 *          Number of seconds left before the OTAP scratchpad is marked
 *          to be processed and the node is rebooted
 *          0 means that no update request is ongoing
 */
typedef void (*onRemoteStatus_cb_f)(app_addr_t source_address,
                                    app_scratchpad_status_t * status,
                                    uint16_t request_timeout);

/**
 * \brief   Register for receiving remote status packets
 * \param   onRemoteStatusaReceived
 *          The callback to call when remote status is received
 * \note    The callback is called on a dedicated thread.
 *          All the registered callback share the same thread,
 *          so the handling of a callback must be kept as simple
 *          as possible or dispatched to another thread for long operations.
 */
app_res_e WPC_register_for_remote_status(onRemoteStatus_cb_f onRemoteStatusReceived);

/**
 * \brief   Unregister for receiving remote status packets
 * \return  Return code of the operation
 */
app_res_e WPC_unregister_for_remote_status();

/**
 * \brief   Callback definition to register for scan neighbors done
 * \param   scan_ready
 *          1 if scan is done
 */
typedef void (*onScanNeighborsDone_cb_f)(uint8_t scan_ready);

/**
 * \brief   Register for receiving scan neighbors done event
 * \param   onScanNeighborDone
 *          The callback to call when scan neighbor event is received
 * \note    The callback is called on a dedicated thread.
 *          All the registered callback share the same thread,
 *          so the handling of a callback must be kept as simple
 *          as possible or dispatched to another thread for long operations.
 */
app_res_e WPC_register_for_scan_neighbors_done(onScanNeighborsDone_cb_f onScanNeighborDone);

/**
 * \brief   Unregister from receiving scan neighbors done event
 * \return  Return code of the operation
 */
app_res_e WPC_unregister_from_scan_neighbors_done();

/**
 * \brief   Callback definition to register for stack status event
 * \param   scan_ready
 *          1 if scan is done
 */
typedef void (*onStackStatusReceived_cb_f)(uint8_t status);

/**
 * \brief   Register for receiving stack status event
 * \param   onStackStatusReceived
 *          The callback to call when stack status is received
 * \note    The callback is called on a dedicated thread.
 *          All the registered callback share the same thread,
 *          so the handling of a callback must be kept as simple
 *          as possible or dispatched to another thread for long operations.
 */
app_res_e WPC_register_for_stack_status(onStackStatusReceived_cb_f onStackStatusReceived);

/**
 * \brief   Unregister from receiving stack status event
 * \return  Return code of the operation
 */
app_res_e WPC_unregister_from_stack_status();

#endif
