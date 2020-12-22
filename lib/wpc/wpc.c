/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#define LOG_MODULE_NAME "wpc"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"
#include <string.h>
#include "csap.h"
#include "dsap.h"
#include "msap.h"
#include "util.h"

#include "wpc.h"  // For DEFAULT_BITRATE
#include "wpc_internal.h"
#include "platform.h"  // For Platform_get_timestamp_ms_epoch()

/**
 * \brief   Macro to convert dual_mcu return code
 *          to library error code based on a LUT
 *          Dual mcu return code are not harmonized so
 *          a different LUT must be used per fonction
 */
#define convert_error_code(LUT, error)          \
    ({                                          \
        app_res_e ret = APP_RES_INTERNAL_ERROR; \
        if (error >= 0 && error < sizeof(LUT))  \
        {                                       \
            ret = LUT[error];                   \
        }                                       \
        ret;                                    \
    })

/** \brief   Timeout to get a valid stack response after
 *           a stop stack. It can be quite long in case of
 *           the bootloader is processing a scratchpad
 *           (especially from an external memory)
*/
#define TIMEOUT_AFTER_STOP_STACK_S 60

app_res_e WPC_initialize(char * port_name, unsigned long bitrate)
{
    return WPC_Int_initialize(port_name, bitrate) == 0 ? APP_RES_OK : APP_RES_INTERNAL_ERROR;
}

void WPC_close(void)
{
    WPC_Int_close();
}

/* Error code LUT for reading attribute */
static const app_res_e ATT_READ_ERROR_CODE_LUT[] = {
    APP_RES_OK,                 // 0
    APP_RES_INTERNAL_ERROR,     // 1
    APP_RES_STACK_NOT_STOPPED,  // 2
    APP_RES_INTERNAL_ERROR,     // 3
    APP_RES_ATTRIBUTE_NOT_SET,  // 4
    APP_RES_ACCESS_DENIED,      // 5
    APP_RES_ACCESS_DENIED       // 6
};

/* Error code LUT for reading attribute */
static const app_res_e ATT_WRITE_ERROR_CODE_LUT[] = {
    APP_RES_OK,                 // 0
    APP_RES_INTERNAL_ERROR,     // 1
    APP_RES_STACK_NOT_STOPPED,  // 2
    APP_RES_INTERNAL_ERROR,     // 3
    APP_RES_INVALID_VALUE,      // 4
    APP_RES_INTERNAL_ERROR,     // 5
    APP_RES_ACCESS_DENIED       // 6
};

app_res_e WPC_set_max_poll_fail_duration(unsigned long duration_s)
{
    if (Platform_set_max_poll_fail_duration(duration_s))
    {
        return APP_RES_OK;
    }
    else
    {
        return APP_RES_ACCESS_DENIED;
    }
}

app_res_e WPC_get_role(app_role_t * role_p)
{
    int res = csap_attribute_read_request(C_NODE_ROLE_ID, 1, role_p);
    return convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
}

