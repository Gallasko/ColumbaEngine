#pragma once

/**
 * @file ssa.h
 * @brief The SSA form of a body for the effect analysis: what each value is made of, what is written.
 *
 * A function's body or a top-level statement becomes a list of nodes, one per value made: the
 * parameters, the globals read, the constants, the tables made by a literal, the fields read, the
 * calls, and the phis where branches and loops join. Locals are not kept: a local is the node it
 * holds at each point. What the body does to the world is listed beside: the fields written
 * (`stores`), the globals assigned (`rebinds`), the values returned, and `unknown` when the body
 * holds what is not followed (a closure, a nested fun, a class, `this`, a dprint).
 *
 * A table made by a literal of the body is followed field by field while it is only read and
 * written with constant keys: a field read after a write is the value written. Any other use
 * (handed to a call, stored, returned, joined) and any loop entered end that for the rest of the
 * body; its field reads are then `Field` nodes.
 */

#include "locations.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pg
{
    class Expression;
    class Statement;
    struct FunctionStatement;

    namespace ast
    {
        namespace analysis
        {
            using ValueId = uint32_t;

            enum class Op : uint8_t { Const, Param, Global, Literal, Field, Phi, Call, Native, Pure, Unknown };

            // What a native known to the analysis does with its arguments
            enum class NativeKind : uint8_t
            {
                None,     // Not a native known
                Fresh,    // Returns a value of its own (a number, a string, a new list of strings)
                Lookup,   // Returns the field of its first named by its second, or its third (lookup, pick)
                Entry,    // Returns an entry of its first (findBy)
                IndexBy,  // Returns a new table of the entries of its first
                Copy,     // Returns a new table or list holding what its first holds
                Push,     // Writes its first, puts its second in it
                Insert,   // Writes its first, puts its third in it
                Take,     // Writes its first, returns an entry of it (pop, removeAt)
                Merge,    // Writes its first, puts what its second holds in it, returns the first
            };

            NativeKind nativeKind(const std::string& name);

            struct Node
            {
                Op op;
                std::vector<ValueId> in;        // Field: {base}; Phi/Call/Native/Pure: operands; Const/Param/Global/Literal: none
                KeyId key = Locations::AnyKey;  // Field: the key (AnyKey, a name, or a param key)
                LocId site = 0;                 // Literal: its site
                uint32_t param = 0;             // Param: its index
                std::string name;               // Global: the name; Call/Native: the callee ("" for a call of a value); Const: its text ("1", "\"a\"", "none"); Literal: its site's text ("@0")
                const void* origin = nullptr;   // The expression that made it
            };

            struct Store
            {
                ValueId base;
                KeyId key;  // AnyKey for push, insert and keys not known
                ValueId value;
            };

            struct BodyIR
            {
                std::vector<Node> nodes;
                std::vector<Store> stores;                               // base[key] = value, in program order
                std::vector<std::pair<std::string, ValueId>> rebinds;    // a global assigned (name, value)
                std::vector<std::pair<std::string, ValueId>> defines;    // a top-level var or const (name, value)
                std::vector<ValueId> returns;
                bool unknown = false;                                    // a closure, nested fun, class, `this` or dprint inside
                std::unordered_map<const Expression*, ValueId> valueOf;  // the IR mapped back to the AST
                std::unordered_map<KeyId, std::string> keyNames;         // the text of each key used, for dump()

                std::vector<ValueId> leaves(ValueId v) const;  // through phis, each non-phi once, in id order
                std::string dump() const;
            };

            // `isNative(name)`: true when the program defines nothing of that name (the builder still checks locals)
            BodyIR buildFunction(const FunctionStatement& f, Locations& locs, const std::function<bool(const std::string&)>& isNative);
            BodyIR buildTopLevel(const std::shared_ptr<Statement>& s, Locations& locs, const std::function<bool(const std::string&)>& isNative);
        }
    }
}
