#include <serial_stub.h>

extern "C" {
#include <serial.h>

    int Serial_open(const char * port_name, unsigned long bitrate)
    {
        return 0;
    }

    int Serial_close()
    {
        return 0;
    }

    int Serial_read(unsigned char * c, unsigned int timeout_ms)
    {
        if (const auto& byte = serial_stub.PopOutByte(); byte.has_value())
        {
            *c = *byte;
            return 1;
        }

        return 0;
    }

    int Serial_write(const unsigned char * buffer, unsigned int buffer_size)
    {
        // This is ok because in practice, c-mesh-api always sends a whole
        // frame with a single Serial_write command.
        serial_stub.ReadAndHandleFrame(buffer, buffer_size);
        return buffer_size;
    }

}

