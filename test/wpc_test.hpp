#pragma once

extern "C" {
  #include <wpc.h>
}

#include <cstring>
#include <gtest/gtest.h>

#define ASSERT_EQ_ARRAY(first, second, size) \
    ASSERT_EQ(0, std::memcmp(first, second, size));

enum class NodeStackProfile
{
    INVALID,
    ISM_24,
    DECT_NR,
};

class WpcTestEnvironment : public testing::Environment
{
public:
    void SetUp() override;
    void TearDown() override;
    static bool IsInitialized();
    static NodeStackProfile GetProfile();
private:
    std::string GetSerialPort() const;
    unsigned long GetBaudRate() const;
    static void ReadStackProfile();
    inline static bool is_initialized = false;
    inline static NodeStackProfile profile = NodeStackProfile::ISM_24;
};

class WpcTest : public testing::Test
{
public:
    static void SetUpTestSuite();
protected:
    static void StartStack();
    static void StopStack();
    static NodeStackProfile GetProfile();
};