app_res_e WPC_set_role(app_role_t role)
{
    uint8_t att = role;
    int res = csap_attribute_write_request(C_NODE_ROLE_ID, 1, &att);
    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_node_address(app_addr_t * addr_p)
{
    app_res_e ret;
    uint8_t att[4];
    int res = csap_attribute_read_request(C_NODE_ADDRESS_ID, 4, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *addr_p = uint32_decode_le(att);
    return APP_RES_OK;
}

app_res_e WPC_set_node_address(app_addr_t add)
{
    uint8_t att[4];
    uint32_encode_le(add, att);
    int res = csap_attribute_write_request(C_NODE_ADDRESS_ID, 4, att);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_network_address(net_addr_t * addr_p)
{
    app_res_e ret;
    uint8_t att[4];

    // Highest byte is always 0 as network address are only 3 bytes
    att[3] = 00;
    int res = csap_attribute_read_request(C_NETWORK_ADDRESS_ID, 3, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *addr_p = uint32_decode_le(att);
    return APP_RES_OK;
}

app_res_e WPC_set_network_address(net_addr_t add)
{
    uint8_t att[4];
    uint32_encode_le(add, att);
    int res = csap_attribute_write_request(C_NETWORK_ADDRESS_ID, 3, att);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_network_channel(net_channel_t * channel_p)
{
    int res = csap_attribute_read_request(C_NETWORK_CHANNEL_ID, 1, channel_p);

    return convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
}

app_res_e WPC_set_network_channel(net_channel_t channel)
{
    uint8_t att;
    att = channel;
    int res = csap_attribute_write_request(C_NETWORK_CHANNEL_ID, 1, &att);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_mtu(uint8_t * value_p)
{
    int res = csap_attribute_read_request(C_MTU_ID, 1, value_p);
    return convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_pdu_buffer_size(uint8_t * value_p)
{
    int res = csap_attribute_read_request(C_PDU_BUFFER_SIZE_ID, 1, value_p);
    return convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_scratchpad_sequence(uint8_t * value_p)
{
    int res = csap_attribute_read_request(C_SCRATCHPAD_SEQUENCE_ID, 1, value_p);
    return convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_mesh_API_version(uint16_t * value_p)
{
    app_res_e ret;
    uint8_t att[2];
    int res = csap_attribute_read_request(C_MESH_API_VER_ID, 2, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *value_p = uint16_decode_le(att);
    return APP_RES_OK;
}

app_res_e WPC_get_firmware_version(uint16_t version[4])
{
    app_res_e ret;
    const uint8_t IDs[4] = {C_FIRMWARE_MAJOR_ID, C_FIRMWARE_MINOR_ID, C_FIRMWARE_MAINT_ID, C_FIRMWARE_DEV_ID};

    for (int i = 0; i < sizeof(IDs); i++)
    {
        uint8_t att[2];
        int res = csap_attribute_read_request(IDs[i], 2, att);

        ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
        if (ret != APP_RES_OK)
        {
            return ret;
        }

        version[i] = uint16_decode_le(att);
    }

    return APP_RES_OK;
}

app_res_e WPC_set_cipher_key(uint8_t key[16])
{
    int res = csap_attribute_write_request(C_CIPHER_KEY_ID, 16, key);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_is_cipher_key_set(bool * set_p)
{
    uint8_t key[16];
    // Try to read the key only to get the error code
    int res = csap_attribute_read_request(C_CIPHER_KEY_ID, 16, key);
    if (res < 0)
    {
        return APP_RES_INTERNAL_ERROR;
    }

    *set_p = (res == 5);
    return APP_RES_OK;
}

app_res_e WPC_remove_cipher_key()
{
    uint8_t disable_key[16];
    memset(disable_key, 0xFF, sizeof(disable_key));

    return WPC_set_cipher_key(disable_key);
}

app_res_e WPC_set_authentication_key(uint8_t key[16])
{
    int res = csap_attribute_write_request(C_AUTH_KEY_ID, 16, key);
    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_is_authentication_key_set(bool * set_p)
{
    uint8_t key[16];
    // Try to read the key only to get the error code
    int res = csap_attribute_read_request(C_AUTH_KEY_ID, 16, key);

    if (res < 0)
    {
        return APP_RES_INTERNAL_ERROR;
    }

    *set_p = (res == 5);
    return APP_RES_OK;
}

app_res_e WPC_remove_authentication_key()
{
    uint8_t disable_key[16];
    memset(disable_key, 0xFF, sizeof(disable_key));

    return WPC_set_authentication_key(disable_key);
}

app_res_e WPC_get_channel_limits(uint8_t * first_channel_p, uint8_t * last_channel_p)
{
    app_res_e ret;
    uint8_t att[2];
    int res = csap_attribute_read_request(C_CHANNEL_LIM_ID, 2, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *first_channel_p = att[0];
    *last_channel_p = att[1];
    return APP_RES_OK;
}

/* Error code LUT for factory reset */
static const app_res_e FACT_RESET_ERROR_CODE_LUT[] = {
    APP_RES_OK,                 // 0
    APP_RES_STACK_NOT_STOPPED,  // 1
    APP_RES_INTERNAL_ERROR,     // 2
    APP_RES_ACCESS_DENIED       // 3
};

app_res_e WPC_do_factory_reset()
{
    int res = csap_factory_reset_request();

    return convert_error_code(FACT_RESET_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_app_config_data_size(uint8_t * value_p)
{
    int res = csap_attribute_read_request(C_APP_CONFIG_DATA_SIZE_ID, 1, value_p);

    return convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_hw_magic(uint16_t * value_p)
{
    app_res_e ret;
    uint8_t att[2];
    int res = csap_attribute_read_request(C_HW_MAGIC, 2, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *value_p = uint16_decode_le(att);
    return APP_RES_OK;
}

app_res_e WPC_get_stack_profile(uint16_t * value_p)
{
    app_res_e ret;
    uint8_t att[2];
    int res = csap_attribute_read_request(C_STACK_PROFILE, 2, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *value_p = uint16_decode_le(att);
    return APP_RES_OK;
}

app_res_e WPC_get_channel_map(uint32_t * value_p)
{
    app_res_e ret;
    uint8_t att[4];
    int res = csap_attribute_read_request(C_CHANNEL_MAP, 4, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *value_p = uint32_decode_le(att);
    return APP_RES_OK;
}

app_res_e WPC_set_channel_map(uint32_t channel_map)
{
    uint8_t att[4];
    uint32_encode_le(channel_map, att);
    int res = csap_attribute_write_request(C_CHANNEL_MAP, 4, att);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

/* Error code LUT for app_config read */
static const app_res_e APP_CONFIG_READ_ERROR_CODE_LUT[] = {
    APP_RES_OK,            // 0
    APP_RES_NO_CONFIG,     // 1
    APP_RES_ACCESS_DENIED  // 2
};

app_res_e WPC_get_app_config_data(uint8_t * seq_p, uint16_t * interval_p, uint8_t * config, uint8_t size)
{
    app_res_e ret;
    uint16_t interval_le;
    uint8_t max_size;
    int res;

    // First get max app config size
    app_res_e max_size_res = WPC_get_app_config_data_size(&max_size);
    if (max_size_res != APP_RES_OK)
    {
        return max_size_res;
    }

    // Check that provided buffer is big enough to store the full app_config
    if (size < max_size)
    {
        return APP_RES_INVALID_VALUE;
    }

    res = msap_app_config_data_read_request(seq_p, &interval_le, config, size);
    ret = convert_error_code(APP_CONFIG_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *interval_p = uint16_decode_le((const uint8_t *) &interval_le);
    return APP_RES_OK;
}

/* Error code LUT for app_config write */
static const app_res_e APP_CONFIG_WRITE_ERROR_CODE_LUT[] = {
    APP_RES_OK,                     // 0
    APP_RES_NODE_NOT_A_SINK,        // 1
    APP_RES_INVALID_DIAG_INTERVAL,  // 2
    APP_RES_INVALID_SEQ,            // 3
    APP_RES_ACCESS_DENIED           // 4
};

app_res_e WPC_set_app_config_data(uint8_t seq, uint16_t interval, uint8_t * config, uint8_t size)
{
    uint16_t interval_le;
    uint16_encode_le(interval, (uint8_t *) &interval_le);
    uint8_t max_size;
    int res;

    // First get max app config size
    app_res_e max_size_res = WPC_get_app_config_data_size(&max_size);
    if (max_size_res != APP_RES_OK)
    {
        return max_size_res;
    }

    // Check with provided size
    if (size > max_size)
    {
        return APP_RES_INVALID_VALUE;
    }

    res = msap_app_config_data_write_request(seq, interval_le, config, size);

    return convert_error_code(APP_CONFIG_WRITE_ERROR_CODE_LUT, res);
}

/* Error code LUT for sink cost read/write */
static const app_res_e SINK_COST_ERROR_CODE_LUT[] = {
    APP_RES_OK,               // 0
    APP_RES_NODE_NOT_A_SINK,  // 1
    APP_RES_ACCESS_DENIED     // 2
};

app_res_e WPC_set_sink_cost(uint8_t cost)
{
    int res = msap_sink_cost_write_request(cost);

    return convert_error_code(SINK_COST_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_sink_cost(uint8_t * cost_p)
{
    int res = msap_sink_cost_read_request(cost_p);

    return convert_error_code(SINK_COST_ERROR_CODE_LUT, res);
}

static bool get_statck_status(uint16_t timeout_s)
{
    uint8_t status;
    app_res_e res = APP_RES_INTERNAL_ERROR;

    // Compute timeout
    unsigned long long timeout = Platform_get_timestamp_ms_epoch() + timeout_s * 1000;

    while (res != APP_RES_OK && Platform_get_timestamp_ms_epoch() < timeout)
    {
        res = WPC_get_stack_status(&status);
        LOGD("Cannot get status after start/stop, try again...\n");
    }

    if (res != APP_RES_OK)
    {
        LOGE("Cannot get stack status after %d seconds\n", timeout_s);
        return false;
    }

    return true;
}
app_res_e WPC_start_stack(void)
{
    int res = msap_stack_start_request(0);

    if (res < 0)
    {
        // Error in communication
        return APP_RES_INTERNAL_ERROR;
    }

    if ((res & 0x1) == 0x01)
    {
        return APP_RES_STACK_ALREADY_STARTED;
    }
    else if ((res & 0x2) == 0x2)
    {
        return APP_RES_NET_ADD_NOT_SET;
    }
    else if ((res & 0x4) == 0x4)
    {
        return APP_RES_NODE_ADD_NOT_SET;
    }
    else if ((res & 0x8) == 0x8)
    {
        return APP_RES_ROLE_NOT_SET;
    }
    else if (res != 0)
    {
        return APP_RES_INTERNAL_ERROR;
    }

    // A start of the stack shouldn't create any interruption
    // of service but let's poll for it to be symmetric with stop
    // and for some reason it was seen on some platforms that the
    // first request following a start is lost
    if (!get_statck_status(2))
    {
        return APP_RES_INTERNAL_ERROR;
    }

    return APP_RES_OK;
}

app_res_e WPC_stop_stack(void)
{
    int res;
    app_res_e f_res = APP_RES_OK;
    // Stop the poll request to avoid timeout error during reboot
    Platform_disable_poll_request(true);

    res = msap_stack_stop_request();

    if (res < 0)
    {
        f_res = APP_RES_INTERNAL_ERROR;
    }
    else if (res == 1)
    {
        f_res = APP_RES_STACK_ALREADY_STOPPED;
    }
    else if (res == 128)
    {
        f_res = APP_RES_ACCESS_DENIED;
    }
    else
    {
        // Wait 500ms to avoid a systematic timeout
        // at each reboot. If status is asked immediately,
        // stack cannot answer
        Platform_usleep(500 * 1000);

        // A stop of the stack will reboot the device
        // Wait for the stack to be up again
        // It can be quite long in case a scratchpad is processed
        if (!get_statck_status(TIMEOUT_AFTER_STOP_STACK_S))
        {
            f_res = APP_RES_INTERNAL_ERROR;
        }
    }

    Platform_disable_poll_request(false);
    return f_res;
}

static app_res_e read_single_byte_msap(uint8_t att, uint8_t * pointer)
{
    int res;
    // app_res_e ret;
    res = msap_attribute_read_request(att, 1, pointer);

    return convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_stack_status(uint8_t * status_p)
{
    return read_single_byte_msap(MSAP_STACK_STATUS, status_p);
}

app_res_e WPC_get_PDU_buffer_usage(uint8_t * usage_p)
{
    return read_single_byte_msap(MSAP_PDU_BUFFER_USAGE, usage_p);
}

app_res_e WPC_get_PDU_buffer_capacity(uint8_t * capacity_p)
{
    return read_single_byte_msap(MSAP_PDU_BUFFER_CAPACITY, capacity_p);
}

app_res_e WPC_get_remaining_energy(uint8_t * energy_p)
{
    return read_single_byte_msap(MSAP_ENERGY, energy_p);
}

app_res_e WPC_set_remaining_energy(uint8_t energy)
{
    int res = msap_attribute_write_request(MSAP_ENERGY, 1, &energy);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_autostart(uint8_t * enable_p)
{
    return read_single_byte_msap(MSAP_AUTOSTART, enable_p);
}

app_res_e WPC_set_autostart(uint8_t enable)
{
    int res = msap_attribute_write_request(MSAP_AUTOSTART, 1, &enable);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_route_count(uint8_t * count_p)
{
    return read_single_byte_msap(MSAP_ROUTE_COUNT, count_p);
}

app_res_e WPC_get_system_time(uint32_t * time_p)
{
    app_res_e ret;
    uint8_t att[4];
    uint32_t internal_time;
    int res = msap_attribute_read_request(MSAP_SYSTEM_TIME, 4, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    internal_time = uint32_decode_le(att);
    *time_p = internal_time_to_s(internal_time);
    return APP_RES_OK;
}

app_res_e WPC_get_access_cycle_range(uint16_t * min_ac_p, uint16_t * max_ac_p)
{
    app_res_e ret;
    uint8_t att[4];
    int res = msap_attribute_read_request(MSAP_ACCESS_CYCLE_RANGE, 4, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *min_ac_p = uint16_decode_le(att);
    *max_ac_p = uint16_decode_le(att + 2);
    return APP_RES_OK;
}

app_res_e WPC_set_access_cycle_range(uint16_t min_ac, uint16_t max_ac)
{
    uint8_t att[4];
    int res;
    uint16_encode_le(min_ac, att);
    uint16_encode_le(max_ac, att + 2);
    res = msap_attribute_write_request(MSAP_ACCESS_CYCLE_RANGE, 4, att);

    return convert_error_code(ATT_WRITE_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_access_cycle_limits(uint16_t * min_ac_l_p, uint16_t * max_ac_l_p)
{
    app_res_e ret;
    uint8_t att[4];
    int res = msap_attribute_read_request(MSAP_ACCESS_CYCLE_LIMITS, 4, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *min_ac_l_p = uint16_decode_le(att);
    *max_ac_l_p = uint16_decode_le(att + 2);
    return APP_RES_OK;
}

app_res_e WPC_get_current_access_cycle(uint16_t * cur_ac_p)
{
    app_res_e ret;
    uint8_t att[2];
    int res = msap_attribute_read_request(MSAP_CURRENT_ACCESS_CYCLE, 2, att);

    ret = convert_error_code(ATT_READ_ERROR_CODE_LUT, res);
    if (ret != APP_RES_OK)
    {
        return ret;
    }

    *cur_ac_p = uint16_decode_le(att);
    return APP_RES_OK;
}

app_res_e WPC_get_scratchpad_block_max(uint8_t * max_size_p)
{
    return read_single_byte_msap(MSAP_SCRATCHPAD_BLOCK_MAX, max_size_p);
}

app_res_e WPC_get_local_scratchpad_status(app_scratchpad_status_t * status_p)
{
    app_res_e res;
    msap_scratchpad_status_conf_pl_t internal_status;

    // There is no return code from this request
    res = msap_scratchpad_status_request(&internal_status);
    if (res != 0)
    {
        return APP_RES_INTERNAL_ERROR;
    }

    // Copy internal payload to app return code. Cannot use memcpy as structures
    // may have different alignment
    convert_internal_to_app_scratchpad_status(status_p, &internal_status);

    return APP_RES_OK;
}

app_res_e WPC_upload_local_scratchpad(uint32_t len, uint8_t * bytes, uint8_t seq)
{
    app_res_e res;

    res = WPC_start_local_scratchpad_update(len, seq);
    if (res != APP_RES_OK)
    {
        LOGE("Cannot start scratchpad update\n");
        return res;
    }

    return WPC_upload_local_block_scratchpad(len, bytes, 0);
}

/* Error code LUT for local start scratchpad */
static const app_res_e SCRATCHPAD_LOCAL_START_ERROR_CODE_LUT[] = {
    APP_RES_OK,                 // 0
    APP_RES_STACK_NOT_STOPPED,  // 1
    APP_RES_INVALID_VALUE,      // 2
    APP_RES_INTERNAL_ERROR,     // 3
    APP_RES_ACCESS_DENIED       // 4
};

app_res_e WPC_start_local_scratchpad_update(uint32_t len, uint8_t seq)
{
    int res;
    uint32_t len_le;
    uint32_encode_le(len, (uint8_t *) &len_le);
    res = msap_scratchpad_start_request(len_le, seq);

    return convert_error_code(SCRATCHPAD_LOCAL_START_ERROR_CODE_LUT, res);
}

/* Error code LUT for local block scratchpad */
static const app_res_e SCRATCHPAD_LOCAL_BLOCK_ERROR_CODE_LUT[] = {
    APP_RES_OK,          // 0
    APP_RES_OK,          // 1 (keep same error code to avoid a success != 0)
    APP_RES_DATA_ERROR,  // 2
    APP_RES_STACK_NOT_STOPPED,        // 3
    APP_RES_NO_SCRATCHPAD_START,      // 4
    APP_RES_INVALID_START_ADDRESS,    // 5
    APP_RES_INVALID_NUMBER_OF_BYTES,  // 6
    APP_RES_INVALID_SCRATCHPAD        // 7
};

app_res_e WPC_upload_local_block_scratchpad(uint32_t len, uint8_t * bytes, uint32_t start)
{
    app_res_e app_res;
    uint8_t res;
    uint32_t loaded = 0;
    uint8_t max_block_size, block_size;

    app_res = WPC_get_scratchpad_block_max(&max_block_size);
    if (app_res != APP_RES_OK)
    {
        LOGE("Cannot get max block scratchpad size\n");
        return app_res;
    }

    while (loaded < len)
    {
        uint32_t remaining = len - loaded;
        block_size = (remaining > max_block_size) ? max_block_size : remaining;
        uint32_t addr_le;
        uint32_encode_le(start + loaded, (uint8_t *) &addr_le);

        res = msap_scratchpad_block_request(addr_le, block_size, bytes + loaded);
        if (res > 1)
        {
            LOGE("Error in loading scratchpad block -> %d\n", res);
            return convert_error_code(SCRATCHPAD_LOCAL_BLOCK_ERROR_CODE_LUT, res);
        }

        if (res == 1)
        {
            LOGD("Last block loaded\n");
        }

        loaded += block_size;
    }
    return APP_RES_OK;
}

/* Error code LUT for sink cost read/write */
static const app_res_e SCRATCHPAD_CLEAR_LOCAL_ERROR_CODE_LUT[] = {
    APP_RES_OK,                 // 0
    APP_RES_STACK_NOT_STOPPED,  // 1
    APP_RES_ACCESS_DENIED       // 2
};

app_res_e WPC_clear_local_scratchpad()
{
    int res = msap_scratchpad_clear_request();

    return convert_error_code(SCRATCHPAD_CLEAR_LOCAL_ERROR_CODE_LUT, res);
}

/* Error code LUT for sink cost read/write */
static const app_res_e SCRATCHPAD_UPDATE_LOCAL_ERROR_CODE_LUT[] = {
    APP_RES_OK,                   // 0
    APP_RES_STACK_NOT_STOPPED,    // 1
    APP_RES_NO_VALID_SCRATCHPAD,  // 2
    APP_RES_ACCESS_DENIED         // 3
};

app_res_e WPC_update_local_scratchpad()
{
    int res = msap_scratchpad_update_request();

    return convert_error_code(SCRATCHPAD_UPDATE_LOCAL_ERROR_CODE_LUT, res);
}

/* Error code LUT for scratchpad remote status */
static const app_res_e SCRATCHPAD_STATUS_REMOTE_ERROR_CODE_LUT[] = {APP_RES_OK,  // 0
                                                                    APP_RES_STACK_IS_STOPPED,  // 1
                                                                    APP_RES_NOT_A_SINK,  // 2
                                                                    APP_RES_OUT_OF_MEMORY,  // 3
                                                                    APP_RES_ACCESS_DENIED};

app_res_e WPC_get_remote_status(app_addr_t destination_address)
{
    uint32_t destination_address_le;
    uint32_encode_le(destination_address, (uint8_t *) &destination_address_le);
    int res = msap_scratchpad_remote_status(destination_address_le);

    return convert_error_code(SCRATCHPAD_STATUS_REMOTE_ERROR_CODE_LUT, res);
}

/* Error code LUT for scratchpad remote update */
static const app_res_e SCRATCHPAD_UPDATE_REMOTE_ERROR_CODE_LUT[] = {
    APP_RES_OK,                // 0
    APP_RES_STACK_IS_STOPPED,  // 1
    APP_RES_NOT_A_SINK,        // 2
    APP_RES_OUT_OF_MEMORY,     // 3
    APP_RES_INVALID_SEQ,       // 4
    APP_RES_INVALID_VALUE,     // 5
    APP_RES_ACCESS_DENIED      // 6
};

app_res_e WPC_remote_scratchpad_update(app_addr_t destination_address, uint8_t seq, uint16_t reboot_delay_s)
{
    uint32_t destination_address_le;
    uint32_encode_le(destination_address, (uint8_t *) &destination_address_le);
    uint16_t reboot_delay_s_le;
    uint16_encode_le(reboot_delay_s, (uint8_t *) &reboot_delay_s_le);
    int res = msap_scratchpad_remote_update(destination_address_le, seq, reboot_delay_s_le);

    return convert_error_code(SCRATCHPAD_UPDATE_REMOTE_ERROR_CODE_LUT, res);
}

/* Error code LUT for target scratchpad write */
static const app_res_e TARGET_SCRATCHPAD_ERROR_CODE_LUT[] = {
    APP_RES_OK,               // 0
    APP_RES_NODE_NOT_A_SINK,  // 1
    APP_RES_INVALID_VALUE,    // 2
    APP_RES_ACCESS_DENIED     // 3
};

app_res_e WPC_write_target_scratchpad(uint8_t target_sequence,
                                      uint16_t target_crc,
                                      uint8_t action,
                                      uint8_t param)
{
    int res = msap_scratchpad_target_write_request(
                                target_sequence,
                                target_crc,
                                action,
                                param);

    return convert_error_code(TARGET_SCRATCHPAD_ERROR_CODE_LUT, res);
}

app_res_e WPC_read_target_scratchpad(uint8_t * target_sequence_p,
                                     uint16_t * target_crc_p,
                                     uint8_t * action_p,
                                     uint8_t * param_p)
{
    int res = msap_scratchpad_target_read_request(
                                target_sequence_p,
                                target_crc_p,
                                action_p,
                                param_p);

    // We should always be able to read it if implemented
    return res == 0 ? APP_RES_OK : APP_RES_INTERNAL_ERROR;
}

/* Error code LUT for scan neighbors */
static const app_res_e SCAN_NEIGHBORS_ERROR_CODE_LUT[] = {
    APP_RES_OK,                // 0
    APP_RES_STACK_IS_STOPPED,  // 1
    APP_RES_ACCESS_DENIED      // 2
};

app_res_e WPC_start_scan_neighbors()
{
    int res = msap_scan_nbors_request();

    return convert_error_code(SCAN_NEIGHBORS_ERROR_CODE_LUT, res);
}

app_res_e WPC_get_neighbors(app_nbors_t * nbors_list_p)
{
    msap_get_nbors_conf_pl_t internal_list;
    int res = msap_get_nbors_request(&internal_list);

    if (res != 0)
    {
        return APP_RES_INTERNAL_ERROR;
    }

    convert_internal_to_app_neighbors_status(nbors_list_p, &internal_list);
    return APP_RES_OK;
}

/* Error code LUT for sending data */
static const app_res_e SEND_DATA_ERROR_CODE_LUT[] = {
    APP_RES_OK,                // 0
    APP_RES_STACK_IS_STOPPED,  // 1
    APP_RES_INVALID_VALUE,     // 2
    APP_RES_INVALID_VALUE,     // 3
    APP_RES_OUT_OF_MEMORY,     // 4
    APP_RES_UNKNOWN_DEST,      // 5
    APP_RES_INVALID_VALUE,     // 6
    APP_RES_INTERNAL_ERROR,    // 7
    APP_RES_INTERNAL_ERROR,    // 8
    APP_RES_INVALID_VALUE,     // 9
    APP_RES_ACCESS_DENIED      // 10
};
app_res_e WPC_send_data_with_options(app_message_t * message_t)
{
    int res;
    uint32_t dst_addr_le;
    uint16_t pdu_id_le;
    uint32_t buffering_delay_le;
    uint32_encode_le(message_t->dst_addr, (uint8_t *) &dst_addr_le);
    uint16_encode_le(message_t->pdu_id, (uint8_t *) &pdu_id_le);
    uint32_encode_le(ms_to_internal_time(message_t->buffering_delay),
                     (uint8_t *) &buffering_delay_le);

    res = dsap_data_tx_request(message_t->bytes,
                               message_t->num_bytes,
                               pdu_id_le,
                               dst_addr_le,
                               (message_t->qos & 0xff) == APP_QOS_HIGH ? 1 : 0,
                               message_t->src_ep,
                               message_t->dst_ep,
                               message_t->on_data_sent_cb,
                               buffering_delay_le,
                               message_t->is_unack_csma_ca,
                               message_t->hop_limit);

    return convert_error_code(SEND_DATA_ERROR_CODE_LUT, res);
}

app_res_e WPC_send_data(const uint8_t * bytes,
                        uint8_t num_bytes,
                        uint16_t pdu_id,
                        app_addr_t dst_addr,
                        app_qos_e qos,
                        uint8_t src_ep,
                        uint8_t dst_ep,
                        onDataSent_cb_f on_data_sent_cb,
                        uint32_t buffering_delay)
{
    app_message_t message;

    // Fill the message structure
    message.bytes = bytes;
    message.num_bytes = num_bytes;
    message.pdu_id = pdu_id;
    message.dst_addr = dst_addr;
    message.qos = qos;
    message.src_ep = src_ep;
    message.dst_ep = dst_ep;
    message.on_data_sent_cb = on_data_sent_cb;
    message.buffering_delay = buffering_delay;
    message.hop_limit = 0;
    message.is_unack_csma_ca = false;

    return WPC_send_data_with_options(&message);
}

app_res_e WPC_register_for_data(uint8_t dst_ep, onDataReceived_cb_f onDataReceived)
{
    return dsap_register_for_data(dst_ep, onDataReceived) ? APP_RES_OK : APP_RES_INVALID_VALUE;
}

app_res_e WPC_unregister_for_data(uint8_t dst_ep)
{
    return dsap_unregister_for_data(dst_ep) ? APP_RES_OK : APP_RES_INVALID_VALUE;
}

app_res_e WPC_register_for_remote_status(onRemoteStatus_cb_f onRemoteStatusReceived)
{
    return msap_register_for_remote_status(onRemoteStatusReceived) ? APP_RES_OK : APP_RES_INVALID_VALUE;
}

app_res_e WPC_unregister_for_remote_status()
{
    return msap_unregister_from_remote_status() ? APP_RES_OK : APP_RES_INVALID_VALUE;
}

app_res_e WPC_register_for_app_config_data(onAppConfigDataReceived_cb_f onAppConfigDataReceived)
{
    return msap_register_for_app_config(onAppConfigDataReceived) ? APP_RES_OK : APP_RES_ALREADY_REGISTERED;
}

app_res_e WPC_unregister_from_app_config_data()
{
    return msap_unregister_from_app_config() ? APP_RES_OK : APP_RES_NOT_REGISTERED;
}

app_res_e WPC_register_for_scan_neighbors_done(onScanNeighborsDone_cb_f onScanNeighborDone)
{
    return msap_register_for_scan_neighbors_done(onScanNeighborDone) ? APP_RES_OK : APP_RES_INVALID_VALUE;
}

app_res_e WPC_unregister_from_scan_neighbors_done()
{
    return msap_unregister_from_scan_neighbors_done() ? APP_RES_OK : APP_RES_INVALID_VALUE;
}

app_res_e WPC_register_for_stack_status(onStackStatusReceived_cb_f onStackStatusReceived)
{
    return msap_register_for_stack_status(onStackStatusReceived) ? APP_RES_OK : APP_RES_INVALID_VALUE;
}

app_res_e WPC_unregister_from_stack_status()
{
    return msap_unregister_from_stack_status() ? APP_RES_OK : APP_RES_INVALID_VALUE;
}
