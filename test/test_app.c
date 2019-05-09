/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test.h"
#include "wpc.h"

#define LOG_MODULE_NAME "TestApp"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

// Default serial port
char * port_name = "/dev/ttyACM0";

int main(int argc, char * argv[])
{
    unsigned long bitrate = DEFAULT_BITRATE;

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

    Test_runAll();

    Test_scratchpad();

    WPC_close();
    return 0;
}
