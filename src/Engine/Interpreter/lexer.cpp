/**
 * @file lexer.cpp
 * @author Gallasko
 * @brief Implementation of the lexer class
 * @version 1.0
 * @date 2022-04-11
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "stdafx.h"

#include "lexer.h"

#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <sstream>

#include "Files/filemanager.h"

namespace pg
{

    namespace
    {
        /** The table of used keywords and the corresponding TokenType */
        const std::unordered_map<std::string, TokenType> keywords = {
            {"true",    TokenType::KEYTRUE},
            {"false",   TokenType::KEYFALSE},
            {"if",      TokenType::TOK_IF},
            {"else",    TokenType::TOK_ELSE},
            {"var",     TokenType::TOK_VAR},
            {"while",   TokenType::TOK_WHILE},
            {"for",     TokenType::TOK_FOR},
            {"and",     TokenType::LOGICAND},
            {"or",      TokenType::LOGICOR},
            {"not",     TokenType::NOT},
            {"fun",     TokenType::TOK_FUN},
            {"return",  TokenType::TOK_RETURN},
            {"class",   TokenType::TOK_CLASS},
            {"this",    TokenType::TOK_THIS},
            {"import",  TokenType::TOK_IMPORT},
            {"from",    TokenType::TOK_FROM},
            {"as",      TokenType::TOK_AS}

        };

        /**
         * @brief Helper function to convert a character to an op Token
         *
         * @param chara    The character that is currently being read form the file
         * @param nextChar The next character to be read from the file (for two characters op)
         *
         * @return The TokenType corresponding to the given character
         */
        TokenType charaToToken(const char chara, const char nextChar = ' ')
        {
            // Cast the character to TokenType, if the character is defined in the table it will be converted to the corresponding TokenType
            TokenType token = static_cast<TokenType>(chara);
            // Cast the character to TokenType, if the character is defined in the table it will be converted to the corresponding TokenType
            TokenType nextToken = static_cast<TokenType>(nextChar);

            // Check if the character is defined in the table
            switch (token)
            {
            // Case if the token is a single op TokenType
            case TokenType::POW:
            case TokenType::PENTER:
            case TokenType::PCLOSE:
            case TokenType::BENTER:
            case TokenType::BCLOSE:
            case TokenType::CENTER:
            case TokenType::CCLOSE:
            case TokenType::QMARK:
            case TokenType::TILDE:
            case TokenType::COMMA:
            case TokenType::POINT:
            case TokenType::SMARK:
            case TokenType::DMARK:
            case TokenType::BSLASH:
            case TokenType::HTAG:
            case TokenType::END:
                return token;
                break;

            // Case if the token can be found inside a two character op TokenType
            case TokenType::EQUAL:
                if (nextToken == TokenType::EQUAL)
                    return TokenType::EQUALEQUAL;
                else
                    return token;
                break;

            case TokenType::NOT:
                if (nextToken == TokenType::EQUAL)
                    return TokenType::NOTEQUAL;
                else
                    return token;
                break;

            case TokenType::PLUS:
                if (nextToken == TokenType::EQUAL)
                    return TokenType::PLUSEQUAL;
                else if (nextToken == TokenType::PLUS)
                    return TokenType::INCREMENT;
                else
                    return token;
                break;

            case TokenType::MINUS:
                if (nextToken == TokenType::EQUAL)
                    return TokenType::MINUSEQUAL;
                else if (nextToken == TokenType::SUP)
                    return TokenType::ARROW;
                else if (nextToken == TokenType::MINUS)
                    return TokenType::DECREMENT;
                else
                    return token;
                break;

            case TokenType::STAR:
                if(nextToken == TokenType::EQUAL)
                    return TokenType::STAREQUAL;
                else
                    return token;
                break;

            case TokenType::SLASH:
                if(nextToken == TokenType::EQUAL)
                    return TokenType::DIVIDEQUAL;
                else
                    return token;
                break;

            case TokenType::MOD:
                if(nextToken == TokenType::EQUAL)
                    return TokenType::MODEQUAL;
                else
                    return token;
                break;

            case TokenType::AMPER:
                if(nextToken == TokenType::AMPER)
                    return TokenType::LOGICAND;
                else
                    return token;
                break;

            case TokenType::SSLASH:
                if(nextToken == TokenType::SSLASH)
                    return TokenType::LOGICOR;
                else
                    return token;
                break;

            case TokenType::SUP:
                if(nextToken == TokenType::EQUAL)
                    return TokenType::SUPEQUAL;
                else if(nextToken == TokenType::SUP)
                    return TokenType::SHIFTRIGHT;
                else
                    return token;
                break;

            case TokenType::INF:
                if(nextToken == TokenType::EQUAL)
                    return TokenType::INFEQUAL;
                else if(nextToken == TokenType::INF)
                    return TokenType::SHIFTLEFT;
                else
                    return token;
                break;

            case TokenType::DPOINT:
                if(nextToken == TokenType::DPOINT)
                    return TokenType::SCOPE;
                else
                    return token;
                break;

            // Return TokenType::NOOP if the character is not defined in the table as an op
            default:
                return TokenType::NOOP;
            }
        }

        /**
         * @brief Helper function to determine if a string is an integer
         *
         * @param s The string to be checked
         *
         * @return True if the string is an integer, false otherwise
         */
        bool isNumber(const std::string& s)
        {
            return !s.empty() && std::find_if(s.begin(),
                s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
        }

        /**
         * @brief Helper function to determine if a string is a float number
         *
         * @param s The string to be checked
         *
         * @return True if the string is a float, false otherwise
         */
        bool isFloat(const std::string& s)
        {
            std::istringstream iss(s);
            float f;
            iss >> std::noskipws >> f; // noskipws considers leading whitespace invalid
            // Check the entire string was consumed and if either failbit or badbit is set
            return iss.eof() && !iss.fail();
        }

        /**
         * @brief Helper function to determine the type of a string expression
         *
         * @param expression The string to be checked
         * @return A TokenType corresponding to the type of the expression
         *
         * This function compare the input string to the keyword list to see if the string is a reserved keyword
         * Then it check if the string is a number
         *
         * If both check failed then the string is an expression
         */
        TokenType strToKeyword(const std::string& expression)
        {
            if (keywords.find(expression) != keywords.end())
                return keywords.at(expression);

            if (isNumber(expression))
                return TokenType::NUMBER;

            if (isFloat(expression))
                return TokenType::FLOAT;

            return TokenType::EXPRESSION;
        }

        void parseToken(const std::string& stream, std::queue<Token>& tokens)
        {
            std::istringstream file(stream);

            // Start line and column at 1 to be easier to read
            int lineNumber = 1;
            int columnNumber = 1;

            std::string currentToken = "";

            bool isTokenNumber = false;
            bool isTokenString = false;
            int lNumberOfString = 1;
            int cNumberOfString = 1;
            int cNumberIfTokenIsString = 1; // Keep track of the column number separatly when parsing a string cause a string can span across multiples lines

            TokenType temp;

            for (std::string line; std::getline(file, line);)
            {
                std::string token = "";

                for (size_t i = 0; i < line.length(); i++)
                {
                    if ((not isTokenString) and (line[i] == ' '  || line[i] == '\t' || line[i] == '\r'))
                    {
                        if (token.length() > 0)
                        {
                            tokens.emplace(strToKeyword(token), token, lineNumber, columnNumber);
                        }
                        columnNumber+= token.length();

                        token = "";
                        isTokenNumber = false;

                        columnNumber++;
                        continue;
                    }

                    const char currentChara = line[i];

                    if ((i + 1) < line.length())
                        temp = charaToToken(currentChara, line[i + 1]);
                    else
                        temp = charaToToken(currentChara);

                    if (temp == TokenType::NOOP)
                    {
                        if (isTokenNumber)
                        {
                            if (not isNumber(std::string(1, currentChara)))
                                throw LexerException("Can't parse correct number: " + token, lineNumber, columnNumber);
                        }

                        token += currentChara;

                        if (isTokenString)
                            cNumberIfTokenIsString++;
                    }
                    else if (temp == TokenType::DMARK)
                    {
                        if (isTokenString)
                        {
                            tokens.emplace(TokenType::STRING, token, lNumberOfString, cNumberOfString);
                            token = "";
                            isTokenString = false;

                            columnNumber += cNumberIfTokenIsString;
                        }
                        else
                        {
                            if (token.length() > 0)
                            {
                                tokens.emplace(strToKeyword(token), token, lineNumber, columnNumber);
                            }

                            columnNumber += token.length();

                            token = "";

                            lNumberOfString = lineNumber;
                            cNumberOfString = columnNumber;

                            isTokenString = true;
                        }
                    }
                    //TODO check if temp is == to TokenType::POINT and current token parse is a number -> create a float
                    else
                    {
                        if (temp == TokenType::POINT)
                        {
                            if (token.length() != 0 && isNumber(token))
                            {
                                token += ".";
                                isTokenNumber = true;
                                goto skip;
                            }
                        }

                        if (temp == TokenType::BSLASH)
                        {
                            if ((i + 1) < line.length())
                            {
                                if (charaToToken(line[i + 1]) == TokenType::DMARK)
                                {
                                    token += "\"";
                                    columnNumber += 2;
                                    i++;
                                    goto skip;
                                }
                            }
                        }

                        if (isTokenString)
                        {
                            token += currentChara;

                            cNumberIfTokenIsString++;
                            goto skip;
                        }

                        if (temp == TokenType::SLASH)
                        {
                            if ((i + 1) < line.length())
                            {
                                if (charaToToken(line[i + 1]) == TokenType::SLASH)
                                    goto comment;
                            }
                        }

                        if (token.length() > 0)
                        {
                            tokens.emplace(strToKeyword(token), token, lineNumber, columnNumber);
                        }

                        columnNumber += token.length();

                        // 270 is the maximum code for single character op
                        if (static_cast<int>(temp) < 270)
                        {
                            tokens.emplace(temp, std::string(1, currentChara), lineNumber, columnNumber);
                            columnNumber++;
                        }
                        else // Else It is a two character op
                        {
                            tokens.emplace(temp, std::string{currentChara, line[i + 1]}, lineNumber, columnNumber);
                            columnNumber+= 2;
                            i++;
                        }

                        token = "";
                        isTokenNumber = false;

                        skip:;
                    }

                }

                comment:;

                if (not isTokenString)
                {
                    if (token.length() > 0)
                    {
                        tokens.emplace(strToKeyword(token), token, lineNumber, columnNumber);
                        columnNumber += token.length();
                    }

                    tokens.emplace(TokenType::EOL, "\n", lineNumber, columnNumber);
                }

                lineNumber++;
                columnNumber = 1;
                isTokenNumber = false;
            }

            if (isTokenString)
                throw LexerException("Expected \" at the end of a string declaration", lNumberOfString, cNumberOfString);

            tokens.emplace(TokenType::ENDOFFILE, "End of file", lineNumber, columnNumber);
        }
    }

    std::string LexerException::createErrorMessage(const std::string& message, int line, int column) const noexcept
    {
        return "Syntax Error: " + message + " at line " + std::to_string(line) + ", column " + std::to_string(column);
    }

    void Lexer::readFromText(const std::string& script)
    {
        parseToken(script, this->tokens);
    }

    void Lexer::readFromFile(const std::string& filename)
    {
        TextFile file = UniversalFileAccessor::openTextFile(filename);

        if (file.data == "")
            return;

        parseToken(file.data, this->tokens);
    }

}
