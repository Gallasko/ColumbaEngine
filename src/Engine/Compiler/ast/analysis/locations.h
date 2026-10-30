#pragma once

/**
 * @file locations.h
 * @brief Interned access paths for the effect analysis: where a value lives.
 *
 * A location is a root and a path of keys below it: a global (`g:name`), an input of the script
 * (`in:name`), a parameter of the function summarized (`#i`), an allocation site (`@n`) or the
 * unknown (`?`). A key is a field name, `*` (AnyKey: a key not known when compiled, a list's
 * entries) or `[#k]` (the field named by parameter k, in summaries only). Every location is
 * interned, so two ids are equal when the paths are. A path deeper than MaxDepth is folded into
 * its ancestor at MaxDepth, which is then deep: it stands for everything below it.
 */

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace pg
{
    namespace ast
    {
        namespace analysis
        {
            using LocId = uint32_t;
            using KeyId = uint32_t;

            enum class RootKind : uint8_t { Global, Input, Param, Site, Unknown };

            constexpr uint8_t MaxDepth = 6;

            class Locations
            {
            public:
                static constexpr KeyId AnyKey = 0;  // "*": a key not known when compiled, a list's entries

                KeyId key(const std::string& name);  // interned; "*" gives AnyKey
                KeyId paramKey(uint32_t param);      // "[#k]": the field named by parameter k (summaries only)
                bool isParamKey(KeyId k) const;
                uint32_t paramOfKey(KeyId k) const;

                LocId global(const std::string& name);  // "g:name"
                LocId input(const std::string& name);   // "in:name"
                LocId param(uint32_t i);                // "#i"
                LocId site(const void* node);           // "@n", n = order of first sight
                LocId unknown();                        // "?"

                LocId child(LocId parent, KeyId key);  // interned; past MaxDepth: the ancestor at depth MaxDepth, deep

                LocId parent(LocId l) const;
                KeyId keyOf(LocId l) const;
                uint8_t depth(LocId l) const;
                RootKind rootKind(LocId l) const;
                LocId rootOf(LocId l) const;
                bool deep(LocId l) const;

                bool overlaps(LocId a, LocId b) const;       // may name a common object or one inside the other
                bool within(LocId inner, LocId outer) const;  // every object `inner` names is `outer` or inside it

                std::string text(LocId l) const;  // "g:activities.*.gains", "#0.[#1].*.amount"

            private:
                static constexpr KeyId ParamKeyBit = 0x80000000u;
                static constexpr LocId NoParent = 0xFFFFFFFFu;

                struct Node
                {
                    LocId parent;
                    KeyId key;  // below a root: the key; of a root: its name key, index or order
                    uint8_t depth;
                    RootKind kind;
                    bool deep;
                    LocId root;
                };

                LocId addRoot(RootKind kind, uint32_t payload);
                static bool keysAgree(KeyId a, KeyId b);

                std::vector<Node> nodes;
                std::vector<std::string> keyNames;
                std::unordered_map<std::string, KeyId> keyIds;
                std::unordered_map<uint64_t, LocId> children;
                std::unordered_map<KeyId, LocId> globals, inputs;
                std::unordered_map<uint32_t, LocId> params;
                std::unordered_map<const void*, LocId> sites;
                LocId unknownLoc = NoParent;
            };
        }
    }
}
