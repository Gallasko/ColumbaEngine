#pragma once

#include "Interpreter/token.h"

#include "chunk.h"

#include "parser.h"

#include <iostream>
#include <iomanip>
#include <queue>

// #define DEBUG_PRINT_TOKENS
// #define DEBUG_PRINT_CODE

namespace pg
{
    struct Compiler
    {
        bool compile(std::queue<Token> tokens, Chunk& chunk);

        void printTokens(std::queue<Token> tokens);

        Parser parser;
    };
}