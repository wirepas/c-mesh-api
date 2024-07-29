/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wpc_proto.h"

#include "MQTTClient.h"

#define PRINT_BUFFERS
#define LOG_MODULE_NAME "Main"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"


#define DEFAULT_BAUDTRATE 1000000

#define SINK_ID "sink0"

#define TOPIC_RX_DATA_PREFIX "gw-event/received_data"
#define TOPIC_EVENT_PREFIX "gw-event/status"

static char m_gateway_id[64] = "\0";

static uint8_t m_proto_buffer[WPC_PROTO_MAX_RESPONSE_SIZE];

static MQTTClient m_client = NULL;

static bool MQTT_publish(char * topic, uint8_t * payload, size_t payload_size, bool retained)
{
    int rc;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payloadlen = payload_size;
    pubmsg.payload = payload;
    pubmsg.qos = 1;
    pubmsg.retained = retained;

    LOGI("Publishing on topic %s\n", topic);
    MQTTClient_publishMessage(m_client, topic, &pubmsg, &token);
    rc = MQTTClient_waitForCompletion(m_client, token, 200);
    LOGD("Message with delivery token %d delivered rc=%d\n", token, rc);

    return rc == MQTTCLIENT_SUCCESS;
}

static bool MQTT_connect(uint32_t timeout_s,
                         const char * host,
                         const char * user,
                         const char * password)
{
    int rc;
    size_t proto_size;
    char topic_status[sizeof(TOPIC_EVENT_PREFIX) + sizeof(m_gateway_id) + 1];
    char topic_all_requests[16 + sizeof(m_gateway_id)]; //"gw-request/+/<gateway_id/#"

    // Prepare our topics
    snprintf(topic_status,
             sizeof(topic_status),
             "%s/%s",
             TOPIC_EVENT_PREFIX,
             m_gateway_id);

    snprintf(topic_all_requests,
             sizeof(topic_all_requests),
             "gw-request/+/%s/#",
             m_gateway_id);

    MQTTClient_init_options global_init_options = MQTTClient_init_options_initializer;
    global_init_options.do_openssl_init = true;

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions will_options = MQTTClient_willOptions_initializer;
    MQTTClient_SSLOptions ssl_options = MQTTClient_SSLOptions_initializer;

    MQTTClient_global_init(&global_init_options);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.ssl = &ssl_options;

    conn_opts.username = user;
    conn_opts.password = password;

    // Setup last will
    proto_size = sizeof(m_proto_buffer);
    if (WPC_Proto_get_current_event_status(false, m_proto_buffer, &proto_size) == APP_RES_PROTO_OK)
    {
        will_options.topicName = topic_status;
        will_options.qos = 1;
        will_options.retained = 1;
        will_options.payload.len = proto_size;
        will_options.payload.data = m_proto_buffer;
        conn_opts.will = &will_options;
    }

    if ((rc = MQTTClient_create(&m_client, host, "embeded-gw",
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
         LOGE("Failed to create client, return code %d\n", rc);
         exit(EXIT_FAILURE);
    }

    if ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        LOGE("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    while (timeout_s > 0)
    {
        if (MQTTClient_isConnected(m_client))
        {
            break;
        }
        sleep(1);
        timeout_s--;
    }

    if (!timeout_s)
    {
        LOGE("Cannot connect within timeout");
        exit(EXIT_FAILURE);
    }
    LOGI("Mqtt client connected\n");

    // Set our current status
    proto_size = sizeof(m_proto_buffer);
    if (WPC_Proto_get_current_event_status(true, m_proto_buffer, &proto_size) == APP_RES_PROTO_OK)
    {
        MQTT_publish(topic_status, m_proto_buffer, proto_size, true);
    }

    // Register for request topic
    if ((rc = MQTTClient_subscribe(m_client, topic_all_requests, 1)) != MQTTCLIENT_SUCCESS)
    {
        LOGE("Failed to subscribe, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    return true;
}

static void MQTT_infinite_loop()
{
    app_proto_res_e res;
    char * topic = NULL;
    char * response_topic_p = NULL;
    MQTTClient_message *message = NULL;
    int topicLen;
    size_t response_size;

    for(;;)
    {
        MQTTClient_receive(m_client, &topic, &topicLen, &message, 100);
        if (message!= NULL)
        {
            LOGD("Message received on topic %s\n", topic);
            response_size = sizeof(m_proto_buffer);

            res = WPC_Proto_handle_request(message->payload, message->payloadlen, m_proto_buffer, &response_size);
            if (res == APP_RES_PROTO_OK)
            {
                // response_topic is same as request with substitution of request with response
                // Allocate space for response topic (+1 as "gw-response" is longer than "gw-request" by 1)
                size_t response_topic_size = strlen(topic) + 2;
                response_topic_p = (char *) malloc(response_topic_size);
                if (!response_topic_p)
                {
                    LOGE("Cannot allocate space for response topic\n");
                }
                else
                {
                    // Create response topic
                    snprintf(response_topic_p,
                            response_topic_size,
                            "gw-response/%s",
                            topic + 11); // Everything after gw-request/
                    LOGI("Response generated of size = %d for topic: %s\n", response_size, response_topic_p);
                    MQTT_publish(response_topic_p, m_proto_buffer, response_size, false);
                    free(response_topic_p);
                }
            }
            else
            {
                LOGE("Cannot handle request: %d\n", res);
            }

            MQTTClient_free(topic);
            MQTTClient_freeMessage(&message);
            message = NULL;
            topic = NULL;
        }
    }
}

static void onDataRxEvent_cb(uint8_t * event_p,
                            size_t event_size,
                            uint32_t network_id,
                            uint16_t src_ep,
                            uint16_t dst_ep)
{

    // Topic max size is header + network address + src/dsp ep in ascii + 3
    // so sizeof(TOPIC_RX_DATA) + 10 + 3 + 3 + 3 + 1
    char topic[sizeof(TOPIC_RX_DATA_PREFIX) + sizeof(SINK_ID) + sizeof(m_gateway_id) + 20];

    snprintf(topic,
             sizeof(topic),
             "%s/%s/%s/%u/%u/%u",
             TOPIC_RX_DATA_PREFIX,
             m_gateway_id,
             SINK_ID,
             network_id,
             src_ep,
             dst_ep);

    MQTT_publish(topic, event_p, event_size, false);
}

static void onEventStatus_cb(uint8_t * event_p, size_t event_size)
{
    char topic_status[sizeof(TOPIC_EVENT_PREFIX) + sizeof(m_gateway_id) + 1];

    // Prepare our topics
    snprintf(topic_status,
             sizeof(topic_status),
             "%s/%s",
             TOPIC_EVENT_PREFIX,
             m_gateway_id);

    MQTT_publish(topic_status, event_p, event_size, true);
    LOGI("EventStatus to publish size = %d\n", event_size);
}


void print_usage() {
    printf("Usage: gw-example -p <port name> [-b <baudrate>] -g <gateway_id> -H <mqtt hostname> [-U <mqtt user>] [-P <mqtt password>]\n");
}

int main(int argc, char * argv[])
{
    unsigned long baudrate = DEFAULT_BAUDTRATE;
    int c;
    char * port_name = NULL;
    char * mqtt_host = NULL;
    char * mqtt_user = NULL;
    char * mqtt_password = NULL;
    char * gateway_id;

    // Parse the arguments
    while ((c = getopt(argc, argv, "b:p:g:U:P:H:h")) != -1)
    {
        switch (c)
        {
            case 'b':
                baudrate = strtoul(optarg, NULL, 0);
                break;
            case 'p':
                port_name = optarg;
                break;
            case 'g':
                gateway_id = optarg;
                break;
            case 'H':
                mqtt_host = optarg;
                break;
            case 'U':
                mqtt_user = optarg;
                break;
            case 'P':
                mqtt_password = optarg;
                break;
            case 'h':
            case '?':
                print_usage();
            default:
                return -1;
            }
    }

    if (!port_name || !gateway_id ||!mqtt_host)
    {
        LOGE("All required parameter are not set\n");
        print_usage();
        return -1;
    }

    if (strlen(gateway_id) > sizeof(m_gateway_id))
    {
        LOGE("Gateway id is too long (> than %d)\n", sizeof(m_gateway_id));
        return -1;
    }

    strcpy(m_gateway_id, gateway_id);

    if (WPC_Proto_initialize(port_name,
                             baudrate,
                             m_gateway_id,
                             "test_gw",
                             "v0.1",
                             "sink0") != APP_RES_PROTO_OK)
    {
        return -1;
    }

    MQTT_connect(3, mqtt_host, mqtt_user, mqtt_password);

    WPC_Proto_register_for_data_rx_event(onDataRxEvent_cb);
    WPC_Proto_register_for_event_status(onEventStatus_cb);

    LOGI("Starting gw with id %s on host %s\n", gateway_id, mqtt_host);

    MQTT_infinite_loop();

    LOGE("End of program\n");
}
