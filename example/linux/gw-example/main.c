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
#include <pthread.h>

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

// Configuration. It has to be static as it is reused
// to reconnect
static char m_gateway_id[64] = "\0";
static char m_user[64] = "\0";
static char m_password[128] = "\0";

static uint8_t m_proto_buffer[WPC_PROTO_MAX_RESPONSE_SIZE];

static pthread_t m_thread_publish;
// Mutex for publishing on MQTT
static pthread_mutex_t m_queue_mutex;
static pthread_cond_t m_queue_not_empty_cond = PTHREAD_COND_INITIALIZER;

// Statically allocated but could be mallocated
typedef struct
{
     char topic[64];        //< Topic to publish
     uint8_t payload[1024];       //< The payload
     size_t payload_size;
     bool retained;
} message_to_publish_t;

#define PUBLISH_QUEUE_SIZE      16
// Publish queue
static message_to_publish_t m_publish_queue[PUBLISH_QUEUE_SIZE];

// Head of the queue for the polling thread to write
static unsigned int m_pub_queue_write = 0;

// // Tail of the queue to read from dispatching thread
static unsigned int m_pub_queue_read = 0;

// Is queue empty?
static bool m_queue_empty = true;


static MQTTClient m_client = NULL;

static bool MQTT_publish(char * topic, uint8_t * payload, size_t payload_size, bool retained)
{
    message_to_publish_t * message_p;
    bool ret;
    pthread_mutex_lock(&m_queue_mutex);

    if (!m_queue_empty && (m_pub_queue_write == m_pub_queue_read))
    {
        ret = false;
    }
    else
    {
        // Insert our publish
        message_p = &m_publish_queue[m_pub_queue_write];
        // TODO add check on size
        strcpy(message_p->topic, topic);
        memcpy(message_p->payload, payload, payload_size);
        message_p->payload_size = payload_size;
        message_p->retained = retained;

        m_pub_queue_write = (m_pub_queue_write + 1) % PUBLISH_QUEUE_SIZE;

        pthread_cond_signal(&m_queue_not_empty_cond);
        m_queue_empty = false;
        ret = true;
    }

    pthread_mutex_unlock(&m_queue_mutex);

    return ret;
}

static void * publish_thread(void * unused)
{
    message_to_publish_t * message;
    pthread_mutex_lock(&m_queue_mutex);
    while (true)
    {
        if (m_queue_empty)
        {
            // Queue is empty, wait
            pthread_cond_wait(&m_queue_not_empty_cond, &m_queue_mutex);

            // Check if we wake up but nothing in queue
            if (m_queue_empty)
            {
                // Release the lock
                pthread_mutex_unlock(&m_queue_mutex);
                // Continue to evaluate the stop condition
                continue;
            }
        }

        // Publish our oldest message
        message = &m_publish_queue[m_pub_queue_read];

        pthread_mutex_unlock(&m_queue_mutex);

        // Queue our message
        int rc;
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;

        pubmsg.payloadlen = message->payload_size;
        pubmsg.payload = message->payload;
        pubmsg.qos = 1;
        pubmsg.retained = message->retained;


        LOGD("Publishing on topic %s\n", message->topic);
        rc = MQTTClient_publishMessage(m_client, message->topic, &pubmsg, &token);
        LOGI("Message with token %d published rc=%d\n", token, rc);

        m_pub_queue_read = (m_pub_queue_read + 1) % PUBLISH_QUEUE_SIZE;
        if (m_pub_queue_read == m_pub_queue_write)
        {
            // Indication was the last one
            m_queue_empty = true;
        }
    }
    LOGW("Exiting publish thread\n");
    return NULL;
}

static void on_mqtt_message_delivered(void *context, MQTTClient_deliveryToken dt)
{
    // For now, only informal but message should be cleared from queue only when
    // delivered and not published as of today
    LOGI("Message with token %d delivery confirmed\n", dt);
}

static int on_message_rx_mqtt(void *context, char *topic, int topic_len, MQTTClient_message *message)
{
    size_t response_size;
    char * response_topic_p;
    app_proto_res_e res;

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
    return 1;
}


