#include "compiler.h"

#include "chunk.h"

#include "compiler_debug.h"

namespace pg
{
    // Define the static member
    Compiler* Compiler::current = nullptr;
    ObjFunction* Compiler::compile(std::queue<Token> tokens)
    {
        // Initialize this compiler as the root script compiler
        initCompiler(FunctionType::TYPE_SCRIPT);

#ifdef DEBUG_PRINT_TOKENS
        printTokens(tokens);
#endif

        parser.parse(tokens);
        parser.setCompiler(this);

        while (not parser.isAtEnd() and not parser.hasError())
            parser.declaration();

        return parser.hasError() ? nullptr : endCompiler();
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
            // if (locals[localCount - 1].isCaptured)
            // {
            //     writeByte(chunk, OpCode::OP_Close_Upvalue);
            // }
            // else
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
        currentFunction->chunk.clear();

        parser.reset();
    }

    void Compiler::initCompiler(FunctionType type, const std::string& name)
    {
        this->enclosing = Compiler::current;
        this->currentType = type;

        // Create new function object
        if (currentFunction)
        {
            delete currentFunction;
        }

        currentFunction = new ObjFunction();

        // Reset local state for this new function
        locals.clear();
        localCount = 0;
        scopeDepth = 0;

        // Set this as the current compiler
        Compiler::current = this;

        currentFunction->name = name;
    }

    ObjFunction* Compiler::endCompiler()
    {
        // Emit implicit return for functions that don't have an explicit return
        parser.emitReturn();

#ifdef DEBUG_PRINT_CODE
        if (not parser.hadError)
        {
            disassembleChunk(getCurrentChunk(), currentFunction != nullptr ? currentFunction->name : "<script>");
        }
#endif

        ObjFunction* function = currentFunction;
        currentFunction = nullptr;

        // Restore the previous compiler as current
        Compiler::current = enclosing;

        return function;
    }
}