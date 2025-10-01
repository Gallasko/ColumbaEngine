#pragma once

#include "Interpreter/token.h"

#include "chunk.h"

#include "object.h"

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

    enum class FunctionType
    {
        TYPE_FUNCTION,
        TYPE_SCRIPT
    };

    struct Compiler
    {
        Compiler() {}
        ~Compiler() { 
            if (currentFunction) {
                delete currentFunction; 
            }
        }

        ObjFunction* compile(std::queue<Token> tokens);

        void printTokens(std::queue<Token> tokens);

        void beginScope();
        void endScope();

        void addLocal(const Token& name);
        int resolveLocal(const Token& name);

        void markInitialized();

        void reset();

        Chunk& getCurrentChunk() { return currentFunction->chunk; }

        // New methods for nested compiler support
        void initCompiler(Compiler* enclosing, FunctionType type);
        ObjFunction* endCompiler();

        Parser parser;

        // Modified to support nested compilers
        Compiler* enclosing = nullptr;  // Points to parent compiler
        ObjFunction* currentFunction = nullptr;
        FunctionType currentType = FunctionType::TYPE_SCRIPT;

        std::vector<Local> locals;

        int localCount = 0;
        int scopeDepth = 0;
    };
}