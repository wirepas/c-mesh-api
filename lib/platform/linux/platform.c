/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define LOG_MODULE_NAME "linux_plat"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"
#include "wpc_internal.h"

// Maximum number of indication to be retrieved from a single poll
#define MAX_NUMBER_INDICATION 30

// Polling interval to check for indication
#define POLLING_INTERVAL_MS 20

// Maximum duration in s of failed poll request to declare the link broken
// (unplugged) Set it to 0 to disable 60 sec is long period but it must cover
// the OTAP exchange with neighbors that can be long with some profiles
#define DEFAULT_MAX_POLL_FAIL_DURATION_S 60

// Mutex for sending, ie serial access
static pthread_mutex_t sending_mutex;

// This thread is used to poll for indication
static pthread_t thread_polling;

// Set to false to stop polling thread execution
static bool m_polling_thread_running;

// This thread is used to dispatch indication
static pthread_t thread_dispatch;

// Set to false to stop dispatch thread execution
static bool m_dispatch_thread_running;

// Last successful poll request
static time_t m_last_successful_poll_ts;
static unsigned int m_max_poll_fail_duration_s;

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

// Mutex to stop the polling
static pthread_mutex_t m_poll_mutex;

/*****************************************************************************/
/*                Dispatch indication Thread implementation                  */
/*****************************************************************************/
/**
 * \brief   Thread to dispatch indication in a non locked environment
 */
static void * dispatch_indication(void * unused)
{
    pthread_mutex_lock(&m_queue_mutex);
    while (m_dispatch_thread_running)
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

        // Get the oldest indication
        timestamped_frame_t * ind = &m_indications_queue[m_ind_queue_read];

        // Handle the indication without the queue lock
        pthread_mutex_unlock(&m_queue_mutex);

        // Dispatch the indication
        WPC_Int_dispatch_indication(&ind->frame, ind->timestamp_ms_epoch);

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
 * \brief   Utility function to get current timesatmp in s.
 */
static time_t get_timestamp_s()
{
    struct timespec spec;

    // Get timestamp in ms since epoch
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_sec;
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
    uint32_t wait_before_next_polling_ms = 0;

    while (m_polling_thread_running)
    {
        usleep(wait_before_next_polling_ms * 1000);

        // Get the lock to be able to poll
        pthread_mutex_lock(&m_poll_mutex);

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

            // Release the poll mutex
            pthread_mutex_unlock(&m_poll_mutex);
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

        max_num_indication = free_buffer_room > MAX_NUMBER_INDICATION ?
                                 MAX_NUMBER_INDICATION :
                                 free_buffer_room;

        LOGD("Poll for %d indications\n", max_num_indication);

        get_ind_res = WPC_Int_get_indication(max_num_indication, onIndicationReceivedLocked);

        if (get_ind_res == 1)
        {
            // Still pending indication, only wait 1 ms to give a chance
            // to other threads but not more to have better throughput
            wait_before_next_polling_ms = 1;
        }
        else
        {
            // In case of error or if no more indication, just wait
            // the POLLING INTERVAL to avoid polling all the time
            wait_before_next_polling_ms = POLLING_INTERVAL_MS;
        }

        if (m_max_poll_fail_duration_s > 0)
        {
            if (get_ind_res >= 0 || get_ind_res == WPC_INT_SYNC_ERROR)
            {
                // Poll request executed fine or at least com is working with sink,
                // reset fail counter
                m_last_successful_poll_ts = get_timestamp_s();
            }
            else
            {
                if (get_timestamp_s() - m_last_successful_poll_ts > m_max_poll_fail_duration_s)
                {
                    // Poll request has failed for too long
                    // This is a fatal error as the com with sink was not possible
                    // for a too long period.
                    exit(EXIT_FAILURE);
                    break;
                }
            }
        }

        // Release the poll mutex
        pthread_mutex_unlock(&m_poll_mutex);
    }

    LOGW("Exiting polling thread\n");

    return NULL;
}

void Platform_usleep(unsigned int time_us)
{
    usleep(time_us);
}

bool Platform_lock_request()
{
    int res = pthread_mutex_lock(&sending_mutex);
    if (res != 0)
    {
        // It must never happen but add a check and
        // return to avoid a deadlock
        LOGE("Mutex already locked %d\n", res);
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

bool Platform_init()
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

    // Initialize mutex to prevent polling
    if (pthread_mutex_init(&m_poll_mutex, &attr) != 0)
    {
        LOGE("Polling Mutex init failed\n");
        goto error2_2;
    }

    m_polling_thread_running = true;
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

    m_last_successful_poll_ts = get_timestamp_s();
    m_max_poll_fail_duration_s = DEFAULT_MAX_POLL_FAIL_DURATION_S;

    return true;

error4:
    pthread_kill(thread_polling, SIGKILL);
error3:
    pthread_mutex_destroy(&m_poll_mutex);
error2_2:
    pthread_mutex_destroy(&m_queue_mutex);
error2:
    pthread_mutex_destroy(&sending_mutex);
error1:
    return false;
}

bool Platform_set_max_poll_fail_duration(unsigned long duration_s)
{
    if ((m_max_poll_fail_duration_s == 0) && (duration_s > 0))
    {
        m_last_successful_poll_ts = get_timestamp_s();
    }
    m_max_poll_fail_duration_s = duration_s;
    return true;
}

void Platform_valid_message_from_node()
{
    m_last_successful_poll_ts = get_timestamp_s();
}

bool Platform_disable_poll_request(bool disabled)
{
    int res;
    if (disabled)
    {
        // Polling thread must be stopped, lock the poll mutex
        res = pthread_mutex_lock(&m_poll_mutex);
    }
    else
    {
        // Polling thread must be started again, unlock the poll mutex
        res = pthread_mutex_unlock(&m_poll_mutex);
    }

    if (res != 0)
    {
        // It must never happen but add a check and
        // return to avoid a deadlock
        LOGE("Poll Mutex already locked or unlocked %d (%d)\n", res, disabled);
        return false;
    }

    return true;
}

void Platform_close()
{
    void * res;
    // Signal our dispatch thread to stop
    m_dispatch_thread_running = false;
    // Signal condition to wakeup thread
    pthread_cond_signal(&m_queue_not_empty_cond);

    // Signal our polling thread to stop
    // No need to signal it as it will wakeup periodically
    m_polling_thread_running = false;

    // Wait for both tread to finish
    pthread_join(thread_polling, &res);
    pthread_join(thread_dispatch, &res);

    // Destroy our mutexes
    pthread_mutex_destroy(&m_queue_mutex);
    pthread_mutex_destroy(&m_poll_mutex);
    pthread_mutex_destroy(&sending_mutex);
}
