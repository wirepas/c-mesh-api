#include <gtest/gtest.h>

#include "wpc_test.hpp"

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::AddGlobalTestEnvironment(new WpcTestEnvironment());
    return RUN_ALL_TESTS();
}

