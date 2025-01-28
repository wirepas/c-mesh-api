/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define LOG_MODULE_NAME "linux_plat"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"
#include "platform.h"
#include "reassembly.h"
#include "wpc_proto.h"

// Maximum number of indication to be retrieved from a single poll
#define MAX_NUMBER_INDICATION 30

// Polling interval to check for indication
#define POLLING_INTERVAL_MS 20

// Wakeup timeout for dispatch thread, mainly for garbage collection of fragments
#define DISPATCH_WAKEUP_TIMEOUT_S 5

// Mutex for sending, ie serial access
static pthread_mutex_t sending_mutex;

// This thread is used to poll for indication
static pthread_t thread_polling;

typedef enum {
    POLLING_THREAD_RUN,
    POLLING_THREAD_STOP,
    POLLING_THREAD_STOP_REQUESTED
} polling_thread_state_t;

// Request to handle polling thread state
static polling_thread_state_t m_polling_thread_state_request = POLLING_THREAD_STOP;

// This thread is used to dispatch indication
static pthread_t thread_dispatch;

// Set to false to stop dispatch thread execution
static bool m_dispatch_thread_running;

// Service to call to get indication from a node
static Platform_get_indication_f m_get_indication_f;

// Service to call to dispatch/handle indication from node
static Platform_dispatch_indication_f m_dispatch_indication_f;

/*****************************************************************************/
/*                Indication queue related variables                        */
/*****************************************************************************/

// Size of the queue between getter and dispatcher of indication
// In most of the cases, the dispatcher is supposed to be faster and the
// queue will have only one element
// But if some execution handling are too long or require to send something over
// UART, the dispatching thread will be hanged until the poll thread finish its
// work
#define MAX_NUMBER_INDICATION_QUEUE (MAX_NUMBER_INDICATION * 2)

// Struct that describes a received frame with its timestamp
typedef struct
{
    wpc_frame_t frame;                      //< The received frame
    unsigned long long timestamp_ms_epoch;  //< The timestamp of reception
} timestamped_frame_t;

// Indications queue
static timestamped_frame_t m_indications_queue[MAX_NUMBER_INDICATION_QUEUE];

// Head of the queue for the polling thread to write
static unsigned int m_ind_queue_write = 0;

// Tail of the queue to read from dispatching thread
static unsigned int m_ind_queue_read = 0;

// Is queue empty?
static bool m_queue_empty = true;

// Mutex and condition variable used for the dispatching queue to wait
static pthread_mutex_t m_queue_mutex;
static pthread_cond_t m_queue_not_empty_cond = PTHREAD_COND_INITIALIZER;

/*****************************************************************************/
/*                Dispatch indication Thread implementation                  */
/*****************************************************************************/
/**
 * \brief   Thread to dispatch indication in a non locked environment
 */
static void * dispatch_indication(void * unused)
{
    struct timespec ts;

    pthread_mutex_lock(&m_queue_mutex);
    while (m_dispatch_thread_running)
    {
        if (m_queue_empty)
        {
            // Queue is empty, wait
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += DISPATCH_WAKEUP_TIMEOUT_S ;  // 5 second timeout
            pthread_cond_timedwait(&m_queue_not_empty_cond, &m_queue_mutex, &ts);

            // Force a garbage collect (to be sure it's called even if no frag are received)
            reassembly_garbage_collect();

            // Check if we wake up but nothing in queue
            if (m_queue_empty)
            {
                // Release the lock
                pthread_mutex_unlock(&m_queue_mutex);
                // Continue to evaluate the stop condition
                continue;
            }
        }

        // Get the oldest indication
        timestamped_frame_t * ind = &m_indications_queue[m_ind_queue_read];

        // Handle the indication without the queue lock
        pthread_mutex_unlock(&m_queue_mutex);

        // Dispatch the indication
        m_dispatch_indication_f(&ind->frame, ind->timestamp_ms_epoch);

        // Take the lock back to update empty status and wait on cond again in
        // next loop iteration
        pthread_mutex_lock(&m_queue_mutex);

        m_ind_queue_read = (m_ind_queue_read + 1) % MAX_NUMBER_INDICATION_QUEUE;
        if (m_ind_queue_read == m_ind_queue_write)
        {
            // Indication was the last one
            m_queue_empty = true;
        }
    }

    LOGW("Exiting dispatch thread\n");
    return NULL;
}

