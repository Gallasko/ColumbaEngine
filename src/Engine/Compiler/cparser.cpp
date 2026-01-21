#include "stdafx.h"

#include "cparser.h"

#include "compiler.h"
#include "vm.h"

#include "Interpreter/lexer.h"
#include "logger.h"
#include "chunk_serializer.h"
#include <unordered_map>

namespace pg
{
    void intNumber(CParser& parser, bool)
    {
        auto n = std::stoi(parser.previousToken.text);
        parser.writeConstant(n);
    }

    void floatNumber(CParser& parser, bool)
    {
        auto n = std::stof(parser.previousToken.text);
        parser.writeConstant(n);
    }

    void strLiterral(CParser& parser, bool)
    {
        auto str = parser.previousToken.text;
        parser.writeConstant(str);
    }

    void litteral(CParser& parser, bool)
    {
        switch (parser.previousToken.type)
        {
            case TokenType::KEYTRUE:
                parser.writeByte(OpCode::OP_True);
                break;
            case TokenType::KEYFALSE:
                parser.writeByte(OpCode::OP_False);
                break;
            default:
                return; // Unreachable
        }
    }

    void grouping(CParser& parser, bool)
    {
        parser.expression();
        parser.consume("Expect ')' after expression.", TokenType::PCLOSE);
    }

    void anonymousFunction(CParser& parser, bool)
    {
        // Parse an anonymous function expression
        // Syntax: fun(params) { body }
        parser.parseFunction(FunctionType::TYPE_FUNCTION);
    }

    void unary(CParser& parser, bool)
    {
        Token operatorToken = parser.previousToken;

        parser.parsePrecedence(Precedence::UNARY);
        // parser.expression(chunk);

        auto& chunk = Compiler::current->getCurrentChunk();

        // Negate should be applied to the value on top of the stack hence we do it after parsing the expression
        switch (operatorToken.type)
        {
            // We print the line of the token in case of an error (instead of the line of the expression)
            case TokenType::MINUS:
                chunk.addCode(OpCode::OP_Negate, operatorToken.line);
                break;
            case TokenType::NOT:
                chunk.addCode(OpCode::OP_Not, operatorToken.line);
                break;
            default:
                return; // Unreachable
        }
    }

    void binary(CParser& parser, bool)
    {
        Token operatorToken = parser.previousToken;
        Precedence precedence = static_cast<Precedence>(static_cast<int>(parser.getRule(operatorToken.type).precedence) + 1);
        parser.parsePrecedence(precedence);

        switch (operatorToken.type)
        {
            case TokenType::PLUS:
                parser.writeByte(OpCode::OP_Add);
                break;
            case TokenType::MINUS:
                parser.writeByte(OpCode::OP_Subtract);
                break;
            case TokenType::STAR:
                parser.writeByte(OpCode::OP_Multiply);
                break;
            case TokenType::SLASH:
                parser.writeByte(OpCode::OP_Divide);
                break;
            case TokenType::MOD:
                parser.writeByte(OpCode::OP_Modulo);
                break;
            case TokenType::LOGICAND:
                parser.writeByte(OpCode::OP_And);
                break;
            case TokenType::LOGICOR:
                parser.writeByte(OpCode::OP_Or);
                break;
            case TokenType::EQUALEQUAL:
                parser.writeByte(OpCode::OP_Equal);
                break;
            case TokenType::NOTEQUAL:
                parser.writeByte(OpCode::OP_NotEqual);
                break;
            case TokenType::INF:
                parser.writeByte(OpCode::OP_Less);
                break;
            case TokenType::INFEQUAL:
                parser.writeByte(OpCode::OP_LessEqual);
                break;
            case TokenType::SUP:
                parser.writeByte(OpCode::OP_Greater);
                break;
            case TokenType::SUPEQUAL:
                parser.writeByte(OpCode::OP_GreaterEqual);
                break;
            default:
                return; // Unreachable
        }
    }

