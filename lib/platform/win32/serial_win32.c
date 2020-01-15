/* Wirepas Oy licensed under Apache License, Version 2.0
 *
 * See file LICENSE for full license details.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define LOG_MODULE_NAME "serial_win32"
#define MAX_LOG_LEVEL INFO_LOG_LEVEL
#include "logger.h"

static HANDLE serial_port_handle = INVALID_HANDLE_VALUE;

#if MAX_LOG_LEVEL >= ERROR_LOG_LEVEL

static char * format_error(unsigned long error)
{
    char * buffer = NULL;

    if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,  // Flags
                       NULL,              // Source (ignored)
                       error,             // Message ID
                       0,                 // Language ID: automatic
                       (LPTSTR) &buffer,  // Allocated buffer
                       256,               // Minimum allocate buffer size
                       NULL) == 0)        // Arguments (ignored)
    {
        buffer = NULL;
    }

    return buffer;
}

static void free_error(char * buffer)
{
    if (buffer)
    {
        LocalFree(buffer);
    }
}

static void print_last_error(const char * info)
{
    unsigned long last_error = GetLastError();
    char * buffer = format_error(last_error);
    LOGE("Error 0x%lx %s: %s\n", last_error, info, buffer ? buffer : "unknown error");
    free_error(buffer);
}

#else  // MAX_LOG_LEVEL >= ERROR_LOG_LEVEL
#    define print_last_error(message)
#endif  // MAX_LOG_LEVEL >= ERROR_LOG_LEVEL

int Serial_open(const char * port_name, unsigned long bitrate)
{
    if (serial_port_handle != INVALID_HANDLE_VALUE)
    {
        LOGE("Serial port already open\n");
        return -1;
    }

    // Convert port name to Win32 device namespace form
    char filename_buf[MAX_PATH];
    const char * ns_prefix = "\\\\.\\";
    if (strncmp(port_name, ns_prefix, 4) == 0)
    {
        // Port name is already in Win32 device namespace
        ns_prefix = "";
    }
    snprintf(filename_buf, MAX_PATH, "%s%s", ns_prefix, port_name);

    // Open serial port
    serial_port_handle = CreateFileA(filename_buf,  // Port filename
                                     GENERIC_READ | GENERIC_WRITE,  // Read/Write
                                     0,              // No sharing
                                     NULL,           // No security
                                     OPEN_EXISTING,  // Open existing port only
                                     0,              // Non-overlapped I/O
                                     NULL);          // NULL for comm devices

    if (serial_port_handle == INVALID_HANDLE_VALUE)
    {
        char buffer[MAX_PATH + 40];
        snprintf(buffer, sizeof(buffer), "opening serial port %s", port_name);
        print_last_error(buffer);
        return -1;
    }

    // Initialize an empty DCB
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    // Create a settings string for BuildCommDCBA()
    char param_buf[80];
    snprintf(param_buf, sizeof(param_buf), "baud=%lu parity=N data=8 stop=1", bitrate);

    // Create a timeout struct for SetCommTimeouts()
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;  // Immediate

    // Configure serial port
    if (!BuildCommDCBA(param_buf, &dcb))
    {
        print_last_error("calling BuildCommDCBA");
    }
    else if (!SetCommState(serial_port_handle, &dcb))
    {
        print_last_error("calling SetCommState");
    }
    else if (!SetCommTimeouts(serial_port_handle, &timeouts))
    {
        print_last_error("calling SetCommTimeouts");
    }
    else
    {
        LOGD("Serial port opened\n");
        return 0;
    }

    // Error setting serial port parameters, close port and leave
    CloseHandle(serial_port_handle);
    serial_port_handle = INVALID_HANDLE_VALUE;
    return -1;
}

int Serial_close(void)
{
    if (serial_port_handle == INVALID_HANDLE_VALUE)
    {
        LOGE("Serial port not open\n");
        return -1;
    }

    if (!CloseHandle(serial_port_handle))
    {
        print_last_error("closing serial port");
        return -1;
    }

    serial_port_handle = INVALID_HANDLE_VALUE;
    LOGD("Serial port closed\n");
    return 0;
}

int Serial_read(unsigned char * c, unsigned int timeout_ms)
{
    if (serial_port_handle == INVALID_HANDLE_VALUE)
    {
        LOGE("Serial port not open\n");
        return -1;
    }

    // Create a timeout struct for SetCommTimeouts()
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = timeout_ms;

    // Set read timeout
    if (!SetCommTimeouts(serial_port_handle, &timeouts))
    {
        print_last_error("calling SetCommTimeouts");
        return -1;
    }

    // Read one byte from serial port, with timeout
    DWORD num_bytes;
    if (!ReadFile(serial_port_handle, c, 1, &num_bytes, NULL))
    {
        print_last_error("calling ReadFile");
        return -1;
    }
    else if (num_bytes == 0)
    {
        LOGD("Timeout to wait for char on serial port\n");
    }

    return (int) num_bytes;
}

int Serial_write(const unsigned char * buffer, size_t buffer_size)
{
    if (serial_port_handle == INVALID_HANDLE_VALUE)
    {
        LOGE("Serial port not open\n");
        return -1;
    }

    DWORD num_bytes;
    if (!WriteFile(serial_port_handle, buffer, buffer_size, &num_bytes, NULL))
    {
        print_last_error("calling WriteFile");
        return -1;
    }
    else if (num_bytes < buffer_size)
    {
        LOGW("Short write to serial port\n");
    }

    return (int) num_bytes;
}
