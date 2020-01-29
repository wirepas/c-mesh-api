# wpc.pxd - Cython API definition for Wirepas C Mesh API library

from libc.stdint cimport uint8_t, uint16_t, uint32_t
from libcpp cimport bool

cdef enum:
    MAXIMUM_NUMBER_OF_NEIGHBOR      = 8
    APP_CONFIG_DATA_MAX_NUM_BYTES   = 1024 # TODO: Needs to be big enough
    CIPHER_KEY_NUM_BYTES            = 16   # TODO: Have as #define in the C side
    AUTHENTICATION_KEY_NUM_BYTES    = 16   # TODO: Ditto

cdef extern from "wpc.h":

    ctypedef uint32_t app_addr_t

    ctypedef uint8_t app_role_t

    ctypedef uint32_t net_addr_t

    ctypedef uint8_t net_channel_t

    ctypedef enum app_role_e:
        APP_ROLE_SINK
        APP_ROLE_HEADNODE
        APP_ROLE_SUBNODE
        APP_ROLE_UNKNOWN

    ctypedef enum app_role_option_e:
        APP_ROLE_OPTION_LL
        APP_ROLE_OPTION_RELAY
        APP_ROLE_OPTION_AUTOROLE

    ctypedef enum app_qos_e:
        APP_QOS_NORMAL
        APP_QOS_HIGH

    ctypedef enum app_special_addr_e:
        APP_ADDR_ANYSINK
        APP_ADDR_BROADCAST

    ctypedef enum app_stack_state_e:
        APP_STACK_STOPPED
        APP_STACK_NETWORK_ADDRESS_NOT_SET
        APP_STACK_NODE_ADDRESS_NOT_SET
        APP_STACK_NETWORK_CHANNEL_NOT_SET
        APP_STACK_ROLE_NOT_SET

    ctypedef enum app_res_e:
        APP_RES_OK
        APP_RES_STACK_NOT_STOPPED
        APP_RES_STACK_ALREADY_STOPPED
        APP_RES_STACK_ALREADY_STARTED
        APP_RES_INVALID_VALUE
        APP_RES_ROLE_NOT_SET
        APP_RES_NODE_ADD_NOT_SET
        APP_RES_NET_ADD_NOT_SET
        APP_RES_NET_CHAN_NOT_SET
        APP_RES_STACK_IS_STOPPED
        APP_RES_NODE_NOT_A_SINK
        APP_RES_UNKNOWN_DEST
        APP_RES_NO_CONFIG
        APP_RES_ALREADY_REGISTERED
        APP_RES_NOT_REGISTERED
        APP_RES_ATTRIBUTE_NOT_SET
        APP_RES_ACCESS_DENIED
        APP_RES_DATA_ERROR
        APP_RES_NO_SCRATCHPAD_START
        APP_RES_NO_VALID_SCRATCHPAD
        APP_RES_NOT_A_SINK
        APP_RES_OUT_OF_MEMORY
        APP_RES_INVALID_DIAG_INTERVAL
        APP_RES_INVALID_SEQ
        APP_RES_INVALID_START_ADDRESS
        APP_RES_INVALID_NUMBER_OF_BYTES
        APP_RES_INVALID_SCRATCHPAD
        APP_RES_INVALID_REBOOT_DELAY
        APP_RES_INTERNAL_ERROR

    ctypedef struct app_scratchpad_status_t:
        uint32_t scrat_len
        uint16_t scrat_crc
        uint8_t scrat_seq_number
        uint8_t scrat_type
        uint8_t scrat_status
        uint32_t processed_scrat_len
        uint16_t processed_scrat_crc
        uint8_t processed_scrat_seq_number
        uint32_t firmware_memory_area_id
        uint8_t firmware_major_ver
        uint8_t firmware_minor_ver
        uint8_t firmware_maint_ver
        uint8_t firmware_dev_ver

    ctypedef struct app_nbor_info_t:
        uint32_t add
        uint8_t link_rel
        uint8_t norm_rssi
        uint8_t cost
        uint8_t channel
        uint8_t nbor_type
        uint8_t tx_power
        uint8_t rx_power
        uint16_t last_update

    ctypedef void (*onDataSent_cb_f)(uint16_t pduid, uint32_t buffering_delay, uint8_t result)

    ctypedef struct app_message_t:
        const uint8_t* bytes
        app_addr_t dst_addr
        onDataSent_cb_f on_data_sent_cb
        uint32_t buffering_delay
        uint16_t pdu_id
        uint8_t num_bytes
        uint8_t src_ep
        uint8_t dst_ep
        uint8_t hop_limit
        app_qos_e qos
        bool is_unack_csma_ca

    ctypedef struct app_nbors_t:
        uint8_t number_of_neighbors
        app_nbor_info_t nbors[8]

    app_res_e WPC_initialize(const char* port_name, unsigned long bitrate)

    void WPC_close()

    app_res_e WPC_set_max_poll_fail_duration(unsigned long duration_s)

    app_res_e WPC_get_role(app_role_t* role_p)

    app_res_e WPC_set_role(app_role_t role)

    app_res_e WPC_get_node_address(app_addr_t* addr_p)

    app_res_e WPC_set_node_address(app_addr_t add)

    app_res_e WPC_get_network_address(net_addr_t* addr_p)

    app_res_e WPC_set_network_address(net_addr_t add)

    app_res_e WPC_get_network_channel(net_channel_t* channel_p)

    app_res_e WPC_set_network_channel(net_channel_t channel)

    app_res_e WPC_get_mtu(uint8_t* value_p)

    app_res_e WPC_get_pdu_buffer_size(uint8_t* value_p)

    app_res_e WPC_get_scratchpad_sequence(uint8_t* value_p)

    app_res_e WPC_get_mesh_API_version(uint16_t* value_p)

    app_res_e WPC_set_cipher_key(const uint8_t key[16])

    app_res_e WPC_is_cipher_key_set(bool* set_p)

    app_res_e WPC_remove_cipher_key()

    app_res_e WPC_set_authentication_key(const uint8_t key[16])

    app_res_e WPC_is_authentication_key_set(bool* set_p)

    app_res_e WPC_remove_authentication_key()

    app_res_e WPC_get_firmware_version(uint16_t version[4])

    app_res_e WPC_get_channel_limits(uint8_t* first_channel_p, uint8_t* last_channel_p)

    app_res_e WPC_do_factory_reset()

    app_res_e WPC_get_hw_magic(uint16_t* value_p)

    app_res_e WPC_get_stack_profile(uint16_t* value_p)

    app_res_e WPC_get_channel_map(uint32_t* value_p)

    app_res_e WPC_set_channel_map(uint32_t channel_map)

    app_res_e WPC_start_stack()

    app_res_e WPC_stop_stack()

    app_res_e WPC_get_app_config_data_size(uint8_t* value_p)

    app_res_e WPC_set_app_config_data(uint8_t seq, uint16_t interval, const uint8_t* config_p, uint8_t size)

    app_res_e WPC_get_app_config_data(uint8_t* seq_p, uint16_t* interval_p, uint8_t* config_p, uint8_t size)

    app_res_e WPC_set_sink_cost(uint8_t cost)

    app_res_e WPC_get_sink_cost(uint8_t* cost_p)

    ctypedef void (*onAppConfigDataReceived_cb_f)(uint8_t seq, uint16_t interval, const uint8_t* config_p)

    app_res_e WPC_register_for_app_config_data(onAppConfigDataReceived_cb_f onAppConfigDataReceived)

    app_res_e WPC_unregister_from_app_config_data()

    app_res_e WPC_get_stack_status(uint8_t* status_p)

    app_res_e WPC_get_PDU_buffer_usage(uint8_t* usage_p)

    app_res_e WPC_get_PDU_buffer_capacity(uint8_t* capacity_p)

    app_res_e WPC_get_remaining_energy(uint8_t* energy_p)

    app_res_e WPC_set_remaining_energy(uint8_t energy)

    app_res_e WPC_get_autostart(uint8_t* enable_p)

    app_res_e WPC_set_autostart(uint8_t enable)

    app_res_e WPC_get_route_count(uint8_t* count_p)

    app_res_e WPC_get_system_time(uint32_t* time_p)

    app_res_e WPC_get_access_cycle_range(uint16_t* min_ac_p, uint16_t* max_ac_p)

    app_res_e WPC_set_access_cycle_range(uint16_t min_ac, uint16_t max_ac)

    app_res_e WPC_get_access_cycle_limits(uint16_t* min_ac_l_p, uint16_t* max_ac_l_p)

    app_res_e WPC_get_current_access_cycle(uint16_t* cur_ac_p)

    app_res_e WPC_get_scratchpad_block_max(uint8_t* max_size_p)

    app_res_e WPC_get_local_scratchpad_status(app_scratchpad_status_t* status)

    app_res_e WPC_start_local_scratchpad_update(uint32_t len, uint8_t seq)

    app_res_e WPC_upload_local_block_scratchpad(uint32_t len, const uint8_t* bytes, uint32_t start)

    app_res_e WPC_upload_local_scratchpad(uint32_t len, const uint8_t* bytes, uint8_t seq)

    app_res_e WPC_clear_local_scratchpad()

    app_res_e WPC_update_local_scratchpad()

    app_res_e WPC_get_remote_status(app_addr_t destination_address)

    app_res_e WPC_remote_scratchpad_update(app_addr_t destination_address, uint8_t seq, uint16_t reboot_delay_s)

    app_res_e WPC_start_scan_neighbors()

    app_res_e WPC_get_neighbors(app_nbors_t* nbors_list_p)

    app_res_e WPC_send_data(const uint8_t* bytes, uint8_t num_bytes, uint16_t pdu_id, app_addr_t dst_addr, app_qos_e qos, uint8_t src_ep, uint8_t dst_ep, onDataSent_cb_f on_data_sent_cb, uint32_t buffering_delay)

    app_res_e WPC_send_data_with_options(app_message_t* message_p)

    ctypedef bool (*onDataReceived_cb_f)(const uint8_t* bytes, uint8_t num_bytes, app_addr_t src_addr, app_addr_t dst_addr, app_qos_e qos, uint8_t src_ep, uint8_t dst_ep, uint32_t travel_time, uint8_t hop_count, unsigned long long timestamp_ms_epoch)

    app_res_e WPC_register_for_data(uint8_t dst_ep, onDataReceived_cb_f onDataReceived)

    app_res_e WPC_unregister_for_data(uint8_t dst_ep)

    ctypedef void (*onRemoteStatus_cb_f)(app_addr_t source_address, const app_scratchpad_status_t* status, uint16_t request_timeout)

    app_res_e WPC_register_for_remote_status(onRemoteStatus_cb_f onRemoteStatusReceived)

    app_res_e WPC_unregister_for_remote_status()

    ctypedef void (*onScanNeighborsDone_cb_f)(uint8_t scan_ready)

    app_res_e WPC_register_for_scan_neighbors_done(onScanNeighborsDone_cb_f onScanNeighborDone)

    app_res_e WPC_unregister_from_scan_neighbors_done()

    ctypedef void (*onStackStatusReceived_cb_f)(uint8_t status)

    app_res_e WPC_register_for_stack_status(onStackStatusReceived_cb_f onStackStatusReceived)

    app_res_e WPC_unregister_from_stack_status()