    void variable(CParser& parser, bool canAssign)
    {
        auto varName = parser.previousToken.text;
        Token varToken = parser.previousToken;
        int arg = Compiler::current->resolveLocal(varToken);

        OpCode setOp, getOp;

        if (arg != -1)
        {
            setOp = OpCode::OP_Set_Local;
            getOp = OpCode::OP_Get_Local;
        }
        else if ((arg = Compiler::current->resolveUpvalue(varToken)) != -1)
        {
            setOp = OpCode::OP_Set_Upvalue;
            getOp = OpCode::OP_Get_Upvalue;
        }
        else
        {
            // Global variable - use constant pool
            if (canAssign and parser.match(TokenType::EQUAL))
            {
                parser.expression();
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Set_Global);
            }
            else if (canAssign and parser.match(TokenType::PLUSEQUAL))
            {
                // var += expr => var = var + expr
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Get_Global);  // Get current value
                parser.expression();                       // Evaluate right side
                parser.writeByte(OpCode::OP_Add);          // Add them
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Set_Global);  // Store result
            }
            else if (canAssign and parser.match(TokenType::MINUSEQUAL))
            {
                // var -= expr => var = var - expr
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Get_Global);  // Get current value
                parser.expression();                       // Evaluate right side
                parser.writeByte(OpCode::OP_Subtract);     // Subtract
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Set_Global);  // Store result
            }
            else if (parser.match(TokenType::INCREMENT))
            {
                // Postfix increment: var++
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Get_Global);
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Post_Incr_Global);
            }
            else if (parser.match(TokenType::DECREMENT))
            {
                // Postfix decrement: var--
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Get_Global);
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Post_Decr_Global);
            }
            else
            {
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Get_Global);
            }

            return;
        }

        // Local variable - use immediate operand
        if (canAssign and parser.match(TokenType::EQUAL))
        {
            parser.expression();
            parser.writeByte(setOp);
            parser.writeByte(static_cast<uint8_t>(arg));
        }
        else if (canAssign and parser.match(TokenType::PLUSEQUAL))
        {
            // var += expr => var = var + expr
            parser.writeByte(getOp);
            parser.writeByte(static_cast<uint8_t>(arg));  // Get current value
            parser.expression();                           // Evaluate right side
            parser.writeByte(OpCode::OP_Add);              // Add them
            parser.writeByte(setOp);
            parser.writeByte(static_cast<uint8_t>(arg));  // Store result
        }
        else if (canAssign and parser.match(TokenType::MINUSEQUAL))
        {
            // var -= expr => var = var - expr
            parser.writeByte(getOp);
            parser.writeByte(static_cast<uint8_t>(arg));  // Get current value
            parser.expression();                           // Evaluate right side
            parser.writeByte(OpCode::OP_Subtract);         // Subtract
            parser.writeByte(setOp);
            parser.writeByte(static_cast<uint8_t>(arg));  // Store result
        }
        else if (parser.match(TokenType::INCREMENT))
        {
            // Postfix increment: var++
            parser.writeByte(getOp);
            parser.writeByte(static_cast<uint8_t>(arg));
            parser.writeConstant(ElementType(arg));
            parser.writeByte(OpCode::OP_Post_Incr_Local);
        }
        else if (parser.match(TokenType::DECREMENT))
        {
            // Postfix decrement: var--
            parser.writeByte(getOp);
            parser.writeByte(static_cast<uint8_t>(arg));
            parser.writeConstant(ElementType(arg));
            parser.writeByte(OpCode::OP_Post_Decr_Local);
        }
        else
        {
            parser.writeByte(getOp);
            parser.writeByte(static_cast<uint8_t>(arg));
        }
    }

    void andOp(CParser& parser, bool)
    {
        // For 'and': if the left operand is false, short-circuit to false
        // If left operand is true, evaluate right operand
        int endJump = parser.emitJump(OpCode::OP_Long_Jump_If_False);

        parser.writeByte(OpCode::OP_Pop);
        parser.parsePrecedence(Precedence::AND);

        parser.patchJump(endJump);
    }

    void orOp(CParser& parser, bool)
    {
        // For 'or': if the left operand is false, jump to evaluate right operand
        // If left operand is true, short-circuit to true
        int elseJump = parser.emitJump(OpCode::OP_Long_Jump_If_False);
        int endJump = parser.emitJump(OpCode::OP_Long_Jump);

        parser.patchJump(elseJump);
        parser.writeByte(OpCode::OP_Pop);

        parser.parsePrecedence(Precedence::OR);
        parser.patchJump(endJump);
    }

    void decrementOp(CParser& parser, bool)
    {
        Token operatorToken = parser.previousToken;

        // Check if next token is a variable (expression/identifier)
        if (parser.check(TokenType::EXPRESSION))
        {
            // Prefix decrement: --var
            parser.advance(); // consume the identifier
            Token varToken = parser.previousToken;
            auto varName = varToken.text;

            OpCode incrOp;
            ElementType identifier;
            int arg = Compiler::current->resolveLocal(varToken);

            if (arg != -1)
            {
                // Local variable
                incrOp = OpCode::OP_Decr_Local;
                identifier = ElementType(arg);
            }
            else
            {
                // Global variable
                incrOp = OpCode::OP_Decr_Global;
                identifier = ElementType(varName);
            }

            // Prefix decrement: --var (modify variable, return new value)
            parser.writeConstant(identifier);
            parser.writeByte(incrOp);
        }
        else
        {
            // Parse the operand at unary precedence (for literals like --5)
            parser.parsePrecedence(Precedence::UNARY);

            auto& chunk = Compiler::current->getCurrentChunk();

            // Treat DECREMENT as double unary minus (--5 becomes -(-5) = 5)
            chunk.addCode(OpCode::OP_Negate, operatorToken.line);
            chunk.addCode(OpCode::OP_Negate, operatorToken.line);
        }
    }

    void incrementOp(CParser& parser, bool)
    {
        Token operatorToken = parser.previousToken;

        // Check if next token is a variable (expression/identifier)
        if (parser.check(TokenType::EXPRESSION))
        {
            // Prefix increment: ++var
            parser.advance(); // consume the identifier
            Token varToken = parser.previousToken;
            auto varName = varToken.text;

            OpCode incrOp;
            ElementType identifier;
            int arg = Compiler::current->resolveLocal(varToken);

            if (arg != -1)
            {
                // Local variable
                incrOp = OpCode::OP_Incr_Local;
                identifier = ElementType(arg);
            }
            else
            {
                // Global variable
                incrOp = OpCode::OP_Incr_Global;
                identifier = ElementType(varName);
            }

            parser.writeConstant(identifier); // Push identifier [id]
            parser.writeByte(incrOp);         // IncrementIt
        }
        else
        {
            // Parse the operand at unary precedence (for literals like ++5)
            parser.parsePrecedence(Precedence::UNARY);

            // For literals, ++5 doesn't make much sense, but we can treat it as +(+5) = 5
            // This is a no-op for numbers
        }
    }

    void postfixIncrementOp(CParser& parser, bool)
    {
        // For postfix increment: var++ (return old value, modify variable)
        // Stack currently has [old_value] from the variable access
        // We need to: return old_value, but also increment the variable

        // Strategy: Examine the last bytecode instruction to determine variable type
        // Local vars: OP_Get_Local <slot_byte>
        // Global vars: OP_Constant <name_idx>, OP_Get_Global

        auto& chunk = Compiler::current->getCurrentChunk();

        if (chunk.code.size() < 2)
        {
            parser.errorAt(parser.previousToken, "No variable found for postfix increment");
            return;
        }

        // Check the last instruction (last byte in the code)
        OpCode lastOp = static_cast<OpCode>(chunk.code.back());

        OpCode incrOp;
        ElementType identifier;

        if (lastOp == OpCode::OP_Get_Local)
        {
            // Local variable - the slot number is the second-to-last byte
            uint8_t slot = chunk.code[chunk.code.size() - 2];
            incrOp = OpCode::OP_Post_Incr_Local;
            identifier = ElementType(static_cast<int>(slot));
        }
        else if (lastOp == OpCode::OP_Get_Global)
        {
            // Global variable - the name is in the last constant
            if (chunk.constants.empty())
            {
                parser.errorAt(parser.previousToken, "No constant found for global variable");
                return;
            }
            ElementType lastConstant = parser.vm->valueToElement(chunk.constants.back());
            incrOp = OpCode::OP_Post_Incr_Global;
            identifier = lastConstant;
        }
        else
        {
            parser.errorAt(parser.previousToken, "Postfix increment must follow a variable access");
            return;
        }

        parser.writeConstant(identifier);      // [old_value, id]
        parser.writeByte(incrOp);
    }

    void postfixDecrementOp(CParser& parser, bool)
    {
        // For postfix decrement: var-- (return old value, modify variable)
        // Stack currently has [old_value] from the variable access
        // We need to: return old_value, but also decrement the variable

        auto& chunk = Compiler::current->getCurrentChunk();

        if (chunk.code.size() < 2)
        {
            parser.errorAt(parser.previousToken, "No variable found for postfix decrement");
            return;
        }

        // Check the last instruction (last byte in the code)
        OpCode lastOp = static_cast<OpCode>(chunk.code.back());

        OpCode decrOp;
        ElementType identifier;

        if (lastOp == OpCode::OP_Get_Local)
        {
            // Local variable - the slot number is the second-to-last byte
            uint8_t slot = chunk.code[chunk.code.size() - 2];
            decrOp = OpCode::OP_Post_Decr_Local;
            identifier = ElementType(static_cast<int>(slot));
        }
        else if (lastOp == OpCode::OP_Get_Global)
        {
            // Global variable - the name is in the last constant
            if (chunk.constants.empty())
            {
                parser.errorAt(parser.previousToken, "No constant found for global variable");
                return;
            }
            ElementType lastConstant = parser.vm->valueToElement(chunk.constants.back());
            decrOp = OpCode::OP_Post_Decr_Global;
            identifier = lastConstant;
        }
        else
        {
            parser.errorAt(parser.previousToken, "Postfix decrement must follow a variable access");
            return;
        }

        parser.writeConstant(identifier);
        parser.writeByte(decrOp);
    }

    uint8_t argumentList(CParser& parser)
    {
        uint8_t argCount = 0;

        if (not parser.check(TokenType::PCLOSE))
        {
            do
            {
                if (argCount == 255)
                {
                    parser.errorAt(parser.previousToken, "Can't have more than 255 arguments.");
                }

                parser.skipEOL();
                parser.expression();
                argCount++;
            } while (parser.match(TokenType::COMMA));
        }

        parser.consume("Expect ')' after arguments.", TokenType::PCLOSE);

        return argCount;
    }

    void call(CParser& parser, bool)
    {
        auto argCount = argumentList(parser);

        parser.emitBytes(OpCode::OP_Call, argCount);
    }

    void dot(CParser& parser, bool canAssign)
    {
        parser.consume("Expect property name after '.'.", TokenType::EXPRESSION);

        auto propertyName = parser.previousToken.text;
        auto value = parser.vm->createString(propertyName); // Ensure string is created in VM
        uint8_t constantIndex = Compiler::current->getCurrentChunk().addConstantIndex(value);

        if (canAssign and parser.match(TokenType::EQUAL))
        {
            parser.expression();
            parser.writeByte(OpCode::OP_Set_Property);
            parser.writeByte(constantIndex);
        }
        else if (parser.match(TokenType::PENTER))
        {
            // Method call
            auto argCount = argumentList(parser);
            parser.emitBytes(OpCode::OP_Invoke, constantIndex);
            parser.writeByte(argCount);
        }
        else
        {
            parser.writeByte(OpCode::OP_Get_Property);
            parser.writeByte(constantIndex);
        }
        // TODO: Add support for obj.prop += expr and obj.prop -= expr
        // This requires either a DUP opcode or re-evaluating the left side
    }

    void this_(CParser& parser, bool)
    {
        if (Compiler::current->currentClass == nullptr)
        {
            parser.errorAt(parser.previousToken, "Can't use 'this' outside of a class.");
            return;
        }

        variable(parser, false);
    }

    void createTable(CParser& parser, bool)
    {
        uint8_t autoIndex = 0;

        bool buildTable = false;

        if (not parser.check(TokenType::CCLOSE))
        {
            do
            {
                parser.skipEOL();

                if (parser.check(TokenType::CCLOSE)) break; // trailing comma

                Token first = parser.currentToken();
                parser.advance(); // now 'previousToken' == first

                if (parser.match(TokenType::DPOINT))
                {
                    // Parse the value expression normally
                    parser.expression(); // value
                    parser.writeConstant(first.text);
                    buildTable = true;
                }
                else
                {
                    parser.parsePrecedenceFromPrev(Precedence::ASSIGNMENT);
                    parser.writeConstant(autoIndex); // Implicit key
                }

                autoIndex++;

                parser.skipEOL();
            } while (parser.match(TokenType::COMMA));
        }

        parser.skipEOL();
        parser.consume("Expect ']' after table values.", TokenType::CCLOSE);

        if (buildTable)
            parser.writeByte(OpCode::OP_Build_Table);
        else
            parser.writeByte(OpCode::OP_Build_Vector);
        parser.writeByte(autoIndex);
    }

    void createTableBrace(CParser& parser, bool)
    {
        uint8_t autoIndex = 0;

        if (not parser.check(TokenType::BCLOSE))
        {
            do
            {
                parser.skipEOL();

                if (parser.check(TokenType::BCLOSE)) break; // trailing comma

                Token first = parser.currentToken();
                parser.advance(); // now 'previousToken' == first

                if (parser.match(TokenType::DPOINT))
                {
                    // Parse the value expression normally
                    parser.expression(); // value
                    parser.writeConstant(first.text);
                }
                else
                {
                    parser.parsePrecedenceFromPrev(Precedence::ASSIGNMENT);
                    parser.writeConstant(autoIndex); // Implicit key
                }

                autoIndex++;

                parser.skipEOL();
            } while (parser.match(TokenType::COMMA));
        }

        parser.skipEOL();
        parser.consume("Expect '}' after table values.", TokenType::BCLOSE);

        parser.writeByte(OpCode::OP_Build_Table);
        parser.writeByte(autoIndex);
    }

    void indexTable(CParser& parser, bool canAssign)
    {
        parser.expression(); // Index expression
        parser.consume("Expect ']' after index expression.", TokenType::CCLOSE);

        if (canAssign and parser.match(TokenType::EQUAL))
        {
            parser.expression(); // Value to assign
            parser.writeByte(OpCode::OP_Set_Index);
        }
        else
        {
            parser.writeByte(OpCode::OP_Get_Index);
        }
    }

    std::unordered_map<TokenType, ParseRule> rules = {
        {TokenType::EQUAL,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::PLUS,         {NULL,        binary,     Precedence::TERM}},
        {TokenType::MINUS,        {unary,       binary,     Precedence::TERM}},
        {TokenType::STAR,         {NULL,        binary,     Precedence::FACTOR}},
        {TokenType::SLASH,        {NULL,        binary,     Precedence::FACTOR}},
        {TokenType::MOD,          {NULL,        binary,     Precedence::FACTOR}},
        {TokenType::POW,          {NULL,        NULL,       Precedence::NONE}},
        {TokenType::PENTER,       {grouping,    call,       Precedence::CALL}},
        {TokenType::PCLOSE,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::BENTER,       {createTableBrace, NULL,  Precedence::NONE}},
        {TokenType::BCLOSE,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::CENTER,       {createTable, indexTable, Precedence::CALL}},
        {TokenType::CCLOSE,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::SUP,          {NULL,        binary,     Precedence::COMPARISON}},
        {TokenType::INF,          {NULL,        binary,     Precedence::COMPARISON}},
        {TokenType::NOT,          {unary,       NULL,       Precedence::NONE}},
        {TokenType::QMARK,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TILDE,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::AMPER,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::COMMA,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::POINT,        {NULL,        dot,        Precedence::CALL}},
        {TokenType::SMARK,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::DMARK,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::BSLASH,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::SSLASH,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::HTAG,         {NULL,        NULL,       Precedence::NONE}},
        {TokenType::DPOINT,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::END,          {NULL,        NULL,       Precedence::NONE}},
        {TokenType::EOL,          {NULL,        NULL,       Precedence::NONE}},
        {TokenType::PLUSEQUAL,    {NULL,        NULL,       Precedence::NONE}},
        {TokenType::MINUSEQUAL,   {NULL,        NULL,       Precedence::NONE}},
        {TokenType::STAREQUAL,    {NULL,        NULL,       Precedence::NONE}},
        {TokenType::DIVIDEQUAL,   {NULL,        NULL,       Precedence::NONE}},
        {TokenType::MODEQUAL,     {NULL,        NULL,       Precedence::NONE}},
        {TokenType::SUPEQUAL,     {NULL,        binary,     Precedence::COMPARISON}},
        {TokenType::INFEQUAL,     {NULL,        binary,     Precedence::COMPARISON}},
        {TokenType::INCREMENT,    {incrementOp, NULL,       Precedence::NONE}},
        {TokenType::DECREMENT,    {decrementOp, NULL,       Precedence::NONE}},
        {TokenType::LOGICAND,     {NULL,        andOp,      Precedence::AND}},
        {TokenType::LOGICOR,      {NULL,        orOp,       Precedence::OR}},
        {TokenType::SHIFTLEFT,    {NULL,        NULL,       Precedence::NONE}},
        {TokenType::SHIFTRIGHT,   {NULL,        NULL,       Precedence::NONE}},
        {TokenType::EQUALEQUAL,   {NULL,        binary,     Precedence::EQUALITY}},
        {TokenType::NOTEQUAL,     {NULL,        binary,     Precedence::EQUALITY}},
        {TokenType::ARROW,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::SCOPE,        {NULL,        NULL,       Precedence::NONE}},
        {TokenType::ENDOFFILE,    {NULL,        NULL,       Precedence::NONE}},
        {TokenType::EXPRESSION,   {variable,    NULL,       Precedence::NONE}},
        {TokenType::STRING,       {strLiterral, NULL,       Precedence::NONE}},
        {TokenType::NUMBER,       {intNumber,   NULL,       Precedence::NONE}},
        {TokenType::FLOAT,        {floatNumber, NULL,       Precedence::NONE}},
        {TokenType::KEYTRUE,      {litteral,    NULL,       Precedence::NONE}},
        {TokenType::KEYFALSE,     {litteral,    NULL,       Precedence::NONE}},
        {TokenType::NOOP,         {NULL,        NULL,       Precedence::NONE}},
        {TokenType::INVALID,      {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_CONST,    {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_INCLUDE,  {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_IF,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_ELSE,     {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_VAR,      {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_WHILE,    {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_FUN,      {anonymousFunction, NULL,       Precedence::NONE}},
        {TokenType::TOK_RETURN,   {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_CLASS,    {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_THIS,     {this_,       NULL,       Precedence::NONE}},
        {TokenType::TOK_FOR,      {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_IMPORT,   {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_FROM,     {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_AS,       {NULL,        NULL,       Precedence::NONE}},
        {TokenType::TOK_ERROR,    {NULL,        NULL,       Precedence::NONE}},
    };

    CParser::~CParser()
    {
        for (auto func : allocatedFunction)
        {
            vm->pools.functionPool.release(vm->asFunction(func));
        }
    }

    void CParser::parsePrecedenceFromPrev(const Precedence& precedence)
    {
        ParseFn prefixRule = getRule(previousToken.type).prefix;

        if (prefixRule == NULL)
        {
            errorAt(previousToken, "Expect expression.");
            return;
        }

        bool canAssign = precedence <= Precedence::ASSIGNMENT;

        prefixRule(*this, canAssign);

        while (precedence <= getRule(currentToken().type).precedence)
        {
            advance();
            ParseFn infixRule = getRule(previousToken.type).infix;
            infixRule(*this, canAssign);
        }

        if (canAssign and match(TokenType::EQUAL))
        {
            errorAt(previousToken, "Invalid assignment target.");
        }
    }

    void CParser::parsePrecedence(const Precedence& precedence)
    {
        advance();

        parsePrecedenceFromPrev(precedence);
    }

    void CParser::declaration()
    {
        if (match(TokenType::TOK_VAR))
        {
            varDeclaration();
        }
        else if (match(TokenType::TOK_FUN))
        {
            funDeclaration();
        }
        else if (match(TokenType::TOK_CLASS))
        {
            classDeclaration();
        }
        else
        {
            statement();
        }

        skipEOL();

        if (panicMode)
            synchronize();
    }

    void CParser::varDeclaration()
    {
        consume("Expect variable name.", TokenType::EXPRESSION);

        Token varName = previousToken;

        if (Compiler::current->scopeDepth > 0)
        {
            declareVariable(varName);
        }

        if (match(TokenType::EQUAL))
        {
            expression();
        }
        else
        {
            writeConstant(ElementType()); // Default initialize to 0
        }

        consumeEnd("Expect end of variable declaration.");

        if (Compiler::current->scopeDepth > 0)
        {
            Compiler::current->markInitialized();
            return;
        }

        writeConstant(varName.text);  // Push variable name onto stack
        writeByte(OpCode::OP_Define_Global);
    }

    void CParser::funDeclaration()
    {
        consume("Expect variable name.", TokenType::EXPRESSION);

        Token varName = previousToken;

        if (Compiler::current->scopeDepth > 0)
        {
            declareVariable(varName);
        }

        Compiler::current->markInitialized();

        parseFunction(FunctionType::TYPE_FUNCTION);

        if (Compiler::current->scopeDepth == 0)
        {
            writeConstant(varName.text);  // Push variable name onto stack
            writeByte(OpCode::OP_Define_Global);
        }
    }

    void CParser::classDeclaration()
    {
        consume("Expect class name.", TokenType::EXPRESSION);
        Token className = previousToken;

        // Emit OP_Class with constant index as operand (not OP_Constant before it)
        auto value = vm->createString(className.text); // Ensure string is created in VM
        uint8_t nameConstant = Compiler::current->getCurrentChunk().addConstantIndex(value);
        emitBytes(OpCode::OP_Class, nameConstant);

        // Compiler::current->beginScope();

        if (Compiler::current->scopeDepth > 0)
        {
            declareVariable(className);
            Compiler::current->markInitialized();
        }
        else
        {
            // Define the class as a global variable
            writeConstant(className.text);  // Push variable name onto stack
            writeByte(OpCode::OP_Define_Global);
        }

        Compiler::ClassCompiler classCompiler;
        classCompiler.enclosing = Compiler::current->currentClass;

        Compiler::current->currentClass = &classCompiler;

        pushVariableInStack(className.text);

        skipEOL();
        consume("Expect '{' before class body.", TokenType::BENTER);
        skipEOL();

        while (not check(TokenType::BCLOSE) and not isAtEnd())
        {
            methodStatement();
            skipEOL();
        }

        skipEOL();
        consume("Expect '}' after class body.", TokenType::BCLOSE);

        writeByte(OpCode::OP_Pop);

        Compiler::current->currentClass = classCompiler.enclosing;
    }

    void CParser::statement()
    {
        if (match(TokenType::BENTER))
        {
            Compiler::current->beginScope();
            blockStatement();
            Compiler::current->endScope();
        }
        else if (match(TokenType::TOK_IF))
        {
            ifStatement();
        }
        else if (match(TokenType::TOK_WHILE))
        {
            whileStatement();
        }
        else if (match(TokenType::TOK_FOR))
        {
            forStatement();
        }
        else if (match(TokenType::TOK_DPRINT))
        {
            dprintStatement();
        }
        else if (match(TokenType::TOK_RETURN))
        {
            returnStatement();
        }
        else if (match(TokenType::TOK_BREAK))
        {
            breakStatement();
        }
        else if (match(TokenType::TOK_CONTINUE))
        {
            continueStatement();
        }
        else if (match(TokenType::TOK_IMPORT))
        {
            importStatement();
        }
        else
        {
            expressionStatement();
        }
    }

    void CParser::expressionStatement()
    {
        expression();
        consumeEnd("Expect end of expression.");
        writeByte(OpCode::OP_Pop);
    }

    void CParser::blockStatement()
    {
        while (not check(TokenType::BCLOSE) and not isAtEnd())
        {
            skipEOL();
            declaration();
        }

        consume("Expect '}' after block.", TokenType::BCLOSE);
    }

    void CParser::ifStatement()
    {
        skipEOL();
        consume("Expect '(' after 'if'.", TokenType::PENTER);
        skipEOL();

        expression();

        consume("Expect ')' after condition.", TokenType::PCLOSE);
        skipEOL();

        int thenJump = emitJump(OpCode::OP_Long_Jump_If_False);
        writeByte(OpCode::OP_Pop); // Pop the condition

        statement();

        int elseJump = emitJump(OpCode::OP_Long_Jump);

        patchJump(thenJump);
        writeByte(OpCode::OP_Pop); // Pop the condition

        skipEOL();
        if (match(TokenType::TOK_ELSE))
        {
            skipEOL();
            statement();
        }

        patchJump(elseJump);
    }

    void CParser::dprintStatement()
    {
        skipEOL();
        consume("Expect '(' after '__dprint'.", TokenType::PENTER);
        skipEOL();

        expression();

        consume("Expect ')' after expression.", TokenType::PCLOSE);
        consumeEnd("Expect ';' or newline after '__dprint' statement.");

        writeByte(OpCode::OP_Debug_Print);
    }

    void CParser::whileStatement()
    {
        int loopStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

        skipEOL();
        consume("Expect '(' after 'while'.", TokenType::PENTER);
        skipEOL();

        expression();

        consume("Expect ')' after condition.", TokenType::PCLOSE);
        skipEOL();

        int exitJump = emitJump(OpCode::OP_Long_Jump_If_False);
        writeByte(OpCode::OP_Pop); // Pop the condition

        statement();
        emitLoop(loopStart);

        patchJump(exitJump);
        writeByte(OpCode::OP_Pop); // Pop the condition
    }

    void CParser::forStatement()
    {
        Compiler::current->beginScope();

        skipEOL();
        consume("Expect '(' after 'for'.", TokenType::PENTER);
        skipEOL();

        // Check for for-in loop: for (var key : table)
        if (match(TokenType::TOK_VAR))
        {
            // Get the variable name
            consume("Expect variable name after 'var'.", TokenType::EXPRESSION);
            Token varToken = previousToken;

            skipEOL();

            // Check if this is a for-in loop
            if (match(TokenType::DPOINT)) // : token
            {
                // This is a for-in loop: for (var key : table)
                skipEOL();

                // Parse the table expression
                expression();

                consume("Expect ')' after for-in expression.", TokenType::PCLOSE);
                skipEOL();

                // Desugar for-in loop to:
                // {
                //     var __table = table;
                //     var __size = __table.size();
                //     for (var __i = 0; __i < __size; __i++) {
                //         var key = __table.at(__i);
                //         body;
                //     }
                // }

                // Stack at this point: [table]

                // 1. Store table as hidden local __table
                Compiler::current->addLocal(Token(TokenType::EXPRESSION, "__table", currentToken().line, 0));
                Compiler::current->markInitialized();
                uint8_t tableSlot = static_cast<uint8_t>(Compiler::current->localCount - 1);
                // Stack: [__table]

                // 2. Get size of table and store as hidden local __size
                writeByte(OpCode::OP_Get_Local);
                writeByte(tableSlot);
                writeByte(OpCode::OP_Table_Size);
                // Stack: [__table, size]

                Compiler::current->addLocal(Token(TokenType::EXPRESSION, "__size", currentToken().line, 0));
                Compiler::current->markInitialized();
                uint8_t sizeSlot = static_cast<uint8_t>(Compiler::current->localCount - 1);
                // Stack: [__table, __size]

                // 3. Initialize loop counter __i = 0
                writeByte(OpCode::OP_Constant);
                writeByte(Compiler::current->getCurrentChunk().addConstantIndex(makeIntValue(0)));
                // Stack: [__table, __size, 0]

                Compiler::current->addLocal(Token(TokenType::EXPRESSION, "__i", currentToken().line, 0));
                Compiler::current->markInitialized();
                uint8_t counterSlot = static_cast<uint8_t>(Compiler::current->localCount - 1);
                // Stack: [__table, __size, __i]

                // 4. Loop condition: __i < __size
                int loopStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

                // Get __i
                writeByte(OpCode::OP_Get_Local);
                writeByte(counterSlot);
                // Get __size
                writeByte(OpCode::OP_Get_Local);
                writeByte(sizeSlot);
                // Compare: __i < __size
                writeByte(OpCode::OP_Less);
                // Stack: [__table, __size, __i, bool]

                int exitJump = emitJump(OpCode::OP_Long_Jump_If_False);
                writeByte(OpCode::OP_Pop); // Pop the condition result
                // Stack: [__table, __size, __i]

                // 5. Begin iteration scope for the key variable
                Compiler::current->beginScope();

                // Get key at current index: var key = __table.at(__i)
                // Get __table
                writeByte(OpCode::OP_Get_Local);
                writeByte(tableSlot);
                // Get __i
                writeByte(OpCode::OP_Get_Local);
                writeByte(counterSlot);
                // Call __table.at(__i)
                writeByte(OpCode::OP_Table_At);
                // Stack: [__table, __size, __i, key]

                // Declare the loop variable with the key
                Compiler::current->addLocal(varToken);
                Compiler::current->markInitialized();
                // Stack: [__table, __size, __i, key]

                // 6. Execute loop body
                statement();

                // 7. End iteration scope - this pops the key variable
                Compiler::current->endScope();
                // Stack: [__table, __size, __i]

                // 8. Increment __i: __i++
                writeByte(OpCode::OP_Get_Local);
                writeByte(counterSlot);
                writeByte(OpCode::OP_Constant);
                writeByte(Compiler::current->getCurrentChunk().addConstantIndex(makeIntValue(1)));
                writeByte(OpCode::OP_Add);
                writeByte(OpCode::OP_Set_Local);
                writeByte(counterSlot);
                writeByte(OpCode::OP_Pop); // Pop the assignment result
                // Stack: [__table, __size, __i]

                // 9. Loop back to condition check
                emitLoop(loopStart);

                // 10. Exit point
                patchJump(exitJump);
                writeByte(OpCode::OP_Pop); // Pop the condition result
                // Stack: [__table, __size, __i]

                // 11. End scope - pops __i, __size, __table (and key if it's still around)
                Compiler::current->endScope();
                return;
            }
            else
            {
                // Regular for loop with initializer
                // var was already consumed, continue with variable declaration
                Compiler::current->addLocal(varToken);

                if (match(TokenType::EQUAL))
                {
                    expression();
                }
                else
                {
                    writeByte(OpCode::OP_False); // Default initialize to false
                }

                Compiler::current->markInitialized();
                consume("Expect ';' after variable declaration.", TokenType::END);
            }
        }
        else if (match(TokenType::END))
        {
            // No initializer
        }
        else
        {
            expressionStatement();
        }

        skipEOL();

        int loopStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

        // Condition
        int exitJump = -1;
        if (not match(TokenType::END))
        {
            expression();
            consume("Expect ';' after condition.", TokenType::END);

            exitJump = emitJump(OpCode::OP_Long_Jump_If_False);
            writeByte(OpCode::OP_Pop); // Pop the condition
        }

        skipEOL();

        // Increment
        if (not match(TokenType::PCLOSE))
        {
            int bodyJump = emitJump(OpCode::OP_Long_Jump);
            int incrementStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

            expression();
            writeByte(OpCode::OP_Pop); // Pop the increment expression result

            consume("Expect ')' after for clauses.", TokenType::PCLOSE);

            emitLoop(loopStart);
            loopStart = incrementStart;

            patchJump(bodyJump);
        }

        skipEOL();

        // Body
        statement();
        emitLoop(loopStart);

        if (exitJump != -1)
        {
            patchJump(exitJump);
            writeByte(OpCode::OP_Pop); // Pop the condition
        }

        Compiler::current->endScope();
    }

    void CParser::breakStatement()
    {
        consumeEnd("Expect end of line after 'break'.");
        errorAt(previousToken, "break statement not yet implemented");
    }

    void CParser::continueStatement()
    {
        consumeEnd("Expect end of line after 'continue'.");
        errorAt(previousToken, "continue statement not yet implemented");
    }

    void CParser::returnStatement()
    {
        // Allow return from top-level scripts to exit early
        // This is useful for early-exit patterns in event handlers

        if (match(TokenType::EOL, TokenType::END))
        {
            emitReturn();
        }
        else
        {
            if (Compiler::current->currentType == FunctionType::TYPE_INITIALIZER)
            {
                errorAt(previousToken, "Can't return a value from an initializer.");
                // emitReturn();
                // return;
            }

            // For top-level scripts, discard the return value and just exit
            if (Compiler::current->currentType == FunctionType::TYPE_SCRIPT)
            {
                expression();
                consumeEnd("Expect ';' or end of line after return value.");
                writeByte(OpCode::OP_Pop);  // Discard the return value
                emitReturn();
            }
            else
            {
                expression();
                consumeEnd("Expect ';' or end of line after return value.");
                writeByte(OpCode::OP_Return);
            }
        }
    }

    void CParser::importStatement()
    {
        // Get the module name

        auto moduleName = getModuleName();

        if (not parseImportFile(moduleName))
        {
            std::cout << "Trying native module import for '" << moduleName << "'" << std::endl;
            // Try to import a native module if file import failed
            if (not vm->loadNativeModule(moduleName))
            {
                error("Failed to import module '" + moduleName + "': not found as file or native module");
            }
            else
            {
                // Track that this native module was imported (for bytecode caching)
                Compiler::current->getCurrentChunk().importedModules.push_back(moduleName);
            }
        }

        // Check for multiple imports: import "mod1", "mod2", "mod3"
        while (match(TokenType::COMMA))
        {
            skipEOL();

            moduleName = getModuleName();

            if (not parseImportFile(moduleName))
            {
                // Try to import a native module if file import failed
                if (not vm->loadNativeModule(moduleName))
                {
                    error("Failed to import module '" + moduleName + "': not found as file or native module");
                }
                else
                {
                    // Track that this native module was imported (for bytecode caching)
                    Compiler::current->getCurrentChunk().importedModules.push_back(moduleName);
                }
            }
        }

        consumeEnd("Expect ';' or end of line after import statement.");
    }

    void CParser::methodStatement()
    {
        consume("Expect method name.", TokenType::EXPRESSION);
        Token methodName = previousToken;

        FunctionType type = FunctionType::TYPE_METHOD;
        if (methodName.text == "init")
        {
            type = FunctionType::TYPE_INITIALIZER;
        }

        parseFunction(type);

        writeByte(OpCode::OP_Method);
        auto value = vm->createString(methodName.text); // Ensure string is created in VM
        uint8_t constantIndex = Compiler::current->getCurrentChunk().addConstantIndex(value);
        writeByte(constantIndex);
    }

    ParseRule& CParser::getRule(const TokenType& type) const
    {
        return rules[type];
    }

    bool CParser::parseImportFile(const std::string& moduleName)
    {
        // Add .pg extension if not present
        std::string fileName = moduleName;
        if (fileName.find(".pg") == std::string::npos)
        {
            fileName += ".pg";
        }

        // Determine the compiled file name based on what exists
        std::string compiledFileName;
        if (fileName.find(".pgc") != std::string::npos)
        {
            compiledFileName = fileName;
        }
        else if (UniversalFileAccessor::exists(fileName + "c"))
        {
            compiledFileName = fileName + "c";
        }
        else if (UniversalFileAccessor::exists(fileName + ".pgc"))
        {
            compiledFileName = fileName + ".pgc";
        }
        else
        {
            compiledFileName = fileName.substr(0, fileName.find_last_of(".")) + ".pgc";
        }

        if (fileName.find(".pgc") != std::string::npos or UniversalFileAccessor::exists(fileName + "c") or UniversalFileAccessor::exists(fileName + ".pgc"))
        {
            // .pgc file exists - load from serialized bytecode
            try
            {
                std::cout << "Trying a compiled source" << std::endl;

                // Load the chunk from the .pgc file
                Chunk chunk;
                ChunkSerializer serializer;
                if (!serializer.deserializeFromFile(chunk, compiledFileName, vm))
                {
                    error("Failed to load compiled module '" + moduleName + "' from " + compiledFileName);
                    return true; // File exists but failed to deserialize - don't fallback to native module
                }

                std::cout << "Loaded compiled module '" + moduleName + "' from " << compiledFileName << std::endl;

                // Wrap the deserialized chunk in a script function
                Value funcValue = vm->createFunction();
                ObjFunction* funcObj = vm->asFunction(funcValue);
                funcObj->chunk = chunk;
                funcObj->name = moduleName;
                funcObj->arity = 0;
                funcObj->upvalueCount = 0;

                allocatedFunction.push_back(funcValue);

                // Emit bytecode to call the imported script immediately
                // This will execute it in the same VM and populate globals
                uint8_t constant = Compiler::current->getCurrentChunk().addConstantIndex(funcValue);
                writeByte(OpCode::OP_Closure);
                writeByte(constant);

                // No upvalues for scripts - upvalue count is 0

                // Call the imported script with 0 arguments
                writeByte(OpCode::OP_Call);
                writeByte(0);  // 0 arguments

                // Pop the return value
                writeByte(OpCode::OP_Pop);
            }
            catch (const std::exception& e)
            {
                error("Failed to import compiled module '" + moduleName + "': " + std::string(e.what()));
                return true; // File exists but error during processing - don't fallback to native module
            }

            return true; // Successfully loaded .pgc file
        }

        // Check if .pg file exists
        if (!UniversalFileAccessor::exists(fileName))
        {
            // File doesn't exist - return false to allow fallback to native module
            return false;
        }

        // .pg file exists - load and compile the imported file during parsing
        try
        {
            // Use the Lexer to read and tokenize the file
            Lexer lexer;
            lexer.readFromFile(fileName);
            auto importTokens = lexer.getTokens();

            // Create a nested compiler for the imported module
            Compiler importCompiler(vm);
            importCompiler.initCompiler(FunctionType::TYPE_SCRIPT);
            importCompiler.parser.parse(importTokens);
            importCompiler.parser.setCompiler(&importCompiler);

            // Parse all declarations in the imported file
            while (not importCompiler.parser.isAtEnd() and not importCompiler.parser.hasError())
            {
                importCompiler.parser.skipEOL();
                importCompiler.parser.declaration();
            }

            if (importCompiler.parser.hasError())
            {
                error("Error compiling imported module '" + moduleName + "'.");
                return true; // File exists but compilation error - don't fallback to native module
            }

            auto importedFunction = importCompiler.endCompiler();
            allocatedFunction.push_back(importedFunction);

            // Transfer ownership of all functions from imported parser to prevent premature release
            for (auto func : importCompiler.parser.allocatedFunction)
            {
                allocatedFunction.push_back(func);
            }

            importCompiler.parser.allocatedFunction.clear();

            // Emit bytecode to call the imported script immediately
            // This will execute it in the same VM and populate globals
            uint8_t constant = Compiler::current->getCurrentChunk().addConstantIndex(importedFunction);
            writeByte(OpCode::OP_Closure);
            writeByte(constant);

            // No upvalues for scripts - upvalue count is 0

            // Call the imported script with 0 arguments
            writeByte(OpCode::OP_Call);
            writeByte(0);  // 0 arguments

            // Pop the return value
            writeByte(OpCode::OP_Pop);

            return true; // Successfully compiled and loaded .pg file
        }
        catch (const std::exception& e)
        {
            // File exists but couldn't be read/processed
            error("Failed to import '" + moduleName + "': " + std::string(e.what()));
            return true; // File exists but error during processing - don't fallback to native module
        }
    }

    void CParser::parseFunction(const FunctionType& type)
    {
        Compiler compiler(vm);

        compiler.initCompiler(type, previousToken.text);
        compiler.beginScope();

        skipEOL();
        consume("Expect '(' after function name.", TokenType::PENTER);
        skipEOL();

        if (not check(TokenType::PCLOSE))
        {
            do
            {
                vm->asFunction(Compiler::current->currentFunction)->arity++;
                if (vm->asFunction(Compiler::current->currentFunction)->arity > 255)
                {
                    errorAt(previousToken, "Can't have more than 255 parameters.");
                }

                consume("Expect variable name.", TokenType::EXPRESSION);

                Token varName = previousToken;

                declareVariable(varName);

                Compiler::current->markInitialized();

            } while (match(TokenType::COMMA));
        }

        skipEOL();
        consume("Expect ')' after parameters.", TokenType::PCLOSE);
        skipEOL();
        consume("Expect '{' before function body.", TokenType::BENTER);
        skipEOL();

        blockStatement();

        auto function = compiler.endCompiler();

        allocatedFunction.push_back(function);

        writeByte(OpCode::OP_Closure);
        uint8_t constantIndex = Compiler::current->getCurrentChunk().addConstantIndex(function);
        writeByte(constantIndex);

        for (int i = 0; i < vm->asFunction(function)->upvalueCount; i++)
        {
            writeByte(compiler.upvalues[i].isLocal ? 1 : 0);
            writeByte(compiler.upvalues[i].index);
        }
    }

    void CParser::declareVariable(const Token& name)
    {
        Compiler::current->addLocal(name);
    }

    void CParser::pushVariableInStack(const std::string& varName)
    {
        int arg = Compiler::current->findLocal(varName);

        OpCode getOp;

        if (arg != -1)
        {
            getOp = OpCode::OP_Get_Local;
        }
        else if ((arg = Compiler::current->findUpvalue(varName)) != -1)
        {
            getOp = OpCode::OP_Get_Upvalue;
        }
        else
        {
            // Global variable - use constant pool
            writeConstant(varName);
            writeByte(OpCode::OP_Get_Global);

            return;
        }

        writeByte(getOp);
        writeByte(static_cast<uint8_t>(arg));
    }

    int CParser::emitJump(const OpCode& instruction)
    {
        writeByte(instruction);
        writeByte(0xff);
        writeByte(0xff);
        writeByte(0xff);
        writeByte(0xff);

        return static_cast<int>(Compiler::current->getCurrentChunk().code.size() - 4);
    }

    void CParser::patchJump(int offset)
    {
        // -1 to adjust for the bytecode for the jump offset itself
        size_t jump = Compiler::current->getCurrentChunk().code.size() - offset - 4;

        if (jump > 0xFFFFFFFF)
        {
            errorAt(previousToken, "Too much code to jump over.");
        }

        Compiler::current->getCurrentChunk().code[offset]     = (jump >> 24) & 0xFF;
        Compiler::current->getCurrentChunk().code[offset + 1] = (jump >> 16) & 0xFF;
        Compiler::current->getCurrentChunk().code[offset + 2] = (jump >> 8) & 0xFF;
        Compiler::current->getCurrentChunk().code[offset + 3] = jump & 0xFF;
    }

    void CParser::emitLoop(int offset)
    {
        writeByte(OpCode::OP_Long_Loop);

        size_t jump = Compiler::current->getCurrentChunk().code.size() - offset + 4;

        if (jump > 0xFFFFFFFF)
        {
            errorAt(previousToken, "Loop body too large.");
        }

        writeByte((jump >> 24) & 0xFF);
        writeByte((jump >> 16) & 0xFF);
        writeByte((jump >> 8) & 0xFF);
        writeByte(jump & 0xFF);
    }

    void CParser::emitReturn()
    {
        if (Compiler::current->currentType == FunctionType::TYPE_INITIALIZER)
        {
            // Load "this" for initializer return
            writeByte(OpCode::OP_Get_Local);
            writeByte(0);
        }
        else
            writeConstant(0);

        writeByte(OpCode::OP_Return);
    }

    void CParser::writeConstant(const ElementType& constant)
    {
        Compiler::current->getCurrentChunk().addConstant(vm->elementToValue(constant), previousToken.line);
    }

    void CParser::writeByte(const OpCode& byte)
    {
        Compiler::current->getCurrentChunk().addCode(byte, previousToken.line);
    }

    void CParser::writeByte(uint8_t byte)
    {
        Compiler::current->getCurrentChunk().addCode(byte, previousToken.line);
    }

    void CParser::synchronize()
    {
        panicMode = false;

        while (not isAtEnd())
        {
            if (previousToken.type == TokenType::END or previousToken.type == TokenType::EOL)
                return;

            switch (currentToken().type)
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

            advance();
        }
    }

    void CParser::errorAt(const Token& token, const std::string& message)
    {
        if (panicMode)
            return;

        panicMode = true;

        LOG_ERROR("CParser", "Syntax Error: " << message << " at line " << token.line << ", column " << token.column << ", in file: " << vm->currentFileName << ".");

        hadError = true;
    }
}