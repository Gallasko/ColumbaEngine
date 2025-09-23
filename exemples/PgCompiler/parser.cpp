#include "parser.h"

#include "compiler.h"

#include "logger.h"
#include <unordered_map>

namespace pg
{
    void intNumber(Chunk& chunk, Parser& parser, bool)
    {
        auto n = std::stoi(parser.previousToken.text);
        parser.writeConstant(chunk, n);
    }

    void floatNumber(Chunk& chunk, Parser& parser, bool)
    {
        auto n = std::stof(parser.previousToken.text);
        parser.writeConstant(chunk, n);
    }

    void strLiterral(Chunk& chunk, Parser& parser, bool)
    {
        auto str = parser.previousToken.text;
        parser.writeConstant(chunk, str);
    }

    void litteral(Chunk& chunk, Parser& parser, bool)
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

    void grouping(Chunk& chunk, Parser& parser, bool)
    {
        parser.expression(chunk);
        parser.consume("Expect ')' after expression.", TokenType::PCLOSE);
    }

    void unary(Chunk& chunk, Parser& parser, bool)
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
            case TokenType::NOT:
                chunk.addCode(OpCode::OP_Not, operatorToken.line);
                break;
            default:
                return; // Unreachable
        }
    }

    void binary(Chunk& chunk, Parser& parser, bool)
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
            case TokenType::LOGICAND:
                parser.writeByte(chunk, OpCode::OP_And);
                break;
            case TokenType::LOGICOR:
                parser.writeByte(chunk, OpCode::OP_Or);
                break;
            case TokenType::EQUALEQUAL:
                parser.writeByte(chunk, OpCode::OP_Equal);
                break;
            case TokenType::NOTEQUAL:
                parser.writeByte(chunk, OpCode::OP_NotEqual);
                break;
            case TokenType::INF:
                parser.writeByte(chunk, OpCode::OP_Less);
                break;
            case TokenType::INFEQUAL:
                parser.writeByte(chunk, OpCode::OP_LessEqual);
                break;
            case TokenType::SUP:
                parser.writeByte(chunk, OpCode::OP_Greater);
                break;
            case TokenType::SUPEQUAL:
                parser.writeByte(chunk, OpCode::OP_GreaterEqual);
                break;
            default:
                return; // Unreachable
        }
    }

    void variable(Chunk& chunk, Parser& parser, bool canAssign)
    {
        auto varName = parser.previousToken.text;

        OpCode setOp, getOp;

        int arg = parser.compiler->resolveLocal(chunk, parser.previousToken);

        if (arg != -1)
        {
            parser.writeConstant(chunk, arg);
            setOp = OpCode::OP_Set_Local;
            getOp = OpCode::OP_Get_Local;
        }
        else
        {
            parser.writeConstant(chunk, varName);
            setOp = OpCode::OP_Set_Global;
            getOp = OpCode::OP_Get_Global;
        }

        if (canAssign and parser.match(TokenType::EQUAL))
        {
            parser.expression(chunk);
            parser.writeByte(chunk, setOp);
        }
        else
        {
            parser.writeByte(chunk, getOp);
        }
    }

    void andOp(Chunk& chunk, Parser& parser, bool)
    {
        // For 'and': if the left operand is false, short-circuit to false
        // If left operand is true, evaluate right operand
        int endJump = parser.emitJump(chunk, OpCode::OP_Long_Jump_If_False);

        parser.writeByte(chunk, OpCode::OP_Pop);
        parser.parsePrecedence(chunk, Precedence::AND);

        parser.patchJump(chunk, endJump);
    }

    void orOp(Chunk& chunk, Parser& parser, bool)
    {
        // For 'or': if the left operand is false, jump to evaluate right operand
        // If left operand is true, short-circuit to true
        int elseJump = parser.emitJump(chunk, OpCode::OP_Long_Jump_If_False);
        int endJump = parser.emitJump(chunk, OpCode::OP_Long_Jump);

        parser.patchJump(chunk, elseJump);
        parser.writeByte(chunk, OpCode::OP_Pop);

        parser.parsePrecedence(chunk, Precedence::OR);
        parser.patchJump(chunk, endJump);
    }

    void decrementOp(Chunk& chunk, Parser& parser, bool)
    {
        Token operatorToken = parser.previousToken;

        // Check if next token is a variable (expression/identifier)
        if (parser.check(TokenType::EXPRESSION))
        {
            // Prefix decrement: --var
            parser.advance(); // consume the identifier
            Token varToken = parser.previousToken;
            auto varName = varToken.text;

            OpCode setOp, getOp;
            ElementType identifier;
            int arg = parser.compiler->resolveLocal(chunk, varToken);

            if (arg != -1)
            {
                // Local variable
                setOp = OpCode::OP_Set_Local;
                getOp = OpCode::OP_Get_Local;
                identifier = ElementType(arg);
            }
            else
            {
                // Global variable
                setOp = OpCode::OP_Set_Global;
                getOp = OpCode::OP_Get_Global;
                identifier = ElementType(varName);
            }

            // Prefix decrement: --var (modify variable, return new value)
            parser.writeConstant(chunk, identifier);          // Push identifier [id]
            parser.writeConstant(chunk, identifier);          // Push identifier again [id, id]
            parser.writeByte(chunk, getOp);                   // Get current value [id, value]
            parser.writeConstant(chunk, ElementType(1));      // Push 1 [id, value, 1]
            parser.writeByte(chunk, OpCode::OP_Subtract);     // Subtract 1 [id, new_value]
            parser.writeByte(chunk, setOp);                   // Store new value (leaves value on stack)
        }
        else
        {
            // Parse the operand at unary precedence (for literals like --5)
            parser.parsePrecedence(chunk, Precedence::UNARY);

            // Treat DECREMENT as double unary minus (--5 becomes -(-5) = 5)
            chunk.addCode(OpCode::OP_Negate, operatorToken.line);
            chunk.addCode(OpCode::OP_Negate, operatorToken.line);
        }
    }

    void incrementOp(Chunk& chunk, Parser& parser, bool)
    {
        Token operatorToken = parser.previousToken;

        // Check if next token is a variable (expression/identifier)
        if (parser.check(TokenType::EXPRESSION))
        {
            // Prefix increment: ++var
            parser.advance(); // consume the identifier
            Token varToken = parser.previousToken;
            auto varName = varToken.text;

            OpCode setOp, getOp;
            ElementType identifier;
            int arg = parser.compiler->resolveLocal(chunk, varToken);

            if (arg != -1)
            {
                // Local variable
                setOp = OpCode::OP_Set_Local;
                getOp = OpCode::OP_Get_Local;
                identifier = ElementType(arg);
            }
            else
            {
                // Global variable
                setOp = OpCode::OP_Set_Global;
                getOp = OpCode::OP_Get_Global;
                identifier = ElementType(varName);
            }

            // Prefix increment: ++var (modify variable, return new value)
            parser.writeConstant(chunk, identifier);          // Push identifier [id]
            parser.writeConstant(chunk, identifier);          // Push identifier again [id, id]
            parser.writeByte(chunk, getOp);                   // Get current value [id, value]
            parser.writeConstant(chunk, ElementType(1));      // Push 1 [id, value, 1]
            parser.writeByte(chunk, OpCode::OP_Add);          // Add 1 [id, new_value]
            parser.writeByte(chunk, setOp);                   // Store new value (leaves value on stack)
        }
        else
        {
            // Parse the operand at unary precedence (for literals like ++5)
            parser.parsePrecedence(chunk, Precedence::UNARY);

            // For literals, ++5 doesn't make much sense, but we can treat it as +(+5) = 5
            // This is a no-op for numbers
        }
    }

    void postfixIncrementOp(Chunk& chunk, Parser& parser, bool)
    {
        // For postfix increment: var++ (return old value, modify variable)
        // Stack currently has [old_value] from the variable access
        // We need to: return old_value, but also increment the variable

        // Strategy: We need to re-identify the variable that was just accessed
        // Since we're in a postfix context, we can look at the bytecode that was just generated

        // The last operations should have been:
        // OP_Constant <var_name/slot>
        // OP_Get_Global/Local

        // We can examine the last constant that was added to identify the variable
        if (chunk.constants.empty()) {
            parser.errorAt(parser.previousToken, "No variable found for postfix increment");
            return;
        }

        // Get the last constant (should be the variable identifier)
        ElementType lastConstant = chunk.constants.back();

        // Determine if this is a local or global variable
        OpCode setOp, getOp;
        ElementType identifier;

        // Check if it's a number (local variable slot) or string (global variable name)
        if (lastConstant.isNumber()) {
            // Local variable
            setOp = OpCode::OP_Set_Local;
            getOp = OpCode::OP_Get_Local;
            identifier = lastConstant;
        } else {
            // Global variable
            setOp = OpCode::OP_Set_Global;
            getOp = OpCode::OP_Get_Global;
            identifier = lastConstant;
        }

        // Now implement: return old_value, increment variable
        // Stack: [old_value]
        // We want to end with: [old_value] and variable incremented

        // Duplicate the old value for our return
        // But we don't have a DUP instruction, so we need to:
        // 1. Get the variable again
        // 2. Add 1
        // 3. Set it back
        // 4. The original old_value stays on stack

        parser.writeConstant(chunk, identifier);      // [old_value, id]
        parser.writeConstant(chunk, identifier);      // [old_value, id, id]
        parser.writeByte(chunk, getOp);               // [old_value, id, current_value]
        parser.writeConstant(chunk, ElementType(1));  // [old_value, id, current_value, 1]
        parser.writeByte(chunk, OpCode::OP_Add);      // [old_value, id, new_value]
        parser.writeByte(chunk, setOp);               // [old_value, new_value] (set returns the value)
        parser.writeByte(chunk, OpCode::OP_Pop);      // [old_value] (discard the new_value)

        // Result: old_value is on stack (for return), variable has been incremented
    }

    void postfixDecrementOp(Chunk& chunk, Parser& parser, bool)
    {
        // For postfix decrement: var-- (return old value, modify variable)
        // Stack currently has [old_value] from the variable access
        // We need to: return old_value, but also decrement the variable

        // Strategy: Same as postfixIncrementOp - examine the last constant to identify the variable
        if (chunk.constants.empty()) {
            parser.errorAt(parser.previousToken, "No variable found for postfix decrement");
            return;
        }

        // Get the last constant (should be the variable identifier)
        ElementType lastConstant = chunk.constants.back();

        // Determine if this is a local or global variable
        OpCode setOp, getOp;
        ElementType identifier;

        // Check if it's a number (local variable slot) or string (global variable name)
        if (lastConstant.isNumber()) {
            // Local variable
            setOp = OpCode::OP_Set_Local;
            getOp = OpCode::OP_Get_Local;
            identifier = lastConstant;
        } else {
            // Global variable
            setOp = OpCode::OP_Set_Global;
            getOp = OpCode::OP_Get_Global;
            identifier = lastConstant;
        }

        // Now implement: return old_value, decrement variable
        // Stack: [old_value]
        // We want to end with: [old_value] and variable decremented

        parser.writeConstant(chunk, identifier);      // [old_value, id]
        parser.writeConstant(chunk, identifier);      // [old_value, id, id]
        parser.writeByte(chunk, getOp);               // [old_value, id, current_value]
        parser.writeConstant(chunk, ElementType(1));  // [old_value, id, current_value, 1]
        parser.writeByte(chunk, OpCode::OP_Subtract); // [old_value, id, new_value]
        parser.writeByte(chunk, setOp);               // [old_value, new_value] (set returns the value)
        parser.writeByte(chunk, OpCode::OP_Pop);      // [old_value] (discard the new_value)

        // Result: old_value is on stack (for return), variable has been decremented
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
        {TokenType::SUP,          {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::INF,          {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::NOT,          {unary,       NULL,   Precedence::NONE}},
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
        {TokenType::SUPEQUAL,     {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::INFEQUAL,     {NULL,        binary, Precedence::COMPARISON}},
        {TokenType::INCREMENT,    {incrementOp, postfixIncrementOp, Precedence::POSTFIX}},
        {TokenType::DECREMENT,    {decrementOp, postfixDecrementOp, Precedence::POSTFIX}},
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

        bool canAssign = precedence <= Precedence::ASSIGNMENT;

        prefixRule(chunk, *this, canAssign);

        while (precedence <= getRule(currentToken().type).precedence)
        {
            advance();
            ParseFn infixRule = getRule(previousToken.type).infix;
            infixRule(chunk, *this, canAssign);
        }

        if (canAssign and match(TokenType::EQUAL))
        {
            errorAt(previousToken, "Invalid assignment target.");
        }
    }

    void Parser::declaration(Chunk& chunk)
    {
        if (match(TokenType::TOK_VAR))
        {
            varDeclaration(chunk);
        }
        else
        {
            statement(chunk);
        }

        skipEOL();

        if (panicMode)
            synchronize();
    }

    void Parser::varDeclaration(Chunk& chunk)
    {
        consume("Expect variable name.", TokenType::EXPRESSION);

        Token varName = previousToken;

        if (compiler->scopeDepth > 0)
        {
            declareVariable(varName);
        }

        if (match(TokenType::EQUAL))
        {
            expression(chunk);
        }
        else
        {
            writeConstant(chunk, ElementType()); // Default initialize to 0
        }

        consumeEnd("Expect end of variable declaration.");

        if (compiler->scopeDepth > 0)
        {
            compiler->markInitialized();
            return;
        }

        writeConstant(chunk, varName.text);  // Push variable name onto stack
        writeByte(chunk, OpCode::OP_Define_Global);
    }

    void Parser::statement(Chunk& chunk)
    {
        if (match(TokenType::BENTER))
        {
            compiler->beginScope();
            blockStatement(chunk);
            compiler->endScope(chunk);
        }
        else if (match(TokenType::TOK_IF))
        {
            ifStatement(chunk);
        }
        else if (match(TokenType::TOK_WHILE))
        {
            whileStatement(chunk);
        }
        else if (match(TokenType::TOK_FOR))
        {
            forStatement(chunk);
        }
        else if (match(TokenType::TOK_DPRINT))
        {
            dprintStatement(chunk);
        }
        else
        {
            expressionStatement(chunk);
        }
    }

    void Parser::expressionStatement(Chunk& chunk)
    {
        expression(chunk);
        consumeEnd("Expect end of expression.");
        writeByte(chunk, OpCode::OP_Pop);
    }

    void Parser::blockStatement(Chunk& chunk)
    {
        while (not check(TokenType::BCLOSE) and not isAtEnd())
        {
            declaration(chunk);
        }

        consume("Expect '}' after block.", TokenType::BCLOSE);
    }

    void Parser::ifStatement(Chunk& chunk)
    {
        skipEOL();
        consume("Expect '(' after 'if'.", TokenType::PENTER);
        skipEOL();

        expression(chunk);

        consume("Expect ')' after condition.", TokenType::PCLOSE);
        skipEOL();

        int thenJump = emitJump(chunk, OpCode::OP_Long_Jump_If_False);
        writeByte(chunk, OpCode::OP_Pop); // Pop the condition

        statement(chunk);

        int elseJump = emitJump(chunk, OpCode::OP_Long_Jump);

        patchJump(chunk, thenJump);
        writeByte(chunk, OpCode::OP_Pop); // Pop the condition

        if (match(TokenType::TOK_ELSE))
        {
            statement(chunk);
        }

        patchJump(chunk, elseJump);
    }

    void Parser::dprintStatement(Chunk& chunk)
    {
        skipEOL();
        consume("Expect '(' after '__dprint'.", TokenType::PENTER);
        skipEOL();
        
        expression(chunk);
        
        consume("Expect ')' after expression.", TokenType::PCLOSE);
        consumeEnd("Expect ';' or newline after '__dprint' statement.");
        
        writeByte(chunk, OpCode::OP_Debug_Print);
    }

    void Parser::whileStatement(Chunk& chunk)
    {
        int loopStart = static_cast<int>(chunk.code.size());

        skipEOL();
        consume("Expect '(' after 'while'.", TokenType::PENTER);
        skipEOL();

        expression(chunk);

        consume("Expect ')' after condition.", TokenType::PCLOSE);
        skipEOL();

        int exitJump = emitJump(chunk, OpCode::OP_Long_Jump_If_False);
        writeByte(chunk, OpCode::OP_Pop); // Pop the condition

        statement(chunk);
        emitLoop(chunk, loopStart);

        patchJump(chunk, exitJump);
        writeByte(chunk, OpCode::OP_Pop); // Pop the condition
    }

    void Parser::forStatement(Chunk& chunk)
    {
        compiler->beginScope();

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
            varDeclaration(chunk);
        }
        else
        {
            expressionStatement(chunk);
        }

        skipEOL();

        int loopStart = static_cast<int>(chunk.code.size());

        // Condition
        int exitJump = -1;
        if (not match(TokenType::END))
        {
            expression(chunk);
            consume("Expect ';' after condition.", TokenType::END);

            exitJump = emitJump(chunk, OpCode::OP_Long_Jump_If_False);
            writeByte(chunk, OpCode::OP_Pop); // Pop the condition
        }

        skipEOL();

        // Increment
        if (not match(TokenType::PCLOSE))
        {
            int bodyJump = emitJump(chunk, OpCode::OP_Long_Jump);
            int incrementStart = static_cast<int>(chunk.code.size());

            expression(chunk);
            writeByte(chunk, OpCode::OP_Pop); // Pop the increment expression result

            consume("Expect ')' after for clauses.", TokenType::PCLOSE);

            emitLoop(chunk, loopStart);
            loopStart = incrementStart;

            patchJump(chunk, bodyJump);
        }

        skipEOL();

        // Body
        statement(chunk);
        emitLoop(chunk, loopStart);

        if (exitJump != -1)
        {
            patchJump(chunk, exitJump);
            writeByte(chunk, OpCode::OP_Pop); // Pop the condition
        }

        compiler->endScope(chunk);
    }

    ParseRule& Parser::getRule(const TokenType& type) const
    {
        return rules[type];
    }

    void Parser::declareVariable(const Token& name)
    {
        compiler->addLocal(name);
    }

    int Parser::emitJump(Chunk& chunk, const OpCode& instruction)
    {
        writeByte(chunk, instruction);
        writeByte(chunk, 0xff);
        writeByte(chunk, 0xff);
        writeByte(chunk, 0xff);
        writeByte(chunk, 0xff);

        return static_cast<int>(chunk.code.size() - 4);
    }

    void Parser::patchJump(Chunk& chunk, int offset)
    {
        // -1 to adjust for the bytecode for the jump offset itself
        size_t jump = chunk.code.size() - offset - 4;

        if (jump > 0xFFFFFFFF)
        {
            errorAt(previousToken, "Too much code to jump over.");
        }

        chunk.code[offset]     = (jump >> 24) & 0xFF;
        chunk.code[offset + 1] = (jump >> 16) & 0xFF;
        chunk.code[offset + 2] = (jump >> 8) & 0xFF;
        chunk.code[offset + 3] = jump & 0xFF;
    }

    void Parser::emitLoop(Chunk& chunk, int offset)
    {
        writeByte(chunk, OpCode::OP_Long_Loop);

        size_t jump = chunk.code.size() - offset + 4;

        if (jump > 0xFFFFFFFF)
        {
            errorAt(previousToken, "Loop body too large.");
        }

        writeByte(chunk, (jump >> 24) & 0xFF);
        writeByte(chunk, (jump >> 16) & 0xFF);
        writeByte(chunk, (jump >> 8) & 0xFF);
        writeByte(chunk, jump & 0xFF);
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