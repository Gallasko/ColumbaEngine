#pragma once

/**
 * @file useless_statements.h
 * @brief AST pass: a statement whose value is dropped and that can do nothing else is removed.
 *
 *     a1 == a2;
 *     x;
 *     1;
 *
 * are gone. A statement kept would read its operands and drop what it made: removed, it cannot be
 * told apart, provided it can neither run code nor stop the script. Only these are taken:
 * - literals, none included;
 * - a local in scope (a parameter, a `var` of an enclosing block or function, read before or as an
 *   upvalue): a global may be undefined, and reading one stops the script;
 * - `==` and `!=`, which compare any two values and run no metamethod;
 * - `and` / `or`, which only test what their operands are;
 * - `not`, of what is surely a bool or none (anything else stops the script);
 * - parentheses around any of these.
 * Arithmetic (it may overflow), `<` and the like (they refuse some types), negation, calls, field
 * and index reads (metamethods), assignments and steps stay.
 *
 * A body of an `if` or a loop that was such a statement becomes an empty block. Anonymous
 * functions are left as they are.
 */

#include "../ast_pass.h"

namespace pg
{
    class UselessStatementsPass : public AstPass
    {
    public:
        std::string getName() const override { return "UselessStatements"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;
    };
}
