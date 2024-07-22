/* Copyright 2019 Wirepas Ltd licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

#include "config_message.pb.h"

/** Structure to hold unmodifiable configs from node */
typedef struct sink_config
{
    /* Read only parameters backup-ed with a table (Read at boot up) */
    uint16_t stack_profile;
    uint16_t hw_magic;
    uint16_t ac_range_min;
    uint16_t ac_range_max;
    uint16_t app_config_max_size;
    uint16_t version[4];
    uint8_t max_mtu;
    uint8_t ch_range_min;
    uint8_t ch_range_max;
    uint8_t pdu_buffer_size;

    /* Read parameters with node interrogation */
    uint16_t CurrentAC; //SD_BUS_PROPERTY("CurrentAC", "q", current_ac_read_handler, 0, 0),
    //SD_BUS_PROPERTY("CipherKeySet", "b", cipher_key_read_handler, 0, 0),
    //SD_BUS_PROPERTY("AuthenticationKeySet", "b", authen_key_read_handler, 0, 0),
    //SD_BUS_PROPERTY("StackStatus", "y", stack_status_read_handler, 0, 0),

    /* Read write also ? */
    uint16_t ac_range_min_cur; // 0 means unset ?
    uint16_t ac_range_max_cur;

    /* Read/Write parameters with node interrogation */
    uint32_t node_address;
    wp_NodeRole node_role;
    uint64_t network_address;
    uint32_t network_channel;
    uint32_t channel_map;
    /* Methods related to config */
    wp_AppConfigData app_config;
} sink_config_t;

/* TODO : check how to get access to config cache. Protected access ? */
extern sink_config_t m_sink_config;

/**
 * \brief   Handle config cache when status changed
 */
void Config_On_stack_boot_status(uint8_t status);

/**
 * \brief   Initialize the config module
 * \return  0 if initialization succeed, an error code otherwise
 * \note    Connection with sink must be ready before calling this module
 */
int Config_Init();

/**
 * \brief   Close the config module
 */
void Config_Close();

#endif /* SOURCE_CONFIG_H_ */