static bool reconnect(uint32_t timeout_s)
{
    int rc;
    size_t proto_size;
    char topic_status[sizeof(TOPIC_EVENT_PREFIX) + sizeof(m_gateway_id) + 1];
    char topic_all_requests[16 + sizeof(m_gateway_id)]; //"gw-request/+/<gateway_id/#"
    uint8_t last_will_message[128];
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_willOptions will_options = MQTTClient_willOptions_initializer;
    MQTTClient_SSLOptions ssl_options = MQTTClient_SSLOptions_initializer;

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.ssl = &ssl_options;

    conn_opts.username = m_user;
    conn_opts.password = m_password;

    // Setup last will
    proto_size = sizeof(last_will_message);
    if (WPC_Proto_get_current_event_status(false, last_will_message, &proto_size) == APP_RES_PROTO_OK)
    {
        will_options.topicName = topic_status;
        will_options.qos = 1;
        will_options.retained = 1;
        will_options.payload.len = proto_size;
        will_options.payload.data = last_will_message;
        conn_opts.will = &will_options;
    }

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

    while (timeout_s > 0)
    {
        if ((rc = MQTTClient_connect(m_client, &conn_opts)) == MQTTCLIENT_SUCCESS)
        {
            break;
        }
        LOGE("Failed to connect, return code %d trying again %d s remaining\n", rc, timeout_s);
        timeout_s--;
        sleep(1);
    }

    if (!MQTTClient_isConnected(m_client))
    {
        LOGE("Failed to connect, return code %d\n", rc);
        return false;
    }

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
        return false;
    }

    return true;
}

static void on_mqtt_connection_lost(void *context, char *cause)
{
    LOGE("Connection lost\n");
    LOGE("cause: %s\n", cause);
    reconnect(60);
}

static bool MQTT_connect(uint32_t timeout_s,
                         const char * host)
{
    int rc;

    MQTTClient_init_options global_init_options = MQTTClient_init_options_initializer;
    global_init_options.do_openssl_init = true;

    MQTTClient_global_init(&global_init_options);

    if ((rc = MQTTClient_create(&m_client, host, m_gateway_id,
        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
         LOGE("Failed to create client, return code %d\n", rc);
         exit(EXIT_FAILURE);
    }

    MQTTClient_setCallbacks(m_client, NULL, on_mqtt_connection_lost, on_message_rx_mqtt, on_mqtt_message_delivered);

    if (!reconnect(timeout_s))
    {
        LOGE("Cannot connect within timeout");
        exit(EXIT_FAILURE);
    }

    LOGI("Mqtt client connected\n");

    return true;
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
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

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

    if (strlen(mqtt_user) > sizeof(m_user))
    {
        LOGE("User is too long (> than %d)\n", sizeof(m_user));
        return -1;
    }

    strcpy(m_user, mqtt_user);

    if (strlen(mqtt_password) > sizeof(m_password))
    {
        LOGE("Password is too long (> than %d)\n", sizeof(m_password));
        return -1;
    }

    strcpy(m_password, mqtt_password);

    if (WPC_Proto_initialize(port_name,
                             baudrate,
                             m_gateway_id,
                             "test_gw",
                             "v0.1",
                             "sink0") != APP_RES_PROTO_OK)
    {
        return -1;
    }


    // Initialize mutex to protect publish
    if (pthread_mutex_init(&m_queue_mutex, &attr) != 0)
    {
        LOGE("Pub Mutex init failed\n");
        return -1;
    }

    // Start a thread to execute publish from same thread
    if (pthread_create(&m_thread_publish, NULL, publish_thread, NULL) != 0)
    {
        LOGE("Cannot create dispatch thread\n");
        return -1;
    }

    MQTT_connect(3, mqtt_host);

    WPC_Proto_register_for_data_rx_event(onDataRxEvent_cb);
    WPC_Proto_register_for_event_status(onEventStatus_cb);

    LOGI("Starting gw with id %s on host %s\n", gateway_id, mqtt_host);

    for (;;)
    {
        sleep(2);
    }

    LOGE("End of program\n");
}
