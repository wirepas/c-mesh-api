#pragma once

extern "C" {
  #include <wpc.h>
}

#include <cstring>
#include <gtest/gtest.h>

#define ASSERT_EQ_ARRAY(first, second, size) \
    ASSERT_EQ(0, std::memcmp(first, second, size));

class WpcTestEnvironment : public testing::Environment
{
public:
    void SetUp() override;
    void TearDown() override;
    static bool IsInitialized();
private:
    std::string GetSerialPort() const;
    unsigned long GetBaudRate() const;
    inline static bool is_initialized = false;
};

class WpcTest : public testing::Test
{
public:
    static void SetUpTestSuite();
};

