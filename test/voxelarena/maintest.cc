#include "stdafx.h"

#include <gtest/gtest.h>

#include "logger.h"

int main(int argc, char** argv)
{
    // Content errors are expected in some tests: keep the log quiet unless asked
    if (std::getenv("VOXELARENA_TEST_LOG"))
        pg::Logger::registerSink<pg::TerminalSink>();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
