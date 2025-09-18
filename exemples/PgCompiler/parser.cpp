#include "parser.h"

#include "logger.h"
#include <unordered_map>

namespace pg
{
    void intNumber(Chunk& chunk, Parser& parser)
    {
        auto n = std::stoi(parser.previousToken.text);
        parser.writeConstant(chunk, n);
    }

    void floatNumber(Chunk& chunk, Parser& parser)
    {
        auto n = std::stof(parser.previousToken.text);
        parser.writeConstant(chunk, n);
    }

    void strLiterral(Chunk& chunk, Parser& parser)
    {
        auto str = parser.previousToken.text;
        parser.writeConstant(chunk, str);
    }

    void litteral(Chunk& chunk, Parser& parser)
    {
        switch (parser.previousToken.type)
        {
            case TokenType::KEYTRUE:
                parser.writeByte(chunk, OpCode::OP_True);
                break;
            case TokenType::KEYFALSE:
                parser.writeByte(chunk, OpCode::OP_False);
                break;
            default:
                return; // Unreachable
        }
    }

    void grouping(Chunk& chunk, Parser& parser)
    {
        parser.expression(chunk);
        parser.consume(TokenType::PCLOSE, "Expect ')' after expression.");
    }

    void unary(Chunk& chunk, Parser& parser)
    {
        Token operatorToken = parser.previousToken;

        parser.parsePrecedence(chunk, Precedence::UNARY);
        // parser.expression(chunk);

        // Negate should be applied to the value on top of the stack hence we do it after parsing the expression
        switch (operatorToken.type)
        {
            // We print the line of the token in case of an error (instead of the line of the expression)
            case TokenType::MINUS:
                chunk.addCode(OpCode::OP_Negate, operatorToken.line);
                break;
            // case TokenType::BANG: emitBytes(chunk, OpCode::OP_Not); break;
            default:
                return; // Unreachable
        }
    }

    void binary(Chunk& chunk, Parser& parser)
    {
        Token operatorToken = parser.previousToken;
        Precedence precedence = static_cast<Precedence>(static_cast<int>(parser.getRule(operatorToken.type).precedence) + 1);
        parser.parsePrecedence(chunk, precedence);

        switch (operatorToken.type)
        {
            case TokenType::PLUS:
                parser.writeByte(chunk, OpCode::OP_Add);
                break;
            case TokenType::MINUS:
                parser.writeByte(chunk, OpCode::OP_Subtract);
                break;
            case TokenType::STAR:
                parser.writeByte(chunk, OpCode::OP_Multiply);
                break;
            case TokenType::SLASH:
                parser.writeByte(chunk, OpCode::OP_Divide);
                break;
            default:
                return; // Unreachable
        }
    }

    std::unordered_map<TokenType, ParseRule> rules = {
        {TokenType::EQUAL,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::PLUS,         {NULL,        binary, Precedence::TERM}},
        {TokenType::MINUS,        {unary,       binary, Precedence::TERM}},
        {TokenType::STAR,         {NULL,        binary, Precedence::FACTOR}},
        {TokenType::MOD,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::POW,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::PENTER,       {grouping,    NULL,   Precedence::NONE}},
        {TokenType::PCLOSE,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::BENTER,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::BCLOSE,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::CENTER,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::CCLOSE,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SUP,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::INF,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::NOT,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::QMARK,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TILDE,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::AMPER,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::COMMA,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::POINT,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SMARK,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::DMARK,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SLASH,        {NULL,        binary, Precedence::FACTOR}},
        {TokenType::BSLASH,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SSLASH,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::HTAG,         {NULL,        NULL,   Precedence::NONE}},
        {TokenType::DPOINT,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::END,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::EOL,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::PLUSEQUAL,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::MINUSEQUAL,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::STAREQUAL,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::DIVIDEQUAL,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::MODEQUAL,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SUPEQUAL,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::INFEQUAL,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::INCREMENT,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::DECREMENT,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::LOGICAND,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::LOGICOR,      {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SHIFTLEFT,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SHIFTRIGHT,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::EQUALEQUAL,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::NOTEQUAL,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::ARROW,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SCOPE,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::ENDOFFILE,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::EXPRESSION,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::STRING,       {strLiterral, NULL,   Precedence::NONE}},
        {TokenType::NUMBER,       {intNumber,   NULL,   Precedence::NONE}},
        {TokenType::FLOAT,        {floatNumber, NULL,   Precedence::NONE}},
        {TokenType::KEYTRUE,      {litteral,    NULL,   Precedence::NONE}},
        {TokenType::KEYFALSE,     {litteral,    NULL,   Precedence::NONE}},
        {TokenType::NOOP,         {NULL,        NULL,   Precedence::NONE}},
        {TokenType::INVALID,      {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_CONST,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_INCLUDE,  {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_IF,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_ELSE,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_VAR,      {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_WHILE,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_FUN,      {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_RETURN,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_CLASS,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_THIS,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_FOR,      {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_IMPORT,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_FROM,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_AS,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_ERROR,    {NULL,        NULL,   Precedence::NONE}},
    };


    void Parser::parsePrecedence(Chunk& chunk, const Precedence& precedence)
    {
        advance();

        ParseFn prefixRule = getRule(previousToken.type).prefix;

        if (prefixRule == NULL)
        {
            errorAt(previousToken, "Expect expression.");
            return;
        }

        prefixRule(chunk, *this);

        while (precedence <= getRule(currentToken().type).precedence)
        {
            advance();
            ParseFn infixRule = getRule(previousToken.type).infix;
            infixRule(chunk, *this);
        }
    }

    ParseRule& Parser::getRule(const TokenType& type) const
    {
        return rules[type];
    }

    void Parser::writeConstant(Chunk& chunk, const ElementType& constant)
    {
        chunk.addConstant(constant, previousToken.line);
    }

    void Parser::writeByte(Chunk& chunk, const OpCode& byte)
    {
        chunk.addCode(byte, previousToken.line);
    }

    void Parser::writeByte(Chunk& chunk, uint8_t byte)
    {
        chunk.addCode(byte, previousToken.line);
    }

    void Parser::errorAt(const Token& token, const std::string& message)
    {
        if (panicMode)
            return;

        panicMode = true;

        LOG_ERROR("Parser", "Syntax Error: " + message + " at line " + std::to_string(token.line) + ", column " + std::to_string(token.column));

        hadError = true;
    }
}