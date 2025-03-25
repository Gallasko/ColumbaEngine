#pragma once

#include <queue>
#include <stdexcept>

#include "token.h"
#include "expression.h"
#include "statement.h"

namespace pg
{

    class ParseException : public std::runtime_error
    {
    public: 
        ParseException(const Token& token, const std::string& message) noexcept : std::runtime_error(createErrorMessage(token, message)) {}
        virtual ~ParseException() = default;

        std::string createErrorMessage(const Token& token, const std::string& message) const noexcept;

    private:
        Token token;
        std::string message;
    };

    class Parser
    {
    public:
        Parser(const std::queue<Token>& tokenList) : tokenList(tokenList) {}

        std::queue<StatementPtr> parse();

        inline bool hasError() const noexcept { return errorEncountered; }

    private:
        inline const TokenType& peek() const { return tokenList.front().type; }

        inline bool isAtEnd() const { return peek() == TokenType::ENDOFFILE; }

        inline bool checkType(const TokenType& token) const
        {
            if (isAtEnd()) return false;
            
            return peek() == token;
        }

        const Token& advance()
        {
            if (not isAtEnd())
            {
                previousToken = tokenList.front();
                tokenList.pop();
            }

            return previousToken;
        }

        constexpr bool check() const { return false; }

        bool check(const TokenType& token) const { return checkType(token); }

        template <class... TT>
        bool check(const TokenType& token, const TT&... tokens)
        { 
            if (checkType(token))
                return true;

            return check(tokens...);
        }

        template <class... TT>
        bool match(const TT&... tokens)
        { 
            if (check(tokens...))
            {
                advance();
                return true;
            }

            return false;
        }

        void skipEOL()
        {
            while (match(TokenType::EOL));
        }

        template <class... TT>
        const Token& consume(const std::string& sErrMsg, const TT&... tokens)
        {
            if (check(tokens...))
                return advance();

            throw ParseException(tokenList.front(), sErrMsg);
        }

        void synchronize() 
        {
            do
            {
                advance();

                // Check if an end of line here is relevant for synchronize
                //if (previousToken.info.type == TokenType::END || previousToken.info.type == TokenType::EOL) return;
                if (previousToken.type == TokenType::END) return;

                switch (peek()) 
                {
                    case TokenType::TOK_CLASS:
                    case TokenType::TOK_FUN:
                    case TokenType::TOK_VAR:
                    case TokenType::TOK_FOR:
                    case TokenType::TOK_IF:
                    case TokenType::TOK_WHILE:
                    case TokenType::TOK_RETURN:
                        return;
                        break;

                    default:
                        break;
                }

            } while (not isAtEnd());
        }

        std::queue<StatementPtr> block();
        
        ExprPtr finishCall(ExprPtr caller);
        ExprPtr finishList();

        ExprPtr expression();
        ExprPtr assignment();
        ExprPtr logicOr();
        ExprPtr logicAnd();
        ExprPtr equality();
        ExprPtr comparison();
        ExprPtr term();
        ExprPtr factor();
        ExprPtr unary();
        ExprPtr call();
        ExprPtr primary();

        StatementPtr declaration();
        StatementPtr statement();

        std::shared_ptr<FunctionStatement> makeFun(const std::string& kind = "function");
        
        StatementPtr varDeclaration();
        StatementPtr funDeclaration(const std::string& kind = "function");
        StatementPtr classDeclaration();
        StatementPtr forStatement();
        StatementPtr ifStatement();
        StatementPtr whileStatement();
        StatementPtr returnStatement();    
        StatementPtr blockDeclaration();
        StatementPtr importStatement();
        StatementPtr expressionStatement();
        
        std::queue<Token> tokenList;
        Token previousToken;
        bool errorEncountered = false;
    };

}