/*****************************************************************************/
/*                Polling Thread implementation                              */
/*****************************************************************************/
static void onIndicationReceivedLocked(wpc_frame_t * frame, unsigned long long timestamp_ms)
{
    LOGD("Frame received with timestamp = %lld\n", timestamp_ms);
    pthread_mutex_lock(&m_queue_mutex);

    // Check if queue is full
    if (!m_queue_empty && (m_ind_queue_write == m_ind_queue_read))
    {
        // Queue is FULL
        LOGE("No more room for indications! Must never happen!\n");
    }
    else
    {
        // Insert our received indication
        timestamped_frame_t * ind = &m_indications_queue[m_ind_queue_write];
        memcpy(&ind->frame, frame, sizeof(wpc_frame_t));
        ind->timestamp_ms_epoch = timestamp_ms;

        m_ind_queue_write = (m_ind_queue_write + 1) % MAX_NUMBER_INDICATION_QUEUE;
        // At least one indication ready, signal it
        m_queue_empty = false;
        pthread_cond_signal(&m_queue_not_empty_cond);
    }

    pthread_mutex_unlock(&m_queue_mutex);
}

/**
 * \brief   Polling tread.
 *          This thread polls for indication and insert them to the queue
 *          shared with the dispatcher thread
 */
static void * poll_for_indication(void * unused)
{
    unsigned int max_num_indication, free_buffer_room;
    int get_ind_res;
    // Initially wait for 500ms before any polling
    uint32_t wait_before_next_polling_ms = 500;

    m_polling_thread_state_request = POLLING_THREAD_RUN;

    while (m_polling_thread_state_request != POLLING_THREAD_STOP)
    {
        usleep(wait_before_next_polling_ms * 1000);

        if(m_polling_thread_state_request == POLLING_THREAD_STOP_REQUESTED)
        {
            if (!m_queue_empty)
            {
                // Dispatch did not process all indications. Just wait for it to complete.
                wait_before_next_polling_ms = POLLING_INTERVAL_MS;
                continue;
            }

            if (reassembly_is_queue_empty())
            {
                LOGI("Reassembly queue is empty, exiting polling thread\n");
                m_polling_thread_state_request = POLLING_THREAD_STOP;
                break;
            }
        }

        // Get the number of free buffers in the indication queue
        // Note: No need to lock the queue as only m_ind_queue_read can be updated
        // and could still be modified when we release the lock after computing
        // the max free space

        /* Ask for maximum room in buffer queue and less than MAX */
        if (!m_queue_empty && (m_ind_queue_write == m_ind_queue_read))
        {
            // Queue is FULL, wait for POLLING INTERVALL to give some
            // time for the dispatching thread to handle them
            LOGW("Queue is full, do not poll\n");
            wait_before_next_polling_ms = POLLING_INTERVAL_MS;

            continue;
        }
        else if (m_queue_empty)
        {
            // Queue is empty
            free_buffer_room = MAX_NUMBER_INDICATION_QUEUE;
        }
        else
        {
            // Queue has elements, determine the number of free buffers
            if (m_ind_queue_write > m_ind_queue_read)
            {
                free_buffer_room = MAX_NUMBER_INDICATION_QUEUE -
                                   (m_ind_queue_write - m_ind_queue_read);
            }
            else
            {
                free_buffer_room = m_ind_queue_read - m_ind_queue_write;
            }
        }
                  
        if (m_polling_thread_state_request == POLLING_THREAD_STOP_REQUESTED)
        {
            // In case we are about to stop, let's poll only one by one to have more chance to
            // finish uncomplete fragmented packet and not start to receive a new one
            max_num_indication = 1;
            LOGD("Poll for one more fragment to empty reassembly queue\n");
        }
        else
        {
            // Let's read max indications that can fit in the queue
            max_num_indication = MIN(MAX_NUMBER_INDICATION, free_buffer_room);
        }

        LOGD("Poll for %d indications\n", max_num_indication);

        get_ind_res = m_get_indication_f(max_num_indication, onIndicationReceivedLocked);

        if ((get_ind_res == 1)
            && (m_polling_thread_state_request != POLLING_THREAD_STOP_REQUESTED))
        {
            // Still pending indication, only wait 1 ms to give a chance
            // to other threads but not more to have better throughput
            wait_before_next_polling_ms = 1;
        }
        else
        {
            // In case of error or if no more indication, just wait
            // the POLLING INTERVAL to avoid polling all the time
            // In case of stop request, wait for to give time to push data received
            wait_before_next_polling_ms = POLLING_INTERVAL_MS;
        }
    }

    LOGW("Exiting polling thread\n");

    return NULL;
}

