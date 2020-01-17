/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdlib.h>  // For NULL
#include <string.h>  // For memcpy()
#include <stdbool.h>
#include <windows.h>

#define LOG_MODULE_NAME "plat_win32"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"
#include "platform.h"
#include "wpc_internal.h"

// Maximum number of indication to be retrieved from a single poll
#define MAX_NUMBER_INDICATION 30

// Polling interval to check for indication
#define POLLING_INTERVAL_MS 20

// Maximum duration in s of failed poll request to declare the link broken
// (unplugged) Set it to 0 to disable 60 sec is long period but it must cover
// the OTAP exchange with neighbors that can be long with some profiles
#define DEFAULT_MAX_POLL_FAIL_DURATION_S 60

// Critical section for sending, i.e. serial port access
static CRITICAL_SECTION m_sending_cs;

// This thread is used to poll for indication
static HANDLE thread_polling = NULL;

// This thread is used to dispatch indication
static HANDLE thread_dispatch = NULL;

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
static volatile unsigned int m_ind_queue_write = 0;

// Tail of the queue to read from dispatching thread
static volatile unsigned int m_ind_queue_read = 0;

// Is queue empty?
static volatile bool m_queue_empty = true;

// Critical section and condition variable
// used for the dispatching queue to wait
static CRITICAL_SECTION m_queue_cs;
static CONDITION_VARIABLE m_queue_not_empty_cond;

/*****************************************************************************/
/*                Dispatch indication Thread implementation                  */
/*****************************************************************************/
/**
 * \brief   Thread to dispatch indication in a non locked environment
 */
static DWORD WINAPI dispatch_indication(LPVOID unused)
{
    (void) unused;

    LOGD("Dispatch thread started\n");

    // Acquire ownership of queue critical section initially
    EnterCriticalSection(&m_queue_cs);

    while (true)
    {
        while (m_queue_empty)
        {
            // Queue is empty, release ownership and wait
            SleepConditionVariableCS(&m_queue_not_empty_cond, &m_queue_cs, INFINITE);
        }

        // Get the oldest indication
        timestamped_frame_t * ind = &m_indications_queue[m_ind_queue_read];

        // Handle the indication without the queue critical section
        LeaveCriticalSection(&m_queue_cs);

        // Dispatch the indication
        WPC_Int_dispatch_indication(&ind->frame, ind->timestamp_ms_epoch);

        // Re-acquire ownership of queue critical section, to update empty
        // status and wait on condition again in next loop iteration
        EnterCriticalSection(&m_queue_cs);

        m_ind_queue_read = (m_ind_queue_read + 1) % MAX_NUMBER_INDICATION_QUEUE;
        if (m_ind_queue_read == m_ind_queue_write)
        {
            // Indication was the last one
            m_queue_empty = true;
        }
    }

    LOGE("Exiting dispatch thread\n");
    return 0;
}

/*****************************************************************************/
/*                Polling Thread implementation                              */
/*****************************************************************************/
static void onIndicationReceivedLocked(wpc_frame_t * frame, unsigned long long timestamp_ms)
{
    LOGD("Frame received with timestamp = %llu\n", timestamp_ms);

    // Acquire ownership of queue critical section, to
    // add an indication and update the empty status
    EnterCriticalSection(&m_queue_cs);

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
        WakeConditionVariable(&m_queue_not_empty_cond);
    }

    // Handle the indication without the queue critical section
    LeaveCriticalSection(&m_queue_cs);
}

/**
 * \brief   Utility function to get current timestamp in seconds
 */
static time_t get_timestamp_s(void)
{
    return Platform_get_timestamp_ms_epoch() / 1000;
}

/**
 * \brief   Polling tread
 *          This thread polls for indication and insert them to the queue
 *          shared with the dispatcher thread
 */
static DWORD WINAPI poll_for_indication(void * unused)
{
    (void) unused;

    LOGD("Polling thread started\n");

    unsigned int max_num_indication, free_buffer_room;
    int get_ind_res;
    uint32_t wait_before_next_polling_ms = 0;

    while (true)
    {
        Platform_usleep(wait_before_next_polling_ms * 1000);

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
                    // Poll request has failed for too long, just exit
                    break;
                }
            }
        }
    }

    LOGE("Exiting polling thread\n");

    // Full process must be exited if uart doesnt' work
    exit(EXIT_FAILURE);  // TODO: Don't do this
}

void Platform_usleep(unsigned int time_us)
{
    Sleep((time_us + 999) / 1000);  // Sleep at least 1 ms
}

bool Platform_lock_request(void)
{
    EnterCriticalSection(&m_sending_cs);
    return true;
}

void Platform_unlock_request(void)
{
    LeaveCriticalSection(&m_sending_cs);
}

unsigned long long Platform_get_timestamp_ms_epoch(void)
{
    // Get system time
    SYSTEMTIME now;
    GetSystemTime(&now);
    FILETIME filetime;

    // Convert to filetime, i.e. 100-nanosecond
    // intervals since January 1, 1601 (UTC)
    if (!SystemTimeToFileTime(&now, &filetime))
    {
        // Could not get time
        return 0ULL;
    }

    // Convert to 64 bits
    ULARGE_INTEGER timestamp;
    memcpy(&timestamp, &filetime, sizeof(FILETIME));

    // Return milliseconds since epoch (not UNIX epoch)
    return timestamp.QuadPart / 10000;
}

bool Platform_init(void)
{
    /* This win32 implementation uses a dedicated thread
     * to poll for indication. The indication are then handled
     * by this thread.
     * All the other API calls can be made on different threads
     * as this platform implements the lock mechanism in order
     * to protect the access to critical sections.
     */

    // Initialize a condition variable for queue not empty
    InitializeConditionVariable(&m_queue_not_empty_cond);

    // Initialize a crical section to access queue
    InitializeCriticalSection(&m_queue_cs);

    // Create a crical section to access serial port
    InitializeCriticalSection(&m_sending_cs);

    // Start a thread to poll for indication
    thread_polling = CreateThread(NULL,  // Default security attributes
                                  0,     // Default stack size
                                  poll_for_indication,  // Thread function
                                  NULL,   // Arguments to thread function
                                  0,      // Default creation flags
                                  NULL);  // Do not store thread ID
    if (!thread_polling)
    {
        LOGE("Cannot create polling thread\n");
        Platform_close();
        return false;
    }

    // Start a thread to dispatch indication
    thread_dispatch = CreateThread(NULL,  // Default security attributes
                                   0,     // Default stack size
                                   dispatch_indication,  // Thread function
                                   NULL,   // Arguments to thread function
                                   0,      // Default creation flags
                                   NULL);  // Do not store thread ID
    if (!thread_dispatch)
    {
        LOGE("Cannot create dispatch thread\n");
        Platform_close();
        return false;
    }

    m_last_successful_poll_ts = get_timestamp_s();
    m_max_poll_fail_duration_s = DEFAULT_MAX_POLL_FAIL_DURATION_S;

    return true;
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

void Platform_close(void)
{
    if (thread_dispatch)
    {
        // TODO: Request dispatch thread to exit

        // Wait for dispatch thread to exit
        WaitForSingleObject(thread_dispatch, INFINITE);

        // Close dispatch thread handle to free allocated resources
        CloseHandle(thread_dispatch);
        thread_dispatch = NULL;
    }

    if (thread_polling)
    {
        // TODO: Request polling thread to exit

        // Wait for polling thread to exit
        WaitForSingleObject(thread_polling, INFINITE);

        // Close polling thread handle to free allocated resources
        CloseHandle(thread_polling);
        thread_polling = NULL;
    }
}
