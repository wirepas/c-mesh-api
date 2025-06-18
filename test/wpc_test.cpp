#include "wpc_test.hpp"

extern "C" {
  #define LOG_MODULE_NAME (char*) "wpc_test"
  #include <logger.h>
}

std::string WpcTestEnvironment::GetSerialPort() const
{
    const auto envVar = std::getenv("WPC_SERIAL_PORT");
    if (envVar != nullptr) {
        return envVar;
    } else {
        return "/dev/ttyACM0";
    }
}

unsigned long WpcTestEnvironment::GetBaudRate() const
{
    const auto envVar = std::getenv("WPC_BAUD_RATE");
    if (envVar != nullptr) {
        return atoi(envVar);
    } else {
        return 125000;
    }
}

void WpcTestEnvironment::SetUp()
{
    const auto& serial_port = GetSerialPort();
    const auto baud_rate = GetBaudRate();

    Platform_set_log_level(NO_LOG_LEVEL);

    if (WPC_initialize(serial_port.c_str(), baud_rate) != APP_RES_OK) {
        std::cerr << "Could not initialize WPC with port: " << serial_port
                  << " baud rate:" << baud_rate << std::endl;
        is_initialized = false;
    } else {
        is_initialized = true;
    }
}

void WpcTestEnvironment::TearDown()
{
    WPC_close();
}

bool WpcTestEnvironment::IsInitialized()
{
    return is_initialized;
}

void WpcTest::SetUpTestSuite()
{
    ASSERT_TRUE(WpcTestEnvironment::IsInitialized()) << "Environment is not initialized";
}

void WpcTest::StartStack()
{
    const auto res = WPC_start_stack();
    ASSERT_TRUE(APP_RES_OK == res || APP_RES_STACK_ALREADY_STARTED == res);
}

void WpcTest::StopStack()
{
    const auto res = WPC_stop_stack();
    ASSERT_TRUE(APP_RES_OK == res || APP_RES_STACK_ALREADY_STOPPED == res);
}

