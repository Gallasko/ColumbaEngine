#pragma once

#include "Interpreter/token.h"

#include <queue>

#include "object.h"

#include "chunk.h"

namespace pg
{
    struct Compiler;

    enum class Precedence : uint8_t
    {
        NONE = 0,
        ASSIGNMENT = 1,  // =
        OR = 2,          // or
        AND = 3,         // and
        EQUALITY = 4,    // == !=
        COMPARISON = 5,  // < > <= >=
        TERM = 6,        // + -
        FACTOR = 7,      // * /
        UNARY = 8,       // ! -
        POSTFIX = 9,     // ++ --
        CALL = 10,       // . ()
        PRIMARY = 11
    };

    typedef void (*ParseFn)(struct Parser&, bool);

    struct ParseRule
    {
        ParseFn prefix;
        ParseFn infix;
        Precedence precedence;
    };

    struct Parser
    {
        Parser() {}

        void parse(std::queue<Token> tokenList) { tokens = tokenList; }

        void parsePrecedence(const Precedence& precedence);

        void advance()
        {
            previousToken = currentToken();

            if (tokens.empty())
                return;

            tokens.pop();
        }

        inline const TokenType& peek() const { return tokens.front().type; }

        inline bool isAtEnd() const { return peek() == TokenType::ENDOFFILE; }

        inline bool checkType(const TokenType& token) const
        {
            if (isAtEnd()) return false;

            return peek() == token;
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

        Token currentToken() const
        {
            if (tokens.empty())
                return Token(TokenType::TOK_ERROR, "", 0, 0);

            return tokens.front();
        }

        template <class... TT>
        void consume(const std::string& sErrMsg, const TT&... tokens)
        {
            if (check(tokens...))
            {
                advance();
                return;
            }

            errorAt(currentToken(), sErrMsg);
        }

        void consumeEnd(const std::string& sErrMsg)
        {
            consume(sErrMsg, TokenType::END, TokenType::EOL);
        }

        void expression()
        {
            parsePrecedence(Precedence::ASSIGNMENT);
        }

        void declaration();
        void varDeclaration();
        void funDeclaration();

        void statement();
        void expressionStatement();
        void blockStatement();
        void ifStatement();
        void whileStatement();
        void forStatement();
        void dprintStatement();
        void returnStatement();

        ParseRule& getRule(const TokenType& type) const;

        void parseFunction(const FunctionType& type);
        void declareVariable(const Token& name);

        int emitJump(const OpCode& instruction);
        void patchJump(int offset);

        void emitLoop(int loopStart);

        // Chunk modification functions
        void emitReturn() { writeConstant(0); writeByte(OpCode::OP_Return); }

        template <typename T, typename T2>
        void emitBytes(T byte1, T2 byte2)
        {
            writeByte(byte1);
            writeByte(byte2);
        }

        void writeConstant(ObjFunction* constant);
        void writeConstant(const ElementType& constant);
        void writeByte(const OpCode& byte);
        void writeByte(uint8_t byte);

        // Error handling
        void synchronize();

        bool hasError() const { return hadError; }

        void errorAt(const Token& token, const std::string& message);

        void error(const std::string& message) { errorAt(previousToken, message); }

        void reset()
        {
            hadError = false;
            panicMode = false;
            tokens = std::queue<Token>();
            previousToken = Token(TokenType::TOK_ERROR, "", 0, 0);
        }

        void setCompiler(Compiler* compiler) { this->compiler = compiler; }

        // Members
        Compiler* compiler = nullptr;

        bool hadError = false;
        bool panicMode = false;

        std::queue<Token> tokens;

        Token previousToken = Token(TokenType::TOK_ERROR, "", 0, 0);
    };
}