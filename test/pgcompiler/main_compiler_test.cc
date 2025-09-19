#include "gtest/gtest.h"
#include <iostream>

/**
 * Entry point for PgCompiler tests
 */
int main(int argc, char **argv)
{
    std::cout << "Starting PgCompiler tests..." << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}