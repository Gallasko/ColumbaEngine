#pragma once

/**
 * @file loop_ownership.h
 * @brief What passes that move a read out of a loop share: which lists a function holds alone, and
 *        whether a loop's region can reach what a path names.
 *
 * All by names: a function's lists are told apart by how they are declared and used. A list made
 * from a literal that never leaves the function can be named by nothing else (Ownership::owned),
 * and with all it holds down to the last row (Ownership::ownedDeep). A local that only ever holds
 * rows of a list (`var row = table[i]`) is that list's: it does not take the rows out of it as long
 * as it stays in the function itself.
 *
 * Known (accepted) limits, as for LoopInvariantHoisting: field reads and writes on class instances
 * run __get / __set metamethods, and an operator metamethod could return a table where a number is
 * taken for granted. A name read before the function declares it is taken for the local.
 */

#include "Interpreter/expression.h"
#include "Interpreter/statement.h"

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace pg
{
    namespace ast
    {
        // Natives that change no table and call no script code
        extern const std::set<std::string> readOnlyNatives;

        // Natives that change the list they are given first
        extern const std::set<std::string> listNatives;

        template <typename T>
        std::vector<T> listOf(std::queue<T> q)
        {
            std::vector<T> out;

            while (not q.empty())
            {
                out.push_back(q.front());
                q.pop();
            }

            return out;
        }

        template <typename T>
        std::queue<T> queueOf(const std::vector<T>& v)
        {
            std::queue<T> out;

            for (const auto& e : v)
                out.push(e);

            return out;
        }

        std::string varName(const ExprPtr& e);

        // The t of `t` or `t[i][j]`, or ""
        std::string rootOf(const ExprPtr& e);

        // A name or a number, as an index reads it; "" for anything else
        std::string indexOf(const ExprPtr& e);

        // A name or a chain of indexing by names and numbers, as text; "" for anything else
        std::string pathOf(const ExprPtr& e);

        // The names of a path
        void namesOf(const ExprPtr& e, std::set<std::string>& out);

        // Whether nothing but what the expression makes can reach its value: no table, or a new one
        // holding only such values. An operator makes a number or a string: lists do not add
        bool fresh(const ExprPtr& e);

        // The names a function body declares
        void declsOf(const StatementPtr& s, std::set<std::string>& out);

        // What a pass that took the list natives for the natives decided rests on, as text
        // (AstPass::factsAbout): whether the program could be read, and which of the names, and of
        // those natives, may be script code
        std::string listNativeFacts(const std::set<std::string>& names, const std::set<std::string>& scriptNames, bool programComplete);

        // Per name of a function: how it is declared and assigned, and whether it leaves
        // Per name of a function: how it is declared and assigned, and whether it leaves
        struct Ownership
        {
            std::map<std::string, int> listDecls;          // `var name = [...]`
            std::set<std::string> tableLiterals;           // Given `{...}` or `[key: ...]`
            std::set<std::string> otherWrites;             // Declared or assigned anything else, a parameter, a loop variable
            std::set<std::string> escaped;                 // Read anywhere a list could leave the function
            std::set<std::string> given;                   // Given a value that is not fresh: its rows may be another's
            std::set<std::string> rowsRead;                // A row read anywhere it could leave the function
            std::set<std::string> noTables;                // Declared or assigned what is no table
            std::set<std::string> maybeTables;             // Declared or assigned what may be one
            std::set<std::string> nativesTrusted;          // The noTableNatives noTables rests on
            std::map<std::string, std::set<std::string>> rowOf;  // A local given rows of these: `var row = t[i]`
            std::set<std::string> notRows;                 // Given anything else: a parameter, a list of its own
            std::map<std::string, int> declared;           // Parameters and `var` by name
            std::set<std::string> rebound;                 // Assigned besides that, or by a function it defines
            // Integers are told by declaration, not by name: each `var` is a key of its own (`k#2`), a read
            // the key of the declaration it reads where it stands, a global its bare name
            std::map<std::string, std::vector<ExprPtr>> intWrites;  // What a key is given
            std::set<std::string> notInts;                 // Keys given what is not read: a parameter, a loop variable
            std::set<std::string> written;                 // Locals given a value, by name
            std::map<std::string, std::set<std::string>> givenNames;  // Locals a list was given
            std::set<std::string> notPlain;                // Given anything but a number, a string or none
            std::set<std::string> ints;                    // Keys that only ever hold an integer (settleInts)

            std::vector<std::map<std::string, std::string>> scopes;  // While read: the keys in sight, innermost last
            std::map<std::string, int> keyCount;
            std::map<std::string, std::set<std::string>> keysOfName;
            std::map<ExprPtr, std::string> keyOf;          // The key a read reads (held: no other node takes its place)
            std::set<ExprPtr> keyMixed;                    // A node met at two places: told by its name
            std::set<std::string> readUnbound;             // Names read where no declaration of theirs stands

            // What is known of a function: its parameters and its body read whole
            static Ownership ofFunction(const std::queue<ExprPtr>& parameters, const StatementPtr& body);

            // What is known of the script's top level read whole: the locals of its nested blocks are
            // its own as a function's are, a global is no one's (`locals`: those of its nested blocks)
            static Ownership ofScript(const std::queue<StatementPtr>& statements, const std::set<std::string>& locals);

            bool owned(const std::string& name) const
            {
                auto it = listDecls.find(name);
                return it != listDecls.end() and it->second == 1 and not otherWrites.count(name) and not escaped.count(name);
            }

            // Only ever given `[...]`: whatever else may name it, it is no table
            bool holdsList(const std::string& name) const
            {
                return listDecls.count(name) and not otherWrites.count(name) and not tableLiterals.count(name);
            }

            // Owned with all it holds, down to the last row
            bool ownedDeep(const std::string& name) const
            {
                std::set<std::string> seen;
                return owned(name) and not given.count(name) and not rowsRead.count(name) and rowsStay(name, seen) and givenOnlyPlain(name);
            }

            // Only ever holds rows of what is owned down to the last row
            bool rowOfOwned(const std::string& name) const;

            // The name and all it may be a row of
            std::set<std::string> listsBehind(const std::string& name) const;

            bool holdsNoTable(const std::string& name) const
            {
                return noTables.count(name) and not maybeTables.count(name);
            }

            void wrote(const std::string& name, const std::string& key, const ExprPtr& value);

            // A value that is no table when the natives it calls are the natives
            bool noTable(const ExprPtr& e);

            // Whether an expression is an integer, by what is known of the locals (settleInts)
            bool isInt(const ExprPtr& e) const;

            // Whether every local it was given only ever held what is no table
            bool givenOnlyPlain(const std::string& name) const;

            // Given a value: it goes on `given` unless what it is can be told, now or once the function
            // is read whole
            void gave(const std::string& list, const ExprPtr& value);

            // Whether nothing holding a row of it leaves: `seen`, the names on the way there
            bool rowsStay(const std::string& name, std::set<std::string>& seen) const;

            // `ints`: the locals that only ever hold what is an integer when they all do
            void settleInts();

            // A declaration's key, in sight once bound: its value is read before, where it is not yet
            std::string newKey(const std::string& name);
            void bind(const std::string& name, const std::string& key) { scopes.back()[name] = key; }
            std::string bound(const std::string& name) const;
            void read(const ExprPtr& var);
        };

        // `nested`: inside a function the function defines, where every name read is captured
        void ownStmt(const StatementPtr& s, Ownership& o, bool nested);

        // What a pass knows where it stands: of the function being rewritten, none at top level
        struct FunctionScope
        {
            const std::set<std::string>& scriptNames;

            std::set<std::string> locals;
            Ownership ownership;
            bool inFunction = false;
            bool scriptRead = false;    // At top level, `ownership` is Ownership::ofScript

            int slotsUsed = 0;
            int counter = 0;
            long hoisted = 0;

            bool isNative(const std::string& name) const
            {
                return not scriptNames.count(name) and not locals.count(name);
            }

            // Ownership took them for the natives
            bool ownershipHolds() const
            {
                if (not inFunction and not scriptRead)
                    return false;

                for (const auto& native : {"len", "push", "insert", "pop", "removeAt"})
                {
                    if (not isNative(native))
                        return false;
                }

                return true;
            }

            // Whether a path is a list by what its function shows: no field write lengthens it, as a
            // field write on a list stops the script. Only a local: a function could give a global anything
            bool isList(const ExprPtr& path) const
            {
                return path->kind() == AstKind::Var and locals.count(varName(path)) and ownership.holdsList(varName(path));
            }

            // Whether no write but those that name it can reach what a path measures: a list that
            // never left the function, or no table at all
            bool beyondWrites(const ExprPtr& path) const
            {
                const std::string root = rootOf(path);
                const bool whole = path->kind() == AstKind::Var;

                if (ownershipHolds() and (whole ? ownership.owned(root) : ownership.ownedDeep(root)))
                    return true;

                if (not whole or not locals.count(root) or not ownership.holdsNoTable(root))
                    return false;

                for (const auto& native : ownership.nativesTrusted)
                {
                    if (not isNative(native))
                        return false;
                }

                return true;
            }
        };

        // Whether a loop's region (not the functions it defines) leaves what a path names alone
        struct RegionCheck
        {
            const FunctionScope& ctx;
            const std::string& x;       // The path's root
            const bool beyondWrites;    // FunctionScope::beyondWrites
            const bool slotOnly = false;  // Only what x holds at the path matters, not how long it is
            const bool noFields = false;  // No field write can change what the path measures
            bool ok = true;

            // Whether writing into a list leaves the path alone
            bool mayWrite(const ExprPtr& list) const;

            void expr(const ExprPtr& e);
            void stmt(const StatementPtr& s);
        };

        // What a region reads of `hidden`, by `keyOf` ("" for what it is not), the hidden local instead
        // (not in the functions it defines)
        struct Replace
        {
            const std::map<std::string, std::string>& hidden;
            std::string (*keyOf)(const ExprPtr&);

            ExprPtr expr(const ExprPtr& e);
            void stmt(const StatementPtr& s);
        };
    }
}