bool Platform_lock_request()
{
    int res = pthread_mutex_lock(&sending_mutex);
    if (res != 0)
    {
        // It must never happen but add a check and
        // return to avoid a deadlock
        if (res == EINVAL)
        {
            LOGW("Mutex no longer exists (destroyed)\n");
        }
        else
        {
            LOGE("Mutex lock failed %d\n", res);
        }
        return false;
    }
    return true;
}

void Platform_unlock_request()
{
    pthread_mutex_unlock(&sending_mutex);
}

unsigned long long Platform_get_timestamp_ms_epoch()
{
    struct timespec spec;

    // Get timestamp in ms since epoch
    clock_gettime(CLOCK_REALTIME, &spec);
    return ((unsigned long long) spec.tv_sec) * 1000 + (spec.tv_nsec) / 1000 / 1000;
}

unsigned long long Platform_get_timestamp_ms_monotonic()
{
    struct timespec spec;

    // Get timestamp in ms since epoch
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return ((unsigned long long) spec.tv_sec) * 1000 + (spec.tv_nsec) / 1000 / 1000;
}

void * Platform_malloc(size_t size)
{
    LOGD("M: %d\n", size);
    return malloc(size);
}

void Platform_free(void *ptr, size_t size)
{
    free(ptr);
    LOGD("F: %d\n", size);
}

bool Platform_init(Platform_get_indication_f get_indication_f,
                   Platform_dispatch_indication_f dispatch_indication_f)
{
    /* This linux implementation uses a dedicated thread
     * to poll for indication. The indication are then handled
     * by this thread.
     * All the other API calls can be made on different threads
     * as this platform implements the lock mechanism in order
     * to protect the access to critical sections.
     */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

    if (get_indication_f == NULL || dispatch_indication_f == NULL)
    {
        LOGE("Invalid parameters\n");
        return false;
    }

    m_get_indication_f = get_indication_f;
    m_dispatch_indication_f = dispatch_indication_f;

    // Initialize mutex to access critical section
    if (pthread_mutex_init(&sending_mutex, &attr) != 0)
    {
        LOGE("Sending Mutex init failed\n");
        goto error1;
    }

    // Initialize mutex to access queue
    if (pthread_mutex_init(&m_queue_mutex, &attr) != 0)
    {
        LOGE("Queue Mutex init failed\n");
        goto error2;
    }

    // Start a thread to poll for indication
    if (pthread_create(&thread_polling, NULL, poll_for_indication, NULL) != 0)
    {
        LOGE("Cannot create polling thread\n");
        goto error3;
    }

    m_dispatch_thread_running = true;
    // Start a thread to dispatch indication
    if (pthread_create(&thread_dispatch, NULL, dispatch_indication, NULL) != 0)
    {
        LOGE("Cannot create dispatch thread\n");
        goto error4;
    }

    return true;

error4:
    pthread_kill(thread_polling, SIGKILL);
error3:
    pthread_mutex_destroy(&m_queue_mutex);
error2:
    pthread_mutex_destroy(&sending_mutex);
error1:
    return false;
}

void Platform_close()
{
    void * res;
    pthread_t cur_thread = pthread_self();

    // Signal our polling thread to stop
    // No need to signal it as it will wakeup periodically
    m_polling_thread_state_request = POLLING_THREAD_STOP_REQUESTED;

    // Wait for polling tread to finish
    if (cur_thread != thread_polling)
    {
        pthread_join(thread_polling, &res);
    }

    // Signal our dispatch thread to stop
    m_dispatch_thread_running = false;
    // Signal condition to wakeup thread
    pthread_cond_signal(&m_queue_not_empty_cond);

    // Wait for dispatch tread to finish
    if (cur_thread != thread_dispatch)
    {
        pthread_join(thread_dispatch, &res);
    }

    // Destroy our mutexes
    pthread_mutex_destroy(&m_queue_mutex);
    pthread_mutex_destroy(&sending_mutex);
}
