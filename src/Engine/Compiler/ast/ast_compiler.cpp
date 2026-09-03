#include "stdafx.h"

#include "ast_compiler.h"

#include "../vm.h"
#include "../chunk.h"

#include "Interpreter/parser.h"

#include "logger.h"

// Every emission sequence in this file mirrors a case of the Pratt front-end
// (cparser.cpp) and must produce semantically identical bytecode; when
// changing anything here, check the corresponding CParser emitter first and
// keep the differential testbench green.

namespace pg
{
    namespace
    {
        const char* DOM = "AstCompiler";
    }

    Value AstCompiler::compile(std::queue<Token> tokens)
    {
        Parser parser(tokens);

        auto statements = parser.parse();

        if (parser.hasError())
        {
            LOG_ERROR(DOM, "AST parsing failed, aborting compilation");
            hadError = true;
            return 0x0;
        }

        // AST optimization passes: structure-level transformations only this
        // front-end can do (loop-invariant hoisting; later entity-loop and
        // component-access lowering). O0 / --no-opt disables them together
        // with the bytecode passes.
        if (vm->enableOptimizations)
        {
            vm->astPassManager.runAllPasses(vm, statements);
        }

        return compileAst(std::move(statements));
    }

    Value AstCompiler::compileAst(std::queue<StatementPtr> statements)
    {
        root.initCompiler(FunctionType::TYPE_SCRIPT);
        root.parser.setCompiler(&root);

        try
        {
            while (not statements.empty() and not hasError())
            {
                auto stmt = statements.front();
                statements.pop();

                if (stmt)
                    stmt->accept(this);
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(DOM, "Exception during bytecode emission: " << e.what());
            hadError = true;
        }

        if (hasError())
        {
            // Same cleanup as Compiler::compile: never leave the static
            // compiler chain pointing at a soon-destroyed compiler
            Compiler::current = root.enclosing;
            return 0x0;
        }

        auto func = root.endCompiler();

        root.parser.allocatedFunction.push_back(func);

        return func;
    }

    void AstCompiler::error(const Token& token, const std::string& message)
    {
        hadError = true;
        root.parser.errorAt(token, message);
    }

    int AstCompiler::internString(const std::string& name, const Token& token)
    {
        auto& chunk = Compiler::current->getCurrentChunk();

        for (size_t i = 0; i < chunk.constantStrings.size(); i++)
        {
            if (chunk.constantStrings[i] == name)
                return static_cast<int>(i);
        }

        if (chunk.constantStrings.size() >= 256)
        {
            error(token, "Too many constant strings in chunk.");
            return -1;
        }

        chunk.constantStrings.push_back(name);

        return static_cast<int>(chunk.constantStrings.size() - 1);
    }

    void AstCompiler::emitVariableGet(const Token& name)
    {
        int arg = Compiler::current->resolveLocal(name);

        if (arg != -1)
        {
            root.parser.writeByte(OpCode::OP_Get_Local);
            root.parser.writeByte(static_cast<uint8_t>(arg));
        }
        else if ((arg = Compiler::current->resolveUpvalue(name)) != -1)
        {
            root.parser.writeByte(OpCode::OP_Get_Upvalue);
            root.parser.writeByte(static_cast<uint8_t>(arg));
        }
        else
        {
            root.parser.writeConstant(name.text);
            root.parser.writeByte(OpCode::OP_Get_Global);
        }
    }

    void AstCompiler::compileFunction(FunctionType type, const std::string& name, std::queue<ExprPtr> parameters, StatementPtr body, const Token& token)
    {
        Compiler compiler(vm);

        compiler.initCompiler(type, name);
        compiler.beginScope();

        setLine(token);

        while (not parameters.empty())
        {
            auto param = parameters.front();
            parameters.pop();

            auto function = vm->asFunction(Compiler::current->currentFunction);
            function->arity++;

            if (function->arity > 255)
                error(token, "Can't have more than 255 parameters.");

            if (param->getType() != "Var")
            {
                error(token, "Invalid parameter in function declaration.");
                continue;
            }

            Token varName = std::static_pointer_cast<Var>(param)->name;

            Compiler::current->addLocal(varName);
            Compiler::current->markInitialized();
        }

        if (body)
        {
            if (body->getType() == "BlockStatement")
            {
                // Mirror CParser::parseFunction: the body block shares the
                // function scope opened above (no extra begin/endScope and no
                // OP_Pop cleanup - OP_Return unwinds the whole frame)
                auto block = std::static_pointer_cast<BlockStatement>(body);
                auto statements = block->statements;

                while (not statements.empty())
                {
                    statements.front()->accept(this);
                    statements.pop();
                }
            }
            else
            {
                // Single-statement body (only producible by the AST parser)
                body->accept(this);
            }
        }

        auto function = compiler.endCompiler();

        root.parser.allocatedFunction.push_back(function);

        setLine(token);
        root.parser.writeByte(OpCode::OP_Closure);
        root.parser.writeByte(Compiler::current->getCurrentChunk().addConstantIndex(function));

        for (int i = 0; i < vm->asFunction(function)->upvalueCount; i++)
        {
            root.parser.writeByte(compiler.upvalues[i].isLocal ? 1 : 0);
            root.parser.writeByte(compiler.upvalues[i].index);
        }
    }

    void AstCompiler::emitLoopScopeUnwind()
    {
        LoopContext& loop = Compiler::current->loopContexts.back();

        for (int i = Compiler::current->localCount - 1; i >= 0 and Compiler::current->locals[i].depth > loop.scopeDepth; i--)
        {
            if (Compiler::current->locals[i].isCaptured)
                root.parser.writeByte(OpCode::OP_Close_Upvalue);
            else
                root.parser.writeByte(OpCode::OP_Pop);
        }
    }

    // ------------------------------------------------------------------
    // Expressions
    // ------------------------------------------------------------------

    std::shared_ptr<Valuable> AstCompiler::visit(BinaryExpression* expr)
    {
        expr->leftExpr->accept(this);
        expr->rightExpr->accept(this);

        setLine(expr->op);

        switch (expr->op.type)
        {
            case TokenType::PLUS:
                root.parser.writeByte(OpCode::OP_Add);
                break;

            case TokenType::MINUS:
                root.parser.writeByte(OpCode::OP_Subtract);
                break;

            case TokenType::STAR:
                root.parser.writeByte(OpCode::OP_Multiply);
                break;

            case TokenType::SLASH:
                root.parser.writeByte(OpCode::OP_Divide);
                break;

            case TokenType::MOD:
                root.parser.writeByte(OpCode::OP_Modulo);
                break;

            case TokenType::EQUALEQUAL:
                root.parser.writeByte(OpCode::OP_Equal);
                break;

            case TokenType::NOTEQUAL:
                root.parser.writeByte(OpCode::OP_NotEqual);
                break;

            case TokenType::INF:
                root.parser.writeByte(OpCode::OP_Less);
                break;

            case TokenType::INFEQUAL:
                root.parser.writeByte(OpCode::OP_LessEqual);
                break;

            case TokenType::SUP:
                root.parser.writeByte(OpCode::OP_Greater);
                break;

            case TokenType::SUPEQUAL:
                root.parser.writeByte(OpCode::OP_GreaterEqual);
                break;

            default:
                error(expr->op, "Unknown binary operator '" + expr->op.text + "'.");
                break;
        }

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(LogicExpression* expr)
    {
        expr->leftExpr->accept(this);

        setLine(expr->op);

        if (expr->op.type == TokenType::LOGICAND)
        {
            // Mirror andOp: short-circuit when the left operand is false
            int endJump = root.parser.emitJump(OpCode::OP_Long_Jump_If_False);

            root.parser.writeByte(OpCode::OP_Pop);
            expr->rightExpr->accept(this);

            root.parser.patchJump(endJump);
        }
        else if (expr->op.type == TokenType::LOGICOR)
        {
            // Mirror orOp: short-circuit when the left operand is true
            int elseJump = root.parser.emitJump(OpCode::OP_Long_Jump_If_False);
            int endJump = root.parser.emitJump(OpCode::OP_Long_Jump);

            root.parser.patchJump(elseJump);
            root.parser.writeByte(OpCode::OP_Pop);

            expr->rightExpr->accept(this);
            root.parser.patchJump(endJump);
        }
        else
        {
            error(expr->op, "Unknown logic operator '" + expr->op.text + "'.");
        }

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(UnaryExpression* expr)
    {
        expr->expr->accept(this);

        setLine(expr->op);

        switch (expr->op.type)
        {
            case TokenType::MINUS:
                root.parser.writeByte(OpCode::OP_Negate);
                break;

            case TokenType::NOT:
                root.parser.writeByte(OpCode::OP_Not);
                break;

            default:
                error(expr->op, "Unknown unary operator '" + expr->op.text + "'.");
                break;
        }

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(PreFixExpression* expr)
    {
        // Mirror incrementOp/decrementOp: prefix ++/-- only supports plain
        // variables (locals resolve to a slot, everything else goes global by
        // name, matching the Pratt front-end which has no upvalue path here)
        setLine(expr->op);

        if (expr->expr->getType() != "Var")
        {
            error(expr->op, "Expected variable after prefix operator.");
            return nullptr;
        }

        Token varToken = std::static_pointer_cast<Var>(expr->expr)->name;

        int arg = Compiler::current->resolveLocal(varToken);

        OpCode op;
        ElementType identifier;

        if (arg != -1)
        {
            op = expr->op.type == TokenType::INCREMENT ? OpCode::OP_Incr_Local : OpCode::OP_Decr_Local;
            identifier = ElementType(arg);
        }
        else
        {
            op = expr->op.type == TokenType::INCREMENT ? OpCode::OP_Incr_Global : OpCode::OP_Decr_Global;
            identifier = ElementType(varToken.text);
        }

        root.parser.writeConstant(identifier);
        root.parser.writeByte(op);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(PostFixExpression* expr)
    {
        // Mirror the postfix ++/-- handling inside variable(): push the old
        // value, then emit the post-increment op with the identifier operand
        setLine(expr->op);

        if (expr->expr->getType() != "Var")
        {
            error(expr->op, "Expected variable before postfix operator.");
            return nullptr;
        }

        Token varToken = std::static_pointer_cast<Var>(expr->expr)->name;

        const bool increment = expr->op.type == TokenType::INCREMENT;

        int arg = Compiler::current->resolveLocal(varToken);

        if (arg != -1)
        {
            root.parser.writeByte(OpCode::OP_Get_Local);
            root.parser.writeByte(static_cast<uint8_t>(arg));
            root.parser.writeConstant(ElementType(arg));
            root.parser.writeByte(increment ? OpCode::OP_Post_Incr_Local : OpCode::OP_Post_Decr_Local);
        }
        else if ((arg = Compiler::current->resolveUpvalue(varToken)) != -1)
        {
            // Bug-compatible with variable(): upvalues reuse the local
            // post-increment op with the upvalue index as operand
            root.parser.writeByte(OpCode::OP_Get_Upvalue);
            root.parser.writeByte(static_cast<uint8_t>(arg));
            root.parser.writeConstant(ElementType(arg));
            root.parser.writeByte(increment ? OpCode::OP_Post_Incr_Local : OpCode::OP_Post_Decr_Local);
        }
        else
        {
            root.parser.writeConstant(varToken.text);
            root.parser.writeByte(OpCode::OP_Get_Global);
            root.parser.writeConstant(varToken.text);
            root.parser.writeByte(increment ? OpCode::OP_Post_Incr_Global : OpCode::OP_Post_Decr_Global);
        }

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(CompoundAtom* expr)
    {
        expr->expr->accept(this);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(Atom* expr)
    {
        if (expr->value.type == UnionType::BOOL)
            root.parser.writeByte(expr->value.isTrue() ? OpCode::OP_True : OpCode::OP_False);
        else
            root.parser.writeConstant(expr->value);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(List* expr)
    {
        // Mirror createTable/createTableBrace: for each entry push the value
        // then its key, then build. Braces or any explicit 'key:' entry make
        // it a table, otherwise it is a vector.
        setLine(expr->squareBracket);

        auto entries = expr->entries;

        if (entries.size() > 255)
        {
            error(expr->squareBracket, "Too many entries in a list literal (max 255).");
            return nullptr;
        }

        uint8_t count = 0;

        while (not entries.empty())
        {
            auto entry = entries.front();
            entries.pop();

            entry.value->accept(this);

            // Keys are constants on the Pratt path: implicit indices are int
            // Atoms; explicit keys always emit as the raw token text (a
            // string constant), mirroring createTable's writeConstant(first.text)
            if (entry.explicitKey)
            {
                if (entry.key->getType() == "Atom" or entry.key->getType() == "Var")
                {
                    root.parser.writeConstant(entry.key->getName());
                }
                else
                {
                    error(expr->squareBracket, "Unsupported table key expression.");
                    return nullptr;
                }
            }
            else
            {
                root.parser.writeConstant(std::static_pointer_cast<Atom>(entry.key)->value);
            }

            count++;
        }

        const bool buildTable = expr->braceForm or expr->hasExplicitKeys;

        root.parser.writeByte(buildTable ? OpCode::OP_Build_Table : OpCode::OP_Build_Vector);
        root.parser.writeByte(count);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(This* expr)
    {
        if (Compiler::current->currentClass == nullptr)
        {
            error(expr->name, "Can't use 'this' outside of a class.");
            return nullptr;
        }

        setLine(expr->name);
        emitVariableGet(expr->name);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(Var* expr)
    {
        setLine(expr->name);
        emitVariableGet(expr->name);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(Assign* expr)
    {
        setLine(expr->name);

        int arg = Compiler::current->resolveLocal(expr->name);

        OpCode setOp = OpCode::OP_Set_Local;

        if (arg == -1 and (arg = Compiler::current->resolveUpvalue(expr->name)) != -1)
            setOp = OpCode::OP_Set_Upvalue;

        if (arg != -1)
        {
            expr->expr->accept(this);

            setLine(expr->name);
            root.parser.writeByte(setOp);
            root.parser.writeByte(static_cast<uint8_t>(arg));
        }
        else
        {
            expr->expr->accept(this);

            setLine(expr->name);
            root.parser.writeConstant(expr->name.text);
            root.parser.writeByte(OpCode::OP_Set_Global);
        }

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(CallExpression* expr)
    {
        setLine(expr->paren);

        auto emitArgs = [this](std::queue<ExprPtr> args, const Token& paren) -> int
        {
            int argCount = 0;

            while (not args.empty())
            {
                if (argCount == 255)
                {
                    error(paren, "Can't have more than 255 arguments.");
                    return -1;
                }

                args.front()->accept(this);
                args.pop();
                argCount++;
            }

            return argCount;
        };

        // Method call on a property access compiles to OP_Invoke (mirror dot())
        if (expr->caller->getType() == "Get")
        {
            auto getExpr = std::static_pointer_cast<Get>(expr->caller);

            getExpr->object->accept(this);

            // Intern before emitting the arguments to keep the interning
            // order identical to the Pratt front-end
            int stringIndex = internString(getExpr->name.text, getExpr->name);

            if (stringIndex < 0)
                return nullptr;

            int argCount = emitArgs(expr->args, expr->paren);

            if (argCount < 0)
                return nullptr;

            setLine(expr->paren);
            root.parser.emitBytes(OpCode::OP_Invoke, static_cast<uint8_t>(stringIndex));
            root.parser.writeByte(static_cast<uint8_t>(argCount));
        }
        else
        {
            expr->caller->accept(this);

            int argCount = emitArgs(expr->args, expr->paren);

            if (argCount < 0)
                return nullptr;

            setLine(expr->paren);
            root.parser.emitBytes(OpCode::OP_Call, static_cast<uint8_t>(argCount));
        }

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(Get* expr)
    {
        expr->object->accept(this);

        setLine(expr->name);

        int stringIndex = internString(expr->name.text, expr->name);

        if (stringIndex < 0)
            return nullptr;

        root.parser.writeByte(OpCode::OP_Get_Property);
        root.parser.writeByte(static_cast<uint8_t>(stringIndex));

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(Set* expr)
    {
        expr->object->accept(this);

        setLine(expr->name);

        // Intern before the value expression, mirroring dot()
        int stringIndex = internString(expr->name.text, expr->name);

        if (stringIndex < 0)
            return nullptr;

        expr->value->accept(this);

        setLine(expr->name);
        root.parser.writeByte(OpCode::OP_Set_Property);
        root.parser.writeByte(static_cast<uint8_t>(stringIndex));

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(AnonymousFunction* expr)
    {
        // The Pratt front-end names anonymous functions after the 'fun'
        // keyword token it just consumed
        compileFunction(FunctionType::TYPE_FUNCTION, expr->token.text, expr->parameters, expr->body, expr->token);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(IndexGet* expr)
    {
        expr->object->accept(this);
        expr->index->accept(this);

        setLine(expr->bracket);
        root.parser.writeByte(OpCode::OP_Get_Index);

        return nullptr;
    }

    std::shared_ptr<Valuable> AstCompiler::visit(IndexSet* expr)
    {
        expr->object->accept(this);
        expr->index->accept(this);
        expr->value->accept(this);

        setLine(expr->bracket);
        root.parser.writeByte(OpCode::OP_Set_Index);

        return nullptr;
    }

    // ------------------------------------------------------------------
    // Statements
    // ------------------------------------------------------------------

    void AstCompiler::visitStatement(ExpressionStatement* stmt)
    {
        stmt->expr->accept(this);

        root.parser.writeByte(OpCode::OP_Pop);
    }

    void AstCompiler::visitStatement(VariableStatement* stmt)
    {
        setLine(stmt->name);

        if (Compiler::current->scopeDepth > 0)
            Compiler::current->addLocal(stmt->name);

        if (stmt->expr)
            stmt->expr->accept(this);
        else
            root.parser.writeConstant(ElementType()); // Default initialize to 0

        if (Compiler::current->scopeDepth > 0)
        {
            Compiler::current->markInitialized();
            return;
        }

        setLine(stmt->name);
        root.parser.writeConstant(stmt->name.text);
        root.parser.writeByte(OpCode::OP_Define_Global);
    }

    void AstCompiler::visitStatement(FunctionStatement* stmt)
    {
        setLine(stmt->name);

        if (Compiler::current->scopeDepth > 0)
            Compiler::current->addLocal(stmt->name);

        Compiler::current->markInitialized();

        compileFunction(FunctionType::TYPE_FUNCTION, stmt->name.text, stmt->parameters, stmt->body, stmt->name);

        if (Compiler::current->scopeDepth == 0)
        {
            setLine(stmt->name);
            root.parser.writeConstant(stmt->name.text);
            root.parser.writeByte(OpCode::OP_Define_Global);
        }
    }

    void AstCompiler::visitStatement(ClassStatement* stmt)
    {
        setLine(stmt->name);

        auto value = vm->createString(stmt->name.text);
        uint8_t nameConstant = Compiler::current->getCurrentChunk().addConstantIndex(value);
        root.parser.emitBytes(OpCode::OP_Class, nameConstant);

        if (Compiler::current->scopeDepth > 0)
        {
            Compiler::current->addLocal(stmt->name);
            Compiler::current->markInitialized();
        }
        else
        {
            root.parser.writeConstant(stmt->name.text);
            root.parser.writeByte(OpCode::OP_Define_Global);
        }

        Compiler::ClassCompiler classCompiler;
        classCompiler.enclosing = Compiler::current->currentClass;

        Compiler::current->currentClass = &classCompiler;

        root.parser.pushVariableInStack(stmt->name.text);

        auto methods = stmt->methods;

        while (not methods.empty())
        {
            auto method = methods.front();
            methods.pop();

            FunctionType type = method->name.text == "init" ?
                FunctionType::TYPE_INITIALIZER : FunctionType::TYPE_METHOD;

            compileFunction(type, method->name.text, method->parameters, method->body, method->name);

            setLine(method->name);
            root.parser.writeByte(OpCode::OP_Method);
            root.parser.writeByte(Compiler::current->getCurrentChunk().addConstantIndex(vm->createString(method->name.text)));
        }

        root.parser.writeByte(OpCode::OP_Pop);

        Compiler::current->currentClass = classCompiler.enclosing;
    }

    void AstCompiler::visitStatement(BlockStatement* stmt)
    {
        Compiler::current->beginScope();

        auto statements = stmt->statements;

        while (not statements.empty())
        {
            if (statements.front())
                statements.front()->accept(this);

            statements.pop();
        }

        Compiler::current->endScope();
    }

    void AstCompiler::visitStatement(IfStatement* stmt)
    {
        stmt->condition->accept(this);

        int thenJump = root.parser.emitJump(OpCode::OP_Long_Jump_If_False);
        root.parser.writeByte(OpCode::OP_Pop); // Pop the condition

        stmt->thenBranch->accept(this);

        int elseJump = root.parser.emitJump(OpCode::OP_Long_Jump);

        root.parser.patchJump(thenJump);
        root.parser.writeByte(OpCode::OP_Pop); // Pop the condition

        if (stmt->elseBranch)
            stmt->elseBranch->accept(this);

        root.parser.patchJump(elseJump);
    }

    void AstCompiler::visitStatement(WhileStatement* stmt)
    {
        int loopStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

        Compiler::current->loopContexts.push_back({
            loopStart,
            std::vector<int>(),
            Compiler::current->scopeDepth
        });

        stmt->condition->accept(this);

        int exitJump = root.parser.emitJump(OpCode::OP_Long_Jump_If_False);
        root.parser.writeByte(OpCode::OP_Pop); // Pop the condition

        stmt->body->accept(this);
        root.parser.emitLoop(loopStart);

        root.parser.patchJump(exitJump);
        root.parser.writeByte(OpCode::OP_Pop); // Pop the condition

        for (int offset : Compiler::current->loopContexts.back().breakJumps)
            root.parser.patchJump(offset);

        Compiler::current->loopContexts.pop_back();
    }

    void AstCompiler::visitStatement(ReturnStatement* stmt)
    {
        setLine(stmt->name);

        if (not stmt->value)
        {
            root.parser.emitReturn();
            return;
        }

        if (Compiler::current->currentType == FunctionType::TYPE_INITIALIZER)
        {
            error(stmt->name, "Can't return a value from an initializer.");
        }

        if (Compiler::current->currentType == FunctionType::TYPE_SCRIPT)
        {
            // Top-level return: discard the value and exit the script
            stmt->value->accept(this);

            setLine(stmt->name);
            root.parser.writeByte(OpCode::OP_Pop);
            root.parser.emitReturn();
        }
        else
        {
            stmt->value->accept(this);

            setLine(stmt->name);
            root.parser.writeByte(OpCode::OP_Return);
        }
    }

    void AstCompiler::visitStatement(ImportStatement* stmt)
    {
        setLine(stmt->name);

        if (stmt->isNamed)
        {
            error(stmt->name, "Named imports are not supported by the bytecode front-end.");
            return;
        }

        auto imports = stmt->imports;

        while (not imports.empty())
        {
            auto import = imports.front();
            imports.pop();

            std::string moduleName = import->getName();

            // Defensively strip quotes, mirroring getModuleName()
            if (moduleName.size() >= 2 and moduleName.front() == '"' and moduleName.back() == '"')
                moduleName = moduleName.substr(1, moduleName.size() - 2);

            if (not root.parser.parseImportFile(moduleName))
            {
                if (not vm->loadNativeModule(moduleName))
                {
                    error(stmt->name, "Failed to import module '" + moduleName + "': not found as file or native module");
                }
                else
                {
                    // Track native imports for bytecode caching
                    Compiler::current->getCurrentChunk().importedModules.push_back(moduleName);
                }
            }
        }
    }

    void AstCompiler::visitStatement(ForStatement* stmt)
    {
        Compiler::current->beginScope();

        // Initializer
        if (stmt->initializer)
        {
            if (stmt->initializer->getType() == "VariableStatement")
            {
                auto varStmt = std::static_pointer_cast<VariableStatement>(stmt->initializer);

                setLine(varStmt->name);
                Compiler::current->addLocal(varStmt->name);

                if (varStmt->expr)
                    varStmt->expr->accept(this);
                else
                    root.parser.writeByte(OpCode::OP_False); // Mirror CParser's for-loop default init

                Compiler::current->markInitialized();
            }
            else
            {
                // Expression initializer (emits its own OP_Pop)
                stmt->initializer->accept(this);
            }
        }

        int loopStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

        Compiler::current->loopContexts.push_back({
            loopStart,
            std::vector<int>(),
            Compiler::current->scopeDepth
        });

        // Condition
        int exitJump = -1;

        if (stmt->condition)
        {
            stmt->condition->accept(this);

            exitJump = root.parser.emitJump(OpCode::OP_Long_Jump_If_False);
            root.parser.writeByte(OpCode::OP_Pop); // Pop the condition
        }

        // Increment (runs after the body; jump-threaded like the Pratt front-end)
        if (stmt->increment)
        {
            int bodyJump = root.parser.emitJump(OpCode::OP_Long_Jump);
            int incrementStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

            stmt->increment->accept(this);
            root.parser.writeByte(OpCode::OP_Pop); // Pop the increment expression result

            root.parser.emitLoop(loopStart);
            loopStart = incrementStart;

            // Continue must jump to the increment, not the condition
            Compiler::current->loopContexts.back().loopStart = incrementStart;

            root.parser.patchJump(bodyJump);
        }

        // Body
        stmt->body->accept(this);
        root.parser.emitLoop(loopStart);

        if (exitJump != -1)
        {
            root.parser.patchJump(exitJump);
            root.parser.writeByte(OpCode::OP_Pop); // Pop the condition
        }

        for (int offset : Compiler::current->loopContexts.back().breakJumps)
            root.parser.patchJump(offset);

        Compiler::current->loopContexts.pop_back();

        Compiler::current->endScope();
    }

    void AstCompiler::visitStatement(ForInStatement* stmt)
    {
        // Mirror the Pratt front-end's for-in desugar:
        // {
        //     var __table = iterable;
        //     var __size = __table.size();
        //     for (var __i = 0; __i < __size; __i++) {
        //         var key = __table.at(__i);
        //         body;
        //     }
        // }
        Compiler::current->beginScope();

        setLine(stmt->varName);

        stmt->iterable->accept(this);
        // Stack: [table]

        Compiler::current->addLocal(Token(TokenType::EXPRESSION, "__table", stmt->varName.line, 0));
        Compiler::current->markInitialized();
        uint8_t tableSlot = static_cast<uint8_t>(Compiler::current->localCount - 1);

        root.parser.writeByte(OpCode::OP_Get_Local);
        root.parser.writeByte(tableSlot);
        root.parser.writeByte(OpCode::OP_Table_Size);
        // Stack: [__table, size]

        Compiler::current->addLocal(Token(TokenType::EXPRESSION, "__size", stmt->varName.line, 0));
        Compiler::current->markInitialized();
        uint8_t sizeSlot = static_cast<uint8_t>(Compiler::current->localCount - 1);

        root.parser.writeByte(OpCode::OP_Constant);
        root.parser.writeByte(Compiler::current->getCurrentChunk().addConstantIndex(makeIntValue(0)));
        // Stack: [__table, __size, 0]

        Compiler::current->addLocal(Token(TokenType::EXPRESSION, "__i", stmt->varName.line, 0));
        Compiler::current->markInitialized();
        uint8_t counterSlot = static_cast<uint8_t>(Compiler::current->localCount - 1);

        int loopStart = static_cast<int>(Compiler::current->getCurrentChunk().code.size());

        Compiler::current->loopContexts.push_back({
            loopStart,
            std::vector<int>(),
            Compiler::current->scopeDepth
        });

        // Condition: __i < __size
        root.parser.writeByte(OpCode::OP_Get_Local);
        root.parser.writeByte(counterSlot);
        root.parser.writeByte(OpCode::OP_Get_Local);
        root.parser.writeByte(sizeSlot);
        root.parser.writeByte(OpCode::OP_Less);

        int exitJump = root.parser.emitJump(OpCode::OP_Long_Jump_If_False);
        root.parser.writeByte(OpCode::OP_Pop); // Pop the condition result

        // Iteration scope holding the key variable
        Compiler::current->beginScope();

        root.parser.writeByte(OpCode::OP_Get_Local);
        root.parser.writeByte(tableSlot);
        root.parser.writeByte(OpCode::OP_Get_Local);
        root.parser.writeByte(counterSlot);
        root.parser.writeByte(OpCode::OP_Table_At);
        // Stack: [__table, __size, __i, key]

        Compiler::current->addLocal(stmt->varName);
        Compiler::current->markInitialized();

        stmt->body->accept(this);

        Compiler::current->endScope();
        // Stack: [__table, __size, __i]

        // __i++
        root.parser.writeByte(OpCode::OP_Get_Local);
        root.parser.writeByte(counterSlot);
        root.parser.writeByte(OpCode::OP_Constant);
        root.parser.writeByte(Compiler::current->getCurrentChunk().addConstantIndex(makeIntValue(1)));
        root.parser.writeByte(OpCode::OP_Add);
        root.parser.writeByte(OpCode::OP_Set_Local);
        root.parser.writeByte(counterSlot);
        root.parser.writeByte(OpCode::OP_Pop); // Pop the assignment result

        root.parser.emitLoop(loopStart);

        root.parser.patchJump(exitJump);
        root.parser.writeByte(OpCode::OP_Pop); // Pop the condition result

        for (int offset : Compiler::current->loopContexts.back().breakJumps)
            root.parser.patchJump(offset);

        Compiler::current->loopContexts.pop_back();

        Compiler::current->endScope();
    }

    void AstCompiler::visitStatement(BreakStatement* stmt)
    {
        setLine(stmt->token);

        if (Compiler::current->loopContexts.empty())
        {
            error(stmt->token, "Cannot use 'break' outside of a loop.");
            return;
        }

        emitLoopScopeUnwind();

        int jumpOffset = root.parser.emitJump(OpCode::OP_Long_Jump);
        Compiler::current->loopContexts.back().breakJumps.push_back(jumpOffset);
    }

    void AstCompiler::visitStatement(ContinueStatement* stmt)
    {
        setLine(stmt->token);

        if (Compiler::current->loopContexts.empty())
        {
            error(stmt->token, "Cannot use 'continue' outside of a loop.");
            return;
        }

        emitLoopScopeUnwind();

        root.parser.emitLoop(Compiler::current->loopContexts.back().loopStart);
    }

    void AstCompiler::visitStatement(DPrintStatement* stmt)
    {
        stmt->expr->accept(this);

        setLine(stmt->token);
        root.parser.writeByte(OpCode::OP_Debug_Print);
    }
}
