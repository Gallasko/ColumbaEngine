#include "parser.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        const char * DOM = "Parser";
    }

    std::string ParseException::createErrorMessage(const Token& token, const std::string& message) const noexcept
    {
        std::string errStr = "Parser Error: " + message + " at line " + std::to_string(token.line) + ", column " + std::to_string(token.column);
        
        return errStr;
    }

    std::queue<StatementPtr> Parser::parse()
    {
        LOG_THIS_MEMBER(DOM);

        std::queue<StatementPtr> statements;

        while (not isAtEnd())
        {
            try
            {
                statements.push(declaration());
            }
            catch (const ParseException& e)
            {
                //Synchronize
                synchronize();

                LOG_ERROR(DOM, e.what());
                errorEncountered = true;
            }
        }

        return statements;
    }

    std::queue<StatementPtr> Parser::block()
    {
        LOG_THIS_MEMBER(DOM);

        std::queue<StatementPtr> statements;

        skipEOL();

        while (not isAtEnd() and not check(TokenType::BCLOSE))
        {
            statements.push(declaration());

            skipEOL();
        }

        consume("Expect a } at the end of a block", TokenType::BCLOSE);

        return statements;
    }

    ExprPtr Parser::finishCall(ExprPtr caller)
    {
        LOG_THIS_MEMBER(DOM);

        std::queue<ExprPtr> arguments;

        if (not check(TokenType::PCLOSE))
        {
            do
            {
                arguments.push(expression());
                skipEOL();
            } while (match(TokenType::COMMA));
        }

        skipEOL();
        Token paren = consume("Expect ')' after arguments", TokenType::PCLOSE);

        return std::make_shared<CallExpression>(caller, paren, arguments);
    }

    ExprPtr Parser::finishList()
    {
        LOG_THIS_MEMBER(DOM);

        int nbEntries = 0;
        std::queue<ListElement> entries;

        ExprPtr lExpr;
        ExprPtr rExpr;

        if (not check(TokenType::CCLOSE))
        {
            do
            {
                lExpr = expression();
                skipEOL();

                // If a double point is present then the user made a dict entry like so "key: value"
                if (match(TokenType::DPOINT))
                {
                    skipEOL();
                    rExpr = expression();
                    skipEOL();
                }
                // Else we set the key of the value as the number of entries in the dict like so "nbEntries: value"
                else
                {
                    rExpr = lExpr;
                    lExpr = std::make_shared<Atom>(nbEntries);
                }

                nbEntries++;

                entries.push(ListElement{lExpr, rExpr});
            } while (match(TokenType::COMMA));
        }

        skipEOL();
        Token squareBracket = consume("Expect ']' after arguments", TokenType::CCLOSE);

        auto self = std::make_shared<This>(Token{TokenType::EXPRESSION, "this", squareBracket.line, squareBracket.column});

        return std::make_shared<List>(self, squareBracket, entries);
    }

    ExprPtr Parser::expression()
    {
        return assignment();
    }

    ExprPtr Parser::assignment()
    {
        auto expr = logicOr();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::EQUAL, TokenType::PLUSEQUAL, TokenType::MINUSEQUAL, TokenType::STAREQUAL, TokenType::DIVIDEQUAL, TokenType::MODEQUAL))
        {
            Token op = previousToken;

            skipEOL();
            auto rExpr = assignment();

            // Todo += desugaring breaks when used on list
            // var list = [0, 1, 2]; list[0] += 1; << Crashes

            // Desugaring operator +=, -=, *=, /= and %=
            switch(op.type)
            {
            case TokenType::PLUSEQUAL:
                rExpr = std::make_shared<BinaryExpression>(expr, Token{TokenType::PLUS, "+", op.line, op.column}, rExpr);
                break;

            case TokenType::MINUSEQUAL:
                rExpr = std::make_shared<BinaryExpression>(expr, Token{TokenType::MINUS, "-", op.line, op.column}, rExpr);
                break;

            case TokenType::STAREQUAL:
                rExpr = std::make_shared<BinaryExpression>(expr, Token{TokenType::STAR, "*", op.line, op.column}, rExpr);
                break;

            case TokenType::DIVIDEQUAL:
                rExpr = std::make_shared<BinaryExpression>(expr, Token{TokenType::SLASH, "/", op.line, op.column}, rExpr);
                break;

            case TokenType::MODEQUAL:
                rExpr = std::make_shared<BinaryExpression>(expr, Token{TokenType::SLASH, "%", op.line, op.column}, rExpr);
                break;

            default:
                break;
            }

            // Check if the expression is a variable definition
            if (expr->getType() == "Var")
            {
                auto& token = std::static_pointer_cast<Var>(expr)->name;
                return std::make_shared<Assign>(token, rExpr);
            }
            // Check if the expression is a field from a class
            else if (expr->getType() == "Get")
            {
                auto getExpr = std::static_pointer_cast<Get>(expr);
                return std::make_shared<Set>(getExpr->object, getExpr->name, rExpr);
            }
            // Check if the expression is an assignment to an array
            else if (expr->getType() == "CallExpression" and expr->getName() == "at")
            {
                auto callExpr = std::static_pointer_cast<CallExpression>(expr);

                // Check if it is call to a get function and not a simple function call with a function named "at"
                if (callExpr->caller and callExpr->caller->getType() == "Get")
                {
                    auto getExpr = std::static_pointer_cast<Get>(callExpr->caller);

                    // Change the function call from "at" to "set"
                    getExpr->name.text = "set";
                    // Push the assign value as the second parameter of the set function call
                    callExpr->args.push(rExpr);
                    return expr;
                }
            }

            LOG_ERROR(DOM, "Syntax Error: Invalid lvalue for assignment at line " + std::to_string(op.line) + ", column " + std::to_string(op.column));
            errorEncountered = true;
        }

        return expr;
    }

    ExprPtr Parser::logicOr()
    {
        auto expr = logicAnd();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::LOGICOR))
        {
            Token op = previousToken;
            
            skipEOL();
            auto rExpr = logicAnd();

            expr = std::make_shared<LogicExpression>(expr, op, rExpr);
        }

        return expr;
    }

    ExprPtr Parser::logicAnd()
    {
        auto expr = equality();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::LOGICAND))
        {
            Token op = previousToken;
            
            skipEOL();
            auto rExpr = equality();

            expr = std::make_shared<LogicExpression>(expr, op, rExpr);
        }

        return expr;
    }

    ExprPtr Parser::equality()
    {
        auto expr = comparison();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::NOTEQUAL, TokenType::EQUALEQUAL))
        {
            Token op = previousToken;
            
            skipEOL();
            auto rExpr = comparison();

            expr = std::make_shared<BinaryExpression>(expr, op, rExpr);
        }

        return expr;
    }

    ExprPtr Parser::comparison()
    {
        auto expr = term();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::SUPEQUAL, TokenType::INFEQUAL, TokenType::SUP, TokenType::INF))
        {
            Token op = previousToken;
            
            skipEOL();
            auto rExpr = term();

            expr = std::make_shared<BinaryExpression>(expr, op, rExpr);
        }

        return expr;
    }

    ExprPtr Parser::term()
    {
        auto expr = factor();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::MINUS, TokenType::PLUS))
        {
            Token op = previousToken;
            
            skipEOL();
            auto rExpr = factor();

            expr = std::make_shared<BinaryExpression>(expr, op, rExpr);
        }

        return expr;
    }

    ExprPtr Parser::factor()
    {
        auto expr = unary();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::SLASH, TokenType::STAR, TokenType::MOD))
        {
            Token op = previousToken;
            
            skipEOL();
            auto rExpr = unary();

            expr = std::make_shared<BinaryExpression>(expr, op, rExpr);
        }

        return expr;
    }

    ExprPtr Parser::unary()
    {
        if (match(TokenType::NOT, TokenType::MINUS))
        {
            LOG_THIS_MEMBER(DOM);

            Token op = previousToken;
            auto expr = unary();

            return std::make_shared<UnaryExpression>(expr, op);
        }
        else if (match(TokenType::INCREMENT, TokenType::DECREMENT))
        {
            LOG_THIS_MEMBER(DOM);

            Token op = previousToken;
            auto expr = unary();

            // Check if the expression is a variable definition
            if (expr->getType() == "Var")
            {
                auto& token = std::static_pointer_cast<Var>(expr)->name;
                return std::make_shared<PreFixExpression>(expr, op, token);
            }
            throw ParseException(tokenList.front(), "Expected Variable after pre fix operator");
        }

        return call();
    }

    ExprPtr Parser::call()
    {
        auto expr = primary();

        LOG_THIS_MEMBER(DOM);

        while (match(TokenType::INCREMENT, TokenType::DECREMENT)) 
        {
            Token op = previousToken;

            // Check if the expression is a variable definition
            if (expr->getType() == "Var")
            {
                auto& token = std::static_pointer_cast<Var>(expr)->name;
                return std::make_shared<PostFixExpression>(expr, op, token);
            }
            // Todo need to be able to parse: var a = 5, b = 4; var c = a--b -> a - (-b);
            /*
            else
            {
                skipEOL();
                auto rExpr = call();

                Token token, token2;

                switch (op.type)
                {
                case TokenType::INCREMENT:
                    token = Token(TokenType::PLUS, "+", op.line, op.column);
                    return std::make_shared<BinaryExpression>(expr, op, rExpr);
                    break;

                case TokenType::DECREMENT:
                    token = Token(TokenType::MINUS, "-", op.line, op.column);
                    token2 = Token(TokenType::MINUS, "-", op.line + 1, op.column);

                    rExpr = std::make_shared<UnaryExpression>(rExpr, token2);

                    return std::make_shared<BinaryExpression>(expr, op, rExpr);
                    break;

                default:
                    throw ParseException(tokenList.front(), "Unexpected Token in post fix operator");
                    break;
                }
                
            }
            */

            throw ParseException(tokenList.front(), "Expected Variable before post fix operator");
        }

        while (true)
        {
            if (match(TokenType::PENTER))
            {
                skipEOL();
                expr = finishCall(expr);
            }
            else if (match(TokenType::POINT))
            {
                skipEOL();
                auto token = consume("Expect property name after '.'.", TokenType::EXPRESSION);
                
                expr = std::make_shared<Get>(expr, token);
            }
            else if (match(TokenType::CENTER))
            {
                auto token = previousToken;

                expr = std::make_shared<Get>(expr, Token{TokenType::EXPRESSION, "at", token.line, token.column});

                skipEOL();
                std::queue<ExprPtr> argument;
                argument.push(expression());

                skipEOL();
                consume("Expect ']' after an Array subscript call.", TokenType::CCLOSE);

                expr = std::make_shared<CallExpression>(expr, token, argument);
            }
            else
            {
                break;
            }
        }

        return expr;
    }

    ExprPtr Parser::primary()
    {
        LOG_THIS_MEMBER(DOM);

        if (match(TokenType::KEYTRUE)) return std::make_shared<Atom>(true);
        if (match(TokenType::KEYFALSE)) return std::make_shared<Atom>(false);

        //TODO see if it is relevant to create a null expression
        //if(match(TokenType::NULL)) return std::make_shared<Atom>(nullptr)

        if (match(TokenType::NUMBER)) return std::make_shared<Atom>(std::stoi(previousToken.text));
        if (match(TokenType::FLOAT)) return std::make_shared<Atom>(std::stof(previousToken.text));
        if (match(TokenType::STRING)) return std::make_shared<Atom>(previousToken.text);

        if (match(TokenType::TOK_THIS)) return std::make_shared<This>(previousToken);

        if (match(TokenType::EXPRESSION)) return std::make_shared<Var>(previousToken);

        if (match(TokenType::PENTER))
        {
            skipEOL();
            auto expr = expression();
            skipEOL();

            consume("Expect ')' after expression.", TokenType::PCLOSE);

            return std::make_shared<CompoundAtom>(expr);
        }

        if (match(TokenType::CENTER)) return finishList();

        throw ParseException(tokenList.front(), "Expected expression");
    }

    StatementPtr Parser::declaration()
    {
        LOG_THIS_MEMBER(DOM);

        try
        {
            if (match(TokenType::TOK_VAR))   return varDeclaration();
            if (match(TokenType::TOK_FUN))   return funDeclaration();
            if (match(TokenType::TOK_CLASS)) return classDeclaration();

            return statement();
        }
        catch (const std::exception& e)
        {
            synchronize();

            LOG_ERROR(DOM, e.what());
            errorEncountered = true;

            return nullptr;
        }
    }

    StatementPtr Parser::statement()
    {
        LOG_THIS_MEMBER(DOM);

        if (match(TokenType::TOK_RETURN))    return returnStatement();
        if (match(TokenType::TOK_FOR))       return forStatement();
        if (match(TokenType::TOK_IF))        return ifStatement();
        if (match(TokenType::TOK_WHILE))     return whileStatement();
        if (match(TokenType::BENTER))    return blockDeclaration();
        if (match(TokenType::TOK_IMPORT))    return importStatement();

        return expressionStatement();
    }

    std::shared_ptr<FunctionStatement> Parser::makeFun(const std::string& kind)
    {
        LOG_THIS_MEMBER(DOM);

        auto name = consume("Expected " + kind + " name", TokenType::EXPRESSION);

        skipEOL();
        consume("Expect '(' after a function declaration.", TokenType::PENTER);
        skipEOL();

        std::queue<ExprPtr> parameters;

        if (not check(TokenType::PCLOSE))
        {
            do
            {
                parameters.push(expression());
                skipEOL();
            } while (match(TokenType::COMMA));
        }

        skipEOL();
        Token paren = consume("Expect ')' after parameters", TokenType::PCLOSE);

        StatementPtr body = nullptr;

        if (match(TokenType::EOL))
        {
            skipEOL();

            LOG_INFO(DOM, "Declaration on the next line");

            body = declaration();

            // if (match(TokenType::BENTER))
            //     body = blockDeclaration();
            // else
            //     return std::make_shared<FunctionStatement>(name, parameters, body);
        }
        else
        {
            LOG_INFO(DOM, "Declaration on the same line");

            body = declaration();
        }

        consume("Expected ; or end of line after a variable definition", TokenType::END, TokenType::EOL);

        return std::make_shared<FunctionStatement>(name, parameters, body);
    }

    StatementPtr Parser::varDeclaration()
    {
        LOG_THIS_MEMBER(DOM);

        auto name = consume("Expected variable name", TokenType::EXPRESSION);

        ExprPtr initializer = nullptr;
        if (match(TokenType::EQUAL))
        {
            skipEOL();
            initializer = expression();
        }

        consume("Expected ; or end of line after a variable definition", TokenType::END, TokenType::EOL);
        return std::make_shared<VariableStatement>(name, initializer);
    }

    StatementPtr Parser::funDeclaration(const std::string& kind)
    {
        return makeFun(kind);
    }

    StatementPtr Parser::classDeclaration()
    {
        LOG_THIS_MEMBER(DOM);

        auto name = consume("Expected class name", TokenType::EXPRESSION);

        std::queue<std::shared_ptr<FunctionStatement>> body;

        if (match(TokenType::EOL))
        {
            skipEOL();

            if (not check(TokenType::BENTER))
                return std::make_shared<ClassStatement>(name, body);
        }

        consume("Expect '{' before class body", TokenType::BENTER);
        skipEOL();

        while (not isAtEnd() and not check(TokenType::BCLOSE))
        {
            body.push(makeFun("method"));
            skipEOL();
        }

        consume("Expect '}' after class body", TokenType::BCLOSE);

        return std::make_shared<ClassStatement>(name, body);
    }

    StatementPtr Parser::forStatement()
    {
        LOG_THIS_MEMBER(DOM);

        skipEOL();
        const auto& token = consume("Expect '(' after 'for'.", TokenType::PENTER);
        skipEOL();

        StatementPtr initializer;

        bool rangeBased = false;
        Token name;
        
        if (match(TokenType::END))
            initializer = nullptr;
        else if (match(TokenType::TOK_VAR))
        {
            name = consume("Expected variable name", TokenType::EXPRESSION);

            ExprPtr init = nullptr;
            if (match(TokenType::EQUAL))
            {
                skipEOL();
                init = expression();
            }

            initializer = std::make_shared<VariableStatement>(name, init);

            if (check(TokenType::DPOINT))
            {
                advance();

                rangeBased = true;
            }
            else
                consume("Expected ; or end of line after a variable definition", TokenType::END, TokenType::EOL);
        }
        else
            initializer = expressionStatement();

        if (rangeBased)
        {
            skipEOL();

            ExprPtr range = expression();

            skipEOL();

            consume("Expect ')' after 'for'.", TokenType::PCLOSE);
            skipEOL();

            std::queue<ExprPtr> emptyQueue;

            // Create a token for the iterator
            auto it = Token{TokenType::EXPRESSION, "__it", token.line, token.column};

            // Get the iterator from the rhs of the : of the for statement
            ExprPtr itExpr = std::make_shared<Get>(range, Token{TokenType::EXPRESSION, "it", token.line, token.column});
            itExpr = std::make_shared<CallExpression>(itExpr, it, emptyQueue);    

            // Store the it in a variable named "__it"
            auto itStmt = std::make_shared<VariableStatement>(it, itExpr);

            // Create a var to get all the other function 
            auto itVar = std::make_shared<Var>(it);

            // Grab the function "begin" in the iterator and store it in a var name "__begin" 
            auto begin = Token{TokenType::EXPRESSION, "__begin", token.line, token.column};
            ExprPtr beginExpr = std::make_shared<Get>(itVar, Token{TokenType::EXPRESSION, "begin", token.line, token.column});
            beginExpr = std::make_shared<CallExpression>(beginExpr, it, emptyQueue);            

            auto beginStmt = std::make_shared<VariableStatement>(begin, beginExpr);

            auto beginVar = std::make_shared<Var>(begin);

            // Grab the function "current" to get the current value of the iterator
            ExprPtr currentExpr = std::make_shared<Get>(itVar, Token{TokenType::EXPRESSION, "current", token.line, token.column});
            currentExpr = std::make_shared<CallExpression>(currentExpr, it, emptyQueue);

            // Grab the function "next" to advance the iterator 
            ExprPtr nextExpr = std::make_shared<Get>(itVar, Token{TokenType::EXPRESSION, "next", token.line, token.column});
            nextExpr = std::make_shared<CallExpression>(nextExpr, it, emptyQueue);

            // Grab the function "end" in the iterator and store it in a var name "__end" 
            auto end = Token{TokenType::EXPRESSION, "__end", token.line, token.column};

            ExprPtr endExpr = std::make_shared<Get>(itVar, Token{TokenType::EXPRESSION, "end", token.line, token.column});
            endExpr = std::make_shared<CallExpression>(endExpr, it, emptyQueue);

            auto endStmt = std::make_shared<VariableStatement>(end, endExpr);

            auto endVar = std::make_shared<Var>(end);

            // And create an expression that assign the value of the iterator (current) to this variable
            auto& varToken = std::static_pointer_cast<VariableStatement>(initializer)->name;
            auto varExpr = std::make_shared<Assign>(varToken, currentExpr);

            // Grab the name of the variable use in the lhs of the : of the for statement
            auto var = std::make_shared<Var>(varToken);

            // Create the condition expression for the while loop (current != end)
            auto conditionToken = Token{TokenType::NOTEQUAL, "!=", token.line, token.column + 5};
            auto condition = std::make_shared<BinaryExpression>(var, conditionToken, endVar);

            // Desugaring of 'range based for' into a basic while loop

            StatementPtr body = statement();

            // Body of the while loop
            {
                std::queue<StatementPtr> q;
                // Execute body
                q.push(body);
                // Advance the iterator
                q.push(std::make_shared<ExpressionStatement>(nextExpr));
                // Set the named var to the current value of the iterator
                q.push(std::make_shared<ExpressionStatement>(varExpr));

                body = std::make_shared<BlockStatement>(q);
            }

            // While loop
            body = std::make_shared<WhileStatement>(condition, body);

            // Init state of the while loop
            {
                std::queue<StatementPtr> q;
                // Init the it, begin and end variable (__it, __begin, __end)
                q.push(itStmt);
                q.push(beginStmt);
                q.push(endStmt);
                
                // Init the named variable
                q.push(initializer);
                q.push(std::make_shared<ExpressionStatement>(varExpr));

                q.push(body);

                body = std::make_shared<BlockStatement>(q);
            }
                
            return body;
        }
        else
        {
            skipEOL();
            ExprPtr condition = nullptr;
            skipEOL();

            if (not check(TokenType::END))
            {
                condition = expression();
                skipEOL();
            }

            consume("Expect ';' after loop condition.", TokenType::END);
            skipEOL();

            ExprPtr increment = nullptr;
            skipEOL();

            if (not check(TokenType::PCLOSE))
            {
                increment = expression();
                skipEOL();
            }

            consume("Expect ')' after 'for'.", TokenType::PCLOSE);
            skipEOL();

            // Desugaring of 'for' into a basic while loop

            StatementPtr body = statement();

            if (increment)
            {
                std::queue<StatementPtr> q;
                q.push(body);
                q.push(std::make_shared<ExpressionStatement>(increment));

                body = std::make_shared<BlockStatement>(q);
            }

            if (condition == nullptr)
                condition = std::make_shared<Atom>(true);

            body = std::make_shared<WhileStatement>(condition, body);

            if (initializer)
            {
                std::queue<StatementPtr> q;
                q.push(initializer);
                q.push(body);

                body = std::make_shared<BlockStatement>(q);
            }
                
            return body;
        }

        LOG_ERROR(DOM, "Should never reach here");
        return nullptr;
    }

    StatementPtr Parser::ifStatement()
    {
        LOG_THIS_MEMBER(DOM);

        skipEOL();
        consume("Expect '(' after 'if'.", TokenType::PENTER);
        skipEOL();

        ExprPtr condition = expression();

        skipEOL();
        consume("Expect ')' after 'if'.", TokenType::PCLOSE);
        skipEOL();
        
        StatementPtr thenBranch = statement();
        StatementPtr elseBranch = nullptr;

        skipEOL();

        if (match(TokenType::TOK_ELSE))
            elseBranch = statement();

        return std::make_shared<IfStatement>(condition, thenBranch, elseBranch);
    }

    StatementPtr Parser::whileStatement()
    {
        LOG_THIS_MEMBER(DOM);

        skipEOL();
        consume("Expect '(' after 'if'.", TokenType::PENTER);
        skipEOL();

        ExprPtr condition = expression();

        skipEOL();
        consume("Expect ')' after 'if'.", TokenType::PCLOSE);
        skipEOL();

        StatementPtr body = statement();

        return std::make_shared<WhileStatement>(condition, body);
    }

    StatementPtr Parser::returnStatement()
    {
        LOG_THIS_MEMBER(DOM);

        auto token = previousToken;

        ExprPtr returnValue = expression();

        consume("Expected ; or end of line after a return statement.", TokenType::END, TokenType::EOL);
        return std::make_shared<ReturnStatement>(token, returnValue);
    }

    StatementPtr Parser::blockDeclaration()
    {
        return std::make_shared<BlockStatement>(block());
    }

    StatementPtr Parser::importStatement()
    {
        LOG_THIS_MEMBER(DOM);

        auto token = previousToken;

        ExprPtr importName = nullptr;

        std::queue<ExprPtr> imports;

        imports.push(expression());

        if (not check(TokenType::END, TokenType::EOL))
        {
            bool multipleImport = false;

            while (match(TokenType::COMMA))
            {
                multipleImport = true;

                skipEOL();
                imports.push(expression());
            }

            if (not multipleImport and match(TokenType::TOK_AS))
            {
                importName = expression();
            }
        }

        consume("Expected ; or end of line after an import statement.", TokenType::END, TokenType::EOL);
        return std::make_shared<ImportStatement>(token, imports, importName);
    }

    StatementPtr Parser::expressionStatement()
    {
        LOG_THIS_MEMBER(DOM);

        if (isAtEnd())
            return nullptr;

        if (match(TokenType::END, TokenType::EOL))
            return declaration();

        ExprPtr expr = expression();

        consume("Expect a ; or an end of line at the end of an expression", TokenType::END, TokenType::EOL);
        return std::make_shared<ExpressionStatement>(expr);
    }

}
