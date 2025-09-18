#include "compiler.h"

#include "chunk.h"

#include "compiler_debug.h"

namespace pg
{
    bool Compiler::compile(std::queue<Token> tokens, Chunk& chunk)
    {
#ifdef DEBUG_PRINT_TOKENS
        printTokens(tokens);
#endif

        parser.parse(tokens);

        parser.expression(chunk);

#ifdef DEBUG_PRINT_CODE
        if (!parser.hadError)
        {
            disassembleChunk(chunk, "code");
        }
#endif

        // parser.consume("Expect end of expression.", TokenType::ENDOFFILE);

        return not parser.hasError();
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
}