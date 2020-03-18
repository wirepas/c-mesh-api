/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wpc.h"
#include "platform.h"
#include "test_mode.h"

#define LOG_MODULE_NAME "test_mode_app"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

// Default serial port
#ifdef PLATFORM_IS_WIN32
static char * port_name = "COM22";
#elif PLATFORM_IS_LINUX
static char * port_name = "/dev/ttyUSB0";
#elif PLATFORM_IS_DARWIN
static char * port_name = "/dev/cu.usbmodem0001234567890";
#else
static char * port_name = "/dev/ttyS0";
#endif

int main(int argc, char * argv[])
{
    unsigned long bitrate = DEFAULT_BITRATE;
    LOGI("## Running test mode tests, requires FW with test-library\n");
    if (argc > 1)
    {
        port_name = argv[1];
    }

    if (argc > 2)
    {
        bitrate = strtoul(argv[2], NULL, 0);
    }

    if (WPC_initialize(port_name, bitrate) != APP_RES_OK)
        return -1;

#if TXNODE
    LOGI("Transmitting...\n");
    if (Set_Test_Mode() != true)
        return 0;

    if (Set_Radio_Channel() != true)
        return 0;

   if (Set_Radio_Power() != true)
        return 0;

    // Sending full radio packets forever
    if (Send_Radio_Data() != true)
    {
        LOGI("Fail");
        return 0;
    }

    /* Another option to send predefined signals
    if (Send_Radio_Signal() != true)
    {
        LOGI("Fail");
        return 0;
    }
    */
#endif

#if RXNODE
    LOGI("Receiving...\n");
    if (Set_Test_Mode() != true)
        return 0;

    if (Set_Radio_Channel() != true)
        return 0;

    if (Enable_Radio_Reception() != true)
        return 0;

    if (Read_Radio_Data() != true)
        return 0;
#endif

    WPC_close();
    LOGE("## Done.\n");
    return 0;
}
