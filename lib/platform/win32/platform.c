/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdbool.h>

#define LOG_MODULE_NAME "linux_plat"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"
#include "wpc_internal.h"

void Platform_usleep(unsigned int time_us)
{
    // TODO
    (void) time_us;
}

bool Platform_lock_request(void)
{
    // TODO
    return false;
}

void Platform_unlock_request(void)
{
    // TODO
}

unsigned long long Platform_get_timestamp_ms_epoch(void)
{
    // TODO
    return 0ULL;
}

bool Platform_init(void)
{
    // TODO
    return false;
}

bool Platform_set_max_poll_fail_duration(unsigned long duration_s)
{
    // TODO
    (void) duration_s;
    return false;
}

void Platform_close(void)
{
    // TODO
}
