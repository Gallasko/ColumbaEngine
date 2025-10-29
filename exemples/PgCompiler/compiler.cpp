#include "compiler.h"

#include "vm.h"

#include "chunk.h"

#include "compiler_debug.h"

namespace pg
{
    // Define the static member
    Compiler* Compiler::current = nullptr;

    Compiler::~Compiler()
    {
        if (currentFunction)
        {
            vm->pools.functionPool.release(vm->asFunction(currentFunction));
        }
    }

    Value Compiler::compile(std::queue<Token> tokens)
    {
        // Initialize this compiler as the root script compiler
        initCompiler(FunctionType::TYPE_SCRIPT);

#ifdef DEBUG_PRINT_TOKENS
        printTokens(tokens);
#endif

        parser.parse(tokens);
        parser.setCompiler(this);

        while (not parser.isAtEnd() and not parser.hasError())
        {
            parser.skipEOL();  // Skip leading EOL tokens (e.g., from comment-only lines)
            parser.declaration();
        }

        if (parser.hasError())
            return 0x0;

        auto func = endCompiler();

        parser.allocatedFunction.push_back(func);

        return func;
    }

    void Compiler::printTokens(std::queue<Token> tokens)
    {
        int line = -1;

        while (not tokens.empty())
        {
            Token token = tokens.front();
            tokens.pop();

            if (static_cast<int>(token.line) != line)
            {
                std::cout << std::setw(4) << token.line << " ";
                line = token.line;
            }
            else
            {
                std::cout << "   | ";
            }

            std::cout << std::setw(4) << token.column << " ";
            std::cout << token.text << std::endl;

            if (token.type == TokenType::ENDOFFILE)
                break;
        }
    }

    void Compiler::beginScope()
    {
        scopeDepth++;
    }

    void Compiler::endScope()
    {
        scopeDepth--;

        while (localCount > 0 and locals[localCount - 1].depth > scopeDepth)
        {
            // If the local variable is a captured variable we need to emit a different instruction
            if (locals[localCount - 1].isCaptured)
            {
                parser.writeByte(OpCode::OP_Close_Upvalue);
            }
            else
            {
                parser.writeByte(OpCode::OP_Pop);
            }

            localCount--;
            locals.pop_back();
        }
    }

    void Compiler::addLocal(const Token& name)
    {
        locals.push_back(Local{name, -1});
        localCount++;
    }

    int Compiler::findLocal(const std::string& name)
    {
        for (int i = localCount - 1; i >= 0; i--)
        {
            if (locals[i].name.text == name)
            {
                return i;
            }
        }

        return -1;
    }

    int Compiler::resolveLocal(const Token& name)
    {
        for (int i = localCount - 1; i >= 0; i--)
        {
            if (locals[i].name.text == name.text)
            {
                if (locals[i].depth == -1)
                {
                    parser.errorAt(name, "Can't read local variable in its own initializer.");
                }

                return i;
            }
        }

        return -1;
    }

    int Compiler::addUpvalue(int index, bool isLocal)
    {
        // Check if upvalue already exists
        auto upvalueCount = vm->asFunction(currentFunction)->upvalueCount;
        for (int i = 0; i < upvalueCount; i++)
        {
            if (upvalues[i].index == index and upvalues[i].isLocal == isLocal)
            {
                return i;
            }
        }

        if (upvalueCount == UINT8_MAX)
        {
            parser.error("Too many closure variables in function.");
            return 0;
        }

        upvalues[upvalueCount].index = static_cast<uint8_t>(index);
        upvalues[upvalueCount].isLocal = isLocal;

        return vm->asFunction(currentFunction)->upvalueCount++;
    }

    int Compiler::findUpvalue(const std::string& name)
    {
        if (enclosing == nullptr)
            return -1;

        int localIndex = enclosing->findLocal(name);
        if (localIndex != -1)
        {
            return localIndex;
        }

        int upvalueIndex = enclosing->findUpvalue(name);
        if (upvalueIndex != -1)
        {
            return upvalueIndex;
        }

        return -1;
    }

    int Compiler::resolveUpvalue(const Token& name)
    {
        if (enclosing == nullptr)
            return -1;

        int localIndex = enclosing->resolveLocal(name);
        if (localIndex != -1)
        {
            // Mark the local as captured
            enclosing->locals[localIndex].isCaptured = true;
            return addUpvalue(localIndex, true);
        }

        int upvalueIndex = enclosing->resolveUpvalue(name);
        if (upvalueIndex != -1)
        {
            return addUpvalue(upvalueIndex, false);
        }

        return -1;
    }

    void Compiler::markInitialized()
    {
        if (scopeDepth == 0)
            return;

        locals[localCount - 1].depth = scopeDepth;
    }

    void Compiler::reset()
    {
        locals.clear();

        localCount = 0;
        scopeDepth = 0;

        // Clear the current function's chunk
        vm->asFunction(currentFunction)->chunk.clear();

        parser.reset();
    }

    Chunk& Compiler::getCurrentChunk() { return vm->asFunction(currentFunction)->chunk; }

    void Compiler::initCompiler(FunctionType type, const std::string& name)
    {
        this->enclosing = Compiler::current;
        this->currentType = type;

        // Inherit the class context from the enclosing compiler (for methods)
        if (this->enclosing != nullptr)
        {
            this->currentClass = this->enclosing->currentClass;
        }

        // Create new function object
        if (currentFunction)
        {
            vm->releaseAndDelete(currentFunction);
        }

        currentFunction = vm->createFunction();

        // Reset local state for this new function
        locals.clear();
        localCount = 0;
        scopeDepth = 0;

        // Set this as the current compiler
        Compiler::current = this;

        vm->asFunction(currentFunction)->name = name;

        if (type == FunctionType::TYPE_METHOD || type == FunctionType::TYPE_INITIALIZER)
        {
            // For methods and initializers, the first local is "this"
            locals.push_back(Local{Token(TokenType::TOK_FUN, "this", 0, 0), 0, false});
            localCount++;
        }
    }

    Value Compiler::endCompiler()
    {
        // Emit implicit return for functions that don't have an explicit return
        parser.emitReturn();

#ifdef DEBUG_PRINT_CODE
        if (not parser.hadError)
        {
            disassembleChunk(vm, getCurrentChunk(), currentFunction != 0x0 ? vm->asFunction(currentFunction)->name : "<script>");
        }
#endif

        Value function = currentFunction;
        currentFunction = 0x0;

        // Restore the previous compiler as current
        Compiler::current = enclosing;

        return function;
    }
}