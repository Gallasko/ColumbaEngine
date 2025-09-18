#include "compiler.h"

#include "chunk.h"

namespace pg
{
    bool Compiler::compile(std::queue<Token> tokens, Chunk& chunk)
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

        return true;
    }
}