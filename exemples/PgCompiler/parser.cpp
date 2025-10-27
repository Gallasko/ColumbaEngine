#include "parser.h"

#include "compiler.h"

#include "logger.h"
#include <unordered_map>

namespace pg
{
    void intNumber(Parser& parser, bool)
    {
        auto n = std::stoi(parser.previousToken.text);
        parser.writeConstant(n);
    }

    void floatNumber(Parser& parser, bool)
    {
        auto n = std::stof(parser.previousToken.text);
        parser.writeConstant(n);
    }

    void strLiterral(Parser& parser, bool)
    {
        auto str = parser.previousToken.text;
        parser.writeConstant(str);
    }

    void litteral(Parser& parser, bool)
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

    void grouping(Parser& parser, bool)
    {
        parser.expression();
        parser.consume("Expect ')' after expression.", TokenType::PCLOSE);
    }

    void unary(Parser& parser, bool)
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

    void binary(Parser& parser, bool)
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

    void variable(Parser& parser, bool canAssign)
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
            parser.writeConstant(varName);
            if (canAssign and parser.match(TokenType::EQUAL))
            {
                parser.expression();
                parser.writeByte(OpCode::OP_Set_Global);
            }
            else if (parser.match(TokenType::INCREMENT))
            {
                // Postfix increment: var++
                parser.writeByte(OpCode::OP_Get_Global);
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Post_Incr_Global);
            }
            else if (parser.match(TokenType::DECREMENT))
            {
                // Postfix decrement: var--
                parser.writeByte(OpCode::OP_Get_Global);
                parser.writeConstant(varName);
                parser.writeByte(OpCode::OP_Post_Decr_Global);
            }
            else
            {
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

    void andOp(Parser& parser, bool)
    {
        // For 'and': if the left operand is false, short-circuit to false
        // If left operand is true, evaluate right operand
        int endJump = parser.emitJump(OpCode::OP_Long_Jump_If_False);

        parser.writeByte(OpCode::OP_Pop);
        parser.parsePrecedence(Precedence::AND);

        parser.patchJump(endJump);
    }

    void orOp(Parser& parser, bool)
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

    void decrementOp(Parser& parser, bool)
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

    void incrementOp(Parser& parser, bool)
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

    void postfixIncrementOp(Parser& parser, bool)
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
            ElementType lastConstant = valueToElement(chunk.constants.back());
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

    void postfixDecrementOp(Parser& parser, bool)
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
            ElementType lastConstant = valueToElement(chunk.constants.back());
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

    uint8_t argumentList(Parser& parser)
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

    void call(Parser& parser, bool)
    {
        auto argCount = argumentList(parser);

        parser.emitBytes(OpCode::OP_Call, argCount);
    }

    void dot(Parser& parser, bool canAssign)
    {
        parser.consume("Expect property name after '.'.", TokenType::EXPRESSION);

        auto propertyName = parser.previousToken.text;
        uint8_t constantIndex = Compiler::current->getCurrentChunk().addConstantIndex(propertyName);

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
    }

    void this_(Parser& parser, bool)
    {
        if (Compiler::current->currentClass == nullptr)
        {
            parser.errorAt(parser.previousToken, "Can't use 'this' outside of a class.");
            return;
        }

        variable(parser, false);
    }

    std::unordered_map<TokenType, ParseRule> rules = {
        {TokenType::EQUAL,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::PLUS,         {NULL,        binary, Precedence::TERM}},
        {TokenType::MINUS,        {unary,       binary, Precedence::TERM}},
        {TokenType::STAR,         {NULL,        binary, Precedence::FACTOR}},
        {TokenType::MOD,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::POW,          {NULL,        NULL,   Precedence::NONE}},
        {TokenType::PENTER,       {grouping,    call,   Precedence::CALL}},
        {TokenType::PCLOSE,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::BENTER,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::BCLOSE,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::CENTER,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::CCLOSE,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SUP,          {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::INF,          {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::NOT,          {unary,       NULL,   Precedence::NONE}},
        {TokenType::QMARK,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TILDE,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::AMPER,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::COMMA,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::POINT,        {NULL,        dot,    Precedence::CALL}},
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
        {TokenType::SUPEQUAL,     {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::INFEQUAL,     {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::INCREMENT,    {incrementOp, NULL, Precedence::NONE}},
        {TokenType::DECREMENT,    {decrementOp, NULL, Precedence::NONE}},
        {TokenType::LOGICAND,     {NULL,        andOp,  Precedence::AND}},
        {TokenType::LOGICOR,      {NULL,        orOp,   Precedence::OR}},
        {TokenType::SHIFTLEFT,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SHIFTRIGHT,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::EQUALEQUAL,   {NULL,        binary, Precedence::EQUALITY}},
        {TokenType::NOTEQUAL,     {NULL,        binary, Precedence::EQUALITY}},
        {TokenType::ARROW,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::SCOPE,        {NULL,        NULL,   Precedence::NONE}},
        {TokenType::ENDOFFILE,    {NULL,        NULL,   Precedence::NONE}},
        {TokenType::EXPRESSION,   {variable,    NULL,   Precedence::NONE}},
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
        {TokenType::TOK_THIS,     {this_,       NULL,   Precedence::NONE}},
        {TokenType::TOK_FOR,      {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_IMPORT,   {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_FROM,     {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_AS,       {NULL,        NULL,   Precedence::NONE}},
        {TokenType::TOK_ERROR,    {NULL,        NULL,   Precedence::NONE}},
    };


    void Parser::parsePrecedence(const Precedence& precedence)
    {
        advance();

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

    void Parser::declaration()
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

    void Parser::varDeclaration()
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

    void Parser::funDeclaration()
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

    void Parser::classDeclaration()
    {
        consume("Expect class name.", TokenType::EXPRESSION);
        Token className = previousToken;

        // Emit OP_Class with constant index as operand (not OP_Constant before it)
        uint8_t nameConstant = Compiler::current->getCurrentChunk().addConstantIndex(elementToValue(ElementType(className.text)));
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

    void Parser::statement()
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
        else
        {
            expressionStatement();
        }
    }

    void Parser::expressionStatement()
    {
        expression();
        consumeEnd("Expect end of expression.");
        writeByte(OpCode::OP_Pop);
    }

    void Parser::blockStatement()
    {
        while (not check(TokenType::BCLOSE) and not isAtEnd())
        {
            skipEOL();
            declaration();
        }

        consume("Expect '}' after block.", TokenType::BCLOSE);
    }

    void Parser::ifStatement()
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

        if (match(TokenType::TOK_ELSE))
        {
            statement();
        }

        patchJump(elseJump);
    }

    void Parser::dprintStatement()
    {
        skipEOL();
        consume("Expect '(' after '__dprint'.", TokenType::PENTER);
        skipEOL();

        expression();

        consume("Expect ')' after expression.", TokenType::PCLOSE);
        consumeEnd("Expect ';' or newline after '__dprint' statement.");

        writeByte(OpCode::OP_Debug_Print);
    }

    void Parser::whileStatement()
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

    void Parser::forStatement()
    {
        Compiler::current->beginScope();

        skipEOL();
        consume("Expect '(' after 'for'.", TokenType::PENTER);
        skipEOL();

        // Initializer
        if (match(TokenType::END))
        {
            // No initializer
        }
        else if (match(TokenType::TOK_VAR))
        {
            varDeclaration();
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

    void Parser::returnStatement()
    {
        if (Compiler::current->currentType == FunctionType::TYPE_SCRIPT)
        {
            errorAt(previousToken, "Can't return from top level script.");
        }

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

            expression();
            consume("Expect ';' or end of line after return value.", TokenType::EOL, TokenType::END);
            writeByte(OpCode::OP_Return);
        }
    }

    void Parser::methodStatement()
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
        uint8_t constantIndex = Compiler::current->getCurrentChunk().addConstantIndex(methodName.text);
        writeByte(constantIndex);
    }

    ParseRule& Parser::getRule(const TokenType& type) const
    {
        return rules[type];
    }

    void Parser::parseFunction(const FunctionType& type)
    {
        Compiler compiler;

        compiler.initCompiler(type, previousToken.text);
        compiler.beginScope();

        skipEOL();
        consume("Expect '(' after function name.", TokenType::PENTER);
        skipEOL();

        if (not check(TokenType::PCLOSE))
        {
            do
            {
                Compiler::current->currentFunction->arity++;
                if (Compiler::current->currentFunction->arity > 255)
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

        ObjFunction *function = compiler.endCompiler();

        allocatedFunction.push_back(function);

        writeByte(OpCode::OP_Closure);
        uint8_t constantIndex = Compiler::current->getCurrentChunk().addConstantIndex(function);
        writeByte(constantIndex);

        for (int i = 0; i < function->upvalueCount; i++)
        {
            writeByte(compiler.upvalues[i].isLocal ? 1 : 0);
            writeByte(compiler.upvalues[i].index);
        }
    }

    void Parser::declareVariable(const Token& name)
    {
        Compiler::current->addLocal(name);
    }

    void Parser::pushVariableInStack(const std::string& varName)
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

    int Parser::emitJump(const OpCode& instruction)
    {
        writeByte(instruction);
        writeByte(0xff);
        writeByte(0xff);
        writeByte(0xff);
        writeByte(0xff);

        return static_cast<int>(Compiler::current->getCurrentChunk().code.size() - 4);
    }

    void Parser::patchJump(int offset)
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

    void Parser::emitLoop(int offset)
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

    void Parser::emitReturn()
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

    void Parser::writeConstant(ObjFunction* constant)
    {
        Compiler::current->getCurrentChunk().addConstant(constant, previousToken.line);
    }

    void Parser::writeConstant(const ElementType& constant)
    {
        Compiler::current->getCurrentChunk().addConstant(constant, previousToken.line);
    }

    void Parser::writeByte(const OpCode& byte)
    {
        Compiler::current->getCurrentChunk().addCode(byte, previousToken.line);
    }

    void Parser::writeByte(uint8_t byte)
    {
        Compiler::current->getCurrentChunk().addCode(byte, previousToken.line);
    }

    void Parser::synchronize()
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

    void Parser::errorAt(const Token& token, const std::string& message)
    {
        if (panicMode)
            return;

        panicMode = true;

        LOG_ERROR("Parser", "Syntax Error: " + message + " at line " + std::to_string(token.line) + ", column " + std::to_string(token.column));

        hadError = true;
    }
}