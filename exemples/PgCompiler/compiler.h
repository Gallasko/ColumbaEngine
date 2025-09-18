#pragma once

#include "Interpreter/token.h"

#include <iostream>
#include <iomanip>
#include <queue>

namespace pg
{
    struct Chunk;

    struct Compiler
    {
        bool compile(std::queue<Token> tokens, Chunk& chunk);
    };
}