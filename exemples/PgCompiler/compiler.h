#pragma once

#include "Interpreter/token.h"

#include "chunk.h"

#include "object.h"

#include "parser.h"

#include <iostream>
#include <iomanip>
#include <queue>

namespace pg
{
    struct Local
    {
        Token name;
        int depth;
        bool isCaptured = false;
    };

    struct Upvalue
    {
        uint8_t index;
        bool isLocal;
    };

    struct VM;

    struct Compiler
    {
        VM *vm;

        Compiler(VM *vm) : vm(vm), parser(vm) {}

        ~Compiler();

        Value compile(std::queue<Token> tokens);

        void printTokens(std::queue<Token> tokens);

        void beginScope();
        void endScope();

        void addLocal(const Token& name);
        int findLocal(const std::string& name);
        int resolveLocal(const Token& name);

        int addUpvalue(int index, bool isLocal);
        int findUpvalue(const std::string& name);
        int resolveUpvalue(const Token& name);

        void markInitialized();

        void reset();

        Chunk& getCurrentChunk();

        // New methods for nested compiler support
        void initCompiler(FunctionType type, const std::string& name = "<script>");
        Value endCompiler();

        Parser parser;

        // Modified to support nested compilers
        Compiler* enclosing = nullptr;  // Points to parent compiler
        Value currentFunction = 0x0;
        FunctionType currentType = FunctionType::TYPE_SCRIPT;

        std::vector<Local> locals;

        int localCount = 0;
        int scopeDepth = 0;

        Upvalue upvalues[UINT8_MAX];

        // Static member for tracking current compiler in the stack
        static Compiler* current;

        struct ClassCompiler { ClassCompiler* enclosing = nullptr;  };

        ClassCompiler* currentClass = nullptr;
    };
}