#pragma once

#include "Interpreter/token.h"

#include "chunk.h"

#include "parser.h"

#include <iostream>
#include <iomanip>
#include <queue>

// #define DEBUG_PRINT_TOKENS
#define DEBUG_PRINT_CODE

namespace pg
{
    struct Local
    {
        Token name;
        int depth;
        // bool isCaptured;
    };

    struct Compiler
    {
        bool compile(std::queue<Token> tokens, Chunk& chunk);

        void printTokens(std::queue<Token> tokens);

        void beginScope();
        void endScope(Chunk& chunk);

        void addLocal(const Token& name);
        int resolveLocal(Chunk& chunk, const Token& name);

        void reset();

        Parser parser;

        std::vector<Local> locals;

        int localCount = 0;
        int scopeDepth = 0;
    };
}