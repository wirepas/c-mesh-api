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
static char * port_name = "/dev/ttyUSB0";

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

#    if TXSIGNAL
    // Send predefined test signal by FW
    if (Send_Radio_Signal() != true)
    {
        return 0;
    }
#    else
    // Create TX packages here and send them over the air
    if (Send_Radio_Data() != true)
    {
        return 0;
    }
#    endif

#endif

#if RXNODE
    LOGI("Receiving...\n");
    if (Set_Test_Mode() != true)
        return 0;

    if (Set_Radio_Channel() != true)
        return 0;

    if (Enable_Radio_Reception() != true)
        return 0;

    // TX node must use Send_Radio_Data() to get PER% calculations right in
    // Read_Radio_Data(). Send_Radio_Signal() des not send header containing
    // esential information for PER calculations.
    if (Read_Radio_Data() != true)
        return 0;
#endif

    WPC_close();
    LOGE("## Done.\n");
    return 0;
}
