#pragma once

#include <string>

namespace pg
{

    enum class TokenType : int
    {
        // Single character Operators

        EQUAL  = '=',
        PLUS   = '+',
        MINUS  = '-',
        STAR   = '*',
        MOD    = '%',
        POW    = '^',
        PENTER = '(',
        PCLOSE = ')',
        BENTER = '{',
        BCLOSE = '}',
        CENTER = '[',
        CCLOSE = ']',
        SUP    = '>',
        INF    = '<',
        NOT    = '!',
        QMARK  = '?',
        TILDE  = '~',
        AMPER  = '&',
        COMMA  = ',',
        POINT  = '.',
        SMARK  = '\'',
        DMARK  = '"',
        SLASH  = '/',
        BSLASH = '\\',
        SSLASH = '|',
        HTAG   = '#',
        DPOINT = ':',
        END    = ';',
        EOL    = '\n',

        //Two characters Operators

        PLUSEQUAL  = 270,
        MINUSEQUAL = 271,
        STAREQUAL  = 272,
        DIVIDEQUAL = 273,
        MODEQUAL   = 274,
        SUPEQUAL   = 275,
        INFEQUAL   = 276,
        INCREMENT  = 277,
        DECREMENT  = 278,

        LOGICAND   = 280,
        LOGICOR    = 281,
        SHIFTLEFT  = 282,
        SHIFTRIGHT = 283,
        EQUALEQUAL = 284,
        NOTEQUAL   = 285,

        ARROW      = 290,
        SCOPE      = 291,

        // Two characters Operators range from 270 through 299 the rest are reserved keywords

        // Keywords
        ENDOFFILE  = 300,

        EXPRESSION,
        STRING,
        NUMBER,
        FLOAT,
        KEYTRUE,
        KEYFALSE,
        NOOP,
        INVALID,
        
        TOK_CONST,
        TOK_INCLUDE,
        TOK_IF,
        TOK_ELSE,
        TOK_VAR,
        TOK_WHILE,
        TOK_FUN,
        TOK_RETURN,
        TOK_CLASS,
        TOK_THIS,
        TOK_FOR,
        TOK_IMPORT,
        TOK_FROM,
        TOK_AS

    };

    struct Token
    {
        Token(const TokenType& type, const std::string& text, unsigned line, unsigned column) : type(type), text(text), line(line), column(column) { }
        Token(const Token& other) : type(other.type), text(other.text), line(other.line), column(other.column) { }

        Token() : Token(TokenType::INVALID, "", 0, 0) { }

        void operator=(const Token& other) { type = other.type; text = other.text; line = other.line; column = other.column; }

        TokenType type;
        std::string text;
        unsigned line;
        unsigned column;
    };

}
