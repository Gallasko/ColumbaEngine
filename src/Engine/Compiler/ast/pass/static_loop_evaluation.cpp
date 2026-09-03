#include "stdafx.h"

#include "static_loop_evaluation.h"

#include "ast_analysis.h"

#include "Interpreter/expression.h"
#include "Interpreter/statement.h"

#include "logger.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace pg
{
    namespace
    {
        // ------------------------------------------------------------------
        // Compile-time values: int64/bool only, mirroring the VM's typed
        // kernels (ops_functors.inc). Floats/strings bail - their promotion
        // and precision rules are not worth replicating in v1.
        // ------------------------------------------------------------------

        struct CVal
        {
            enum class T : uint8_t { Int, Bool };

            T type = T::Int;
            int64_t i = 0;
            bool b = false;

            static CVal makeInt(int64_t v) { CVal c; c.type = T::Int; c.i = v; return c; }
            static CVal makeBool(bool v) { CVal c; c.type = T::Bool; c.b = v; return c; }

            /** VM truthiness (isValueTrue): bool -> value, int -> nonzero */
            bool isTrue() const { return type == T::Bool ? b : i != 0; }
        };

        /** Scoped compile-time environment (innermost scope at the back) */
        struct Env
        {
            std::vector<std::map<std::string, CVal>> scopes;

            CVal* find(const std::string& name)
            {
                for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                {
                    auto found = it->find(name);
                    if (found != it->end())
                        return &found->second;
                }

                return nullptr;
            }

            void declare(const std::string& name, const CVal& v)
            {
                scopes.back()[name] = v;
            }
        };

        // ------------------------------------------------------------------
        // Bounded AST interpreter
        // ------------------------------------------------------------------

        struct Evaluator
        {
            // Generous enough to fold a 100k-iteration benchmark loop, small
            // enough that a runaway loop can't stall compilation
            static constexpr int64_t kMaxSteps = 2'000'000;

            Env env;
            int64_t steps = 0;
            bool failed = false;

            enum class Flow { Normal, Break, Continue };

            void fail() { failed = true; }

            bool tick()
            {
                if (++steps > kMaxSteps)
                    failed = true;

                return not failed;
            }

            std::optional<CVal> evalExpr(const ExprPtr& e)
            {
                if (failed or not e or not tick())
                {
                    fail();
                    return std::nullopt;
                }

                const auto& type = e->getType();

                if (type == "Atom")
                {
                    const auto& v = std::static_pointer_cast<Atom>(e)->value;

                    if (v.type == UnionType::INT)
                        return CVal::makeInt(static_cast<int>(v));

                    if (v.type == UnionType::BOOL)
                        return CVal::makeBool(static_cast<bool>(v));

                    fail(); // floats/strings: out of scope for v1
                    return std::nullopt;
                }

                if (type == "Var")
                {
                    auto* v = env.find(std::static_pointer_cast<Var>(e)->name.text);

                    if (not v)
                    {
                        fail(); // unknown variable (global, engine-provided, ...)
                        return std::nullopt;
                    }

                    return *v;
                }

                if (type == "CompoundAtom")
                    return evalExpr(std::static_pointer_cast<CompoundAtom>(e)->expr);

                if (type == "UnaryExpression")
                {
                    auto u = std::static_pointer_cast<UnaryExpression>(e);

                    auto v = evalExpr(u->expr);
                    if (not v)
                        return std::nullopt;

                    if (u->op.type == TokenType::MINUS and v->type == CVal::T::Int)
                        return CVal::makeInt(-v->i);

                    if (u->op.type == TokenType::NOT)
                        return CVal::makeBool(not v->isTrue());

                    fail();
                    return std::nullopt;
                }

                if (type == "BinaryExpression")
                {
                    auto bin = std::static_pointer_cast<BinaryExpression>(e);

                    auto l = evalExpr(bin->leftExpr);
                    if (not l)
                        return std::nullopt;

                    auto r = evalExpr(bin->rightExpr);
                    if (not r)
                        return std::nullopt;

                    // ==/!= work across int and bool like the VM's generic
                    // equality on same-kind values; restrict to same type
                    if (bin->op.type == TokenType::EQUALEQUAL or bin->op.type == TokenType::NOTEQUAL)
                    {
                        if (l->type != r->type)
                        {
                            fail();
                            return std::nullopt;
                        }

                        bool eq = l->type == CVal::T::Int ? l->i == r->i : l->b == r->b;

                        return CVal::makeBool(bin->op.type == TokenType::EQUALEQUAL ? eq : not eq);
                    }

                    // Everything else is int-only (VM kernel fast paths)
                    if (l->type != CVal::T::Int or r->type != CVal::T::Int)
                    {
                        fail();
                        return std::nullopt;
                    }

                    switch (bin->op.type)
                    {
                        case TokenType::PLUS:     return CVal::makeInt(l->i + r->i);
                        case TokenType::MINUS:    return CVal::makeInt(l->i - r->i);
                        case TokenType::STAR:     return CVal::makeInt(l->i * r->i);

                        case TokenType::SLASH:
                            if (r->i == 0) { fail(); return std::nullopt; } // stays a runtime error
                            return CVal::makeInt(l->i / r->i);

                        case TokenType::MOD:
                            if (r->i == 0) { fail(); return std::nullopt; }
                            return CVal::makeInt(l->i % r->i);

                        case TokenType::INF:      return CVal::makeBool(l->i < r->i);
                        case TokenType::INFEQUAL: return CVal::makeBool(l->i <= r->i);
                        case TokenType::SUP:      return CVal::makeBool(l->i > r->i);
                        case TokenType::SUPEQUAL: return CVal::makeBool(l->i >= r->i);

                        default:
                            fail();
                            return std::nullopt;
                    }
                }

                if (type == "LogicExpression")
                {
                    auto lg = std::static_pointer_cast<LogicExpression>(e);

                    auto l = evalExpr(lg->leftExpr);
                    if (not l)
                        return std::nullopt;

                    // VM short-circuit yields the deciding operand's value
                    if (lg->op.type == TokenType::LOGICAND)
                        return l->isTrue() ? evalExpr(lg->rightExpr) : l;

                    if (lg->op.type == TokenType::LOGICOR)
                        return l->isTrue() ? l : evalExpr(lg->rightExpr);

                    fail();
                    return std::nullopt;
                }

                if (type == "Assign")
                {
                    auto a = std::static_pointer_cast<Assign>(e);

                    auto v = evalExpr(a->expr);
                    if (not v)
                        return std::nullopt;

                    auto* slot = env.find(a->name.text);

                    if (not slot)
                    {
                        fail(); // assignment to an untracked variable
                        return std::nullopt;
                    }

                    *slot = *v;

                    return v; // OP_Set_* leaves the value on the stack
                }

                if (type == "PreFixExpression" or type == "PostFixExpression")
                {
                    const bool prefix = type == "PreFixExpression";

                    const auto& name = prefix
                        ? std::static_pointer_cast<PreFixExpression>(e)->name
                        : std::static_pointer_cast<PostFixExpression>(e)->name;

                    const auto& op = prefix
                        ? std::static_pointer_cast<PreFixExpression>(e)->op
                        : std::static_pointer_cast<PostFixExpression>(e)->op;

                    auto* slot = env.find(name.text);

                    if (not slot or slot->type != CVal::T::Int)
                    {
                        fail();
                        return std::nullopt;
                    }

                    int64_t old = slot->i;
                    slot->i += op.type == TokenType::INCREMENT ? 1 : -1;

                    return CVal::makeInt(prefix ? slot->i : old);
                }

                fail(); // calls, property access, indexing, tables, this, ...
                return std::nullopt;
            }

            Flow execStmt(const StatementPtr& s)
            {
                if (failed or not tick())
                    return Flow::Normal;

                if (not s)
                    return Flow::Normal;

                const auto& type = s->getType();

                if (type == "ExpressionStatement")
                {
                    evalExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr);
                    return Flow::Normal;
                }

                if (type == "VariableStatement")
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);

                    CVal value = CVal::makeInt(0); // default init matches ElementType()

                    if (v->expr)
                    {
                        auto evaluated = evalExpr(v->expr);
                        if (not evaluated)
                            return Flow::Normal;

                        value = *evaluated;
                    }

                    env.declare(v->name.text, value);
                    return Flow::Normal;
                }

                if (type == "BlockStatement")
                {
                    env.scopes.push_back({});

                    auto statements = std::static_pointer_cast<BlockStatement>(s)->statements;
                    Flow flow = Flow::Normal;

                    while (not statements.empty() and not failed and flow == Flow::Normal)
                    {
                        flow = execStmt(statements.front());
                        statements.pop();
                    }

                    env.scopes.pop_back();
                    return flow;
                }

                if (type == "IfStatement")
                {
                    auto i = std::static_pointer_cast<IfStatement>(s);

                    auto cond = evalExpr(i->condition);
                    if (not cond)
                        return Flow::Normal;

                    if (cond->isTrue())
                        return execStmt(i->thenBranch);

                    if (i->elseBranch)
                        return execStmt(i->elseBranch);

                    return Flow::Normal;
                }

                if (type == "WhileStatement")
                {
                    auto w = std::static_pointer_cast<WhileStatement>(s);

                    while (not failed)
                    {
                        auto cond = evalExpr(w->condition);
                        if (not cond or not cond->isTrue())
                            break;

                        Flow flow = execStmt(w->body);

                        if (flow == Flow::Break)
                            break;
                    }

                    return Flow::Normal;
                }

                if (type == "ForStatement")
                {
                    auto fs = std::static_pointer_cast<ForStatement>(s);

                    env.scopes.push_back({});

                    if (fs->initializer)
                        execStmt(fs->initializer);

                    while (not failed)
                    {
                        if (fs->condition)
                        {
                            auto cond = evalExpr(fs->condition);
                            if (not cond or not cond->isTrue())
                                break;
                        }

                        Flow flow = execStmt(fs->body);

                        if (flow == Flow::Break)
                            break;

                        if (fs->increment)
                            evalExpr(fs->increment);
                    }

                    env.scopes.pop_back();
                    return Flow::Normal;
                }

                if (type == "BreakStatement")
                    return Flow::Break;

                if (type == "ContinueStatement")
                    return Flow::Continue;

                // for-in (tables), dprint (output side effect), return,
                // declarations of functions/classes, imports, ...: bail
                fail();
                return Flow::Normal;
            }
        };

        // ------------------------------------------------------------------
        // Pass driver: constant tracking through statement lists + loop folding
        // ------------------------------------------------------------------

        struct Ctx
        {
            bool changed = false;
        };

        bool fitsInt32(int64_t v)
        {
            return v >= std::numeric_limits<int32_t>::min() and v <= std::numeric_limits<int32_t>::max();
        }

        void processStatementList(std::queue<StatementPtr>& statements, Ctx& ctx);

        /** Recurse into every nested statement region with a fresh constant env */
        void recurseNested(const StatementPtr& s, Ctx& ctx)
        {
            if (not s)
                return;

            const auto& type = s->getType();

            if (type == "BlockStatement")
            {
                processStatementList(std::static_pointer_cast<BlockStatement>(s)->statements, ctx);
            }
            else if (type == "IfStatement")
            {
                auto i = std::static_pointer_cast<IfStatement>(s);
                recurseNested(i->thenBranch, ctx);
                recurseNested(i->elseBranch, ctx);
            }
            else if (type == "WhileStatement")
            {
                recurseNested(std::static_pointer_cast<WhileStatement>(s)->body, ctx);
            }
            else if (type == "ForStatement")
            {
                recurseNested(std::static_pointer_cast<ForStatement>(s)->body, ctx);
            }
            else if (type == "ForInStatement")
            {
                recurseNested(std::static_pointer_cast<ForInStatement>(s)->body, ctx);
            }
            else if (type == "FunctionStatement")
            {
                recurseNested(std::static_pointer_cast<FunctionStatement>(s)->body, ctx);
            }
            else if (type == "ClassStatement")
            {
                auto c = std::static_pointer_cast<ClassStatement>(s);

                auto methods = c->methods;
                while (not methods.empty())
                {
                    recurseNested(methods.front()->body, ctx);
                    methods.pop();
                }
            }
        }

        /**
         * Try to fold one loop given the tracked constants. On success
         * returns the replacement statements (final-value assignments) and
         * updates 'tracked'; on failure returns nullopt and the loop stays.
         */
        std::optional<std::vector<StatementPtr>> foldLoop(const StatementPtr& s, std::map<std::string, CVal>& tracked)
        {
            Evaluator eval;
            eval.env.scopes.push_back(tracked);

            eval.execStmt(s);

            if (eval.failed)
                return std::nullopt;

            const auto& after = eval.env.scopes.front();

            // Emit one assignment per pre-existing variable the loop changed
            std::vector<StatementPtr> replacements;

            for (const auto& [name, value] : after)
            {
                auto before = tracked.find(name);

                if (before == tracked.end())
                    continue; // loop-scoped variable, dies with the loop

                const bool same = value.type == before->second.type and
                    (value.type == CVal::T::Int ? value.i == before->second.i : value.b == before->second.b);

                if (same)
                    continue;

                // Script int literals are 32-bit; larger results can't be
                // represented as a constant, keep the loop instead
                if (value.type == CVal::T::Int and not fitsInt32(value.i))
                    return std::nullopt;

                ExprPtr literal = value.type == CVal::T::Int
                    ? std::static_pointer_cast<Expression>(std::make_shared<Atom>(static_cast<int>(value.i)))
                    : std::static_pointer_cast<Expression>(std::make_shared<Atom>(value.b));

                auto assign = std::make_shared<Assign>(Token(TokenType::EXPRESSION, name, 0, 0), literal);

                replacements.push_back(std::make_shared<ExpressionStatement>(assign));
            }

            tracked = after;

            return replacements;
        }

        /** Drop names a statement may have mutated from the tracked constants */
        void invalidate(const StatementPtr& s, std::map<std::string, CVal>& tracked)
        {
            ast::LoopFacts facts;
            ast::analyzeStmt(s, facts);

            if (facts.hasCalls)
            {
                tracked.clear(); // a call can write any global/captured local
                return;
            }

            for (const auto& name : facts.mutated)
                tracked.erase(name);
        }

        void processStatementList(std::queue<StatementPtr>& statements, Ctx& ctx)
        {
            std::map<std::string, CVal> tracked;

            std::queue<StatementPtr> rebuilt;

            while (not statements.empty())
            {
                auto s = statements.front();
                statements.pop();

                if (not s)
                {
                    rebuilt.push(s);
                    continue;
                }

                const auto& type = s->getType();

                if (type == "WhileStatement" or type == "ForStatement")
                {
                    auto replacements = foldLoop(s, tracked);

                    if (replacements)
                    {
                        for (const auto& replacement : *replacements)
                            rebuilt.push(replacement);

                        ctx.changed = true;
                        continue; // loop fully evaluated, drop it
                    }

                    // Not statically resolvable: fold nested loops inside it,
                    // then drop whatever it mutates from the tracked set
                    recurseNested(s, ctx);
                    invalidate(s, tracked);

                    rebuilt.push(s);
                    continue;
                }

                if (type == "VariableStatement")
                {
                    auto v = std::static_pointer_cast<VariableStatement>(s);

                    // Track the declared value when it is statically known;
                    // otherwise the name becomes untracked (still declared)
                    tracked.erase(v->name.text);

                    if (not v->expr)
                    {
                        tracked[v->name.text] = CVal::makeInt(0);
                    }
                    else
                    {
                        Evaluator eval;
                        eval.env.scopes.push_back(tracked);

                        auto value = eval.evalExpr(v->expr);

                        if (value and not eval.failed)
                            tracked[v->name.text] = *value;
                    }

                    rebuilt.push(s);
                    continue;
                }

                if (type == "ExpressionStatement")
                {
                    // Track through pure assignments; anything else falls back
                    // to conservative invalidation
                    Evaluator eval;
                    eval.env.scopes.push_back(tracked);

                    auto value = eval.evalExpr(std::static_pointer_cast<ExpressionStatement>(s)->expr);

                    if (value and not eval.failed)
                        tracked = eval.env.scopes.front();
                    else
                        invalidate(s, tracked);

                    rebuilt.push(s);
                    continue;
                }

                if (type == "ImportStatement")
                {
                    // Imported scripts execute immediately and can write any
                    // global by name - forget everything
                    tracked.clear();

                    rebuilt.push(s);
                    continue;
                }

                // Any other statement: fold nested regions independently and
                // conservatively drop what it may touch
                recurseNested(s, ctx);
                invalidate(s, tracked);

                rebuilt.push(s);
            }

            statements = std::move(rebuilt);
        }
    }

    bool StaticLoopEvaluationPass::runPass(VM*, std::queue<StatementPtr>& statements)
    {
        Ctx ctx;

        processStatementList(statements, ctx);

        return ctx.changed;
    }
}
