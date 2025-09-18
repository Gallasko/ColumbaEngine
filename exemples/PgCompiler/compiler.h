#pragma once

#include "Interpreter/lexer.h"

#include <iostream>
#include <iomanip>

namespace pg
{
    struct Compiler
    {
        void compile(const std::string& source)
        {
            Lexer lexer;

            lexer.readFromText(source);

            auto tokens = lexer.getTokens();

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
    };
}