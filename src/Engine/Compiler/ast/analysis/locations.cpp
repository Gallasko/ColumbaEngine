#include "stdafx.h"

#include "locations.h"

namespace pg
{
    namespace ast
    {
        namespace analysis
        {
            KeyId Locations::key(const std::string& name)
            {
                if (keyNames.empty())
                {
                    keyNames.push_back("*");
                    keyIds["*"] = AnyKey;
                }

                auto it = keyIds.find(name);

                if (it != keyIds.end())
                    return it->second;

                const KeyId id = static_cast<KeyId>(keyNames.size());
                keyNames.push_back(name);
                keyIds[name] = id;
                return id;
            }

            KeyId Locations::paramKey(uint32_t param)
            {
                return ParamKeyBit | param;
            }

            bool Locations::isParamKey(KeyId k) const
            {
                return (k & ParamKeyBit) != 0;
            }

            uint32_t Locations::paramOfKey(KeyId k) const
            {
                return k & ~ParamKeyBit;
            }

            LocId Locations::addRoot(RootKind kind, uint32_t payload)
            {
                const LocId id = static_cast<LocId>(nodes.size());
                nodes.push_back({NoParent, payload, 0, kind, false, id});
                return id;
            }

            LocId Locations::global(const std::string& name)
            {
                const KeyId k = key(name);
                auto it = globals.find(k);

                if (it != globals.end())
                    return it->second;

                return globals[k] = addRoot(RootKind::Global, k);
            }

            LocId Locations::input(const std::string& name)
            {
                const KeyId k = key(name);
                auto it = inputs.find(k);

                if (it != inputs.end())
                    return it->second;

                return inputs[k] = addRoot(RootKind::Input, k);
            }

            LocId Locations::param(uint32_t i)
            {
                auto it = params.find(i);

                if (it != params.end())
                    return it->second;

                return params[i] = addRoot(RootKind::Param, i);
            }

            LocId Locations::site(const void* node)
            {
                auto it = sites.find(node);

                if (it != sites.end())
                    return it->second;

                return sites[node] = addRoot(RootKind::Site, static_cast<uint32_t>(sites.size()));
            }

            LocId Locations::unknown()
            {
                if (unknownLoc == NoParent)
                    unknownLoc = addRoot(RootKind::Unknown, 0);

                return unknownLoc;
            }

            LocId Locations::child(LocId parentLoc, KeyId k)
            {
                if (nodes[parentLoc].depth >= MaxDepth)
                {
                    nodes[parentLoc].deep = true;
                    return parentLoc;
                }

                const uint64_t id = (static_cast<uint64_t>(parentLoc) << 32) | k;
                auto it = children.find(id);

                if (it != children.end())
                    return it->second;

                const LocId loc = static_cast<LocId>(nodes.size());
                const Node p = nodes[parentLoc];
                nodes.push_back({parentLoc, k, static_cast<uint8_t>(p.depth + 1), p.kind, false, p.root});
                children[id] = loc;
                return loc;
            }

            LocId Locations::parent(LocId l) const { return nodes[l].parent; }
            KeyId Locations::keyOf(LocId l) const { return nodes[l].key; }
            uint8_t Locations::depth(LocId l) const { return nodes[l].depth; }
            RootKind Locations::rootKind(LocId l) const { return nodes[l].kind; }
            LocId Locations::rootOf(LocId l) const { return nodes[l].root; }
            bool Locations::deep(LocId l) const { return nodes[l].deep; }

            // A parameter key may be any field at a call, so for overlap it agrees with every key
            bool Locations::keysAgree(KeyId a, KeyId b)
            {
                return a == b or a == AnyKey or b == AnyKey or ((a | b) & ParamKeyBit) != 0;
            }

            bool Locations::overlaps(LocId a, LocId b) const
            {
                if (nodes[a].kind == RootKind::Unknown or nodes[b].kind == RootKind::Unknown)
                    return true;

                if (nodes[a].root != nodes[b].root)
                    return false;

                // Bring the longer path up to the depth of the shorter, then compare pairwise
                while (nodes[a].depth > nodes[b].depth)
                    a = nodes[a].parent;

                while (nodes[b].depth > nodes[a].depth)
                    b = nodes[b].parent;

                while (nodes[a].parent != NoParent)
                {
                    if (not keysAgree(nodes[a].key, nodes[b].key))
                        return false;

                    a = nodes[a].parent;
                    b = nodes[b].parent;
                }

                return true;
            }

            bool Locations::within(LocId inner, LocId outer) const
            {
                if (nodes[outer].kind == RootKind::Unknown)
                    return true;

                if (nodes[inner].root != nodes[outer].root or nodes[inner].depth < nodes[outer].depth)
                    return false;

                while (nodes[inner].depth > nodes[outer].depth)
                    inner = nodes[inner].parent;

                while (nodes[outer].parent != NoParent)
                {
                    if (nodes[outer].key != AnyKey and nodes[outer].key != nodes[inner].key)
                        return false;

                    outer = nodes[outer].parent;
                    inner = nodes[inner].parent;
                }

                return true;
            }

            std::string Locations::text(LocId l) const
            {
                std::string path;

                while (nodes[l].parent != NoParent)
                {
                    const KeyId k = nodes[l].key;
                    const std::string name = isParamKey(k) ? "[#" + std::to_string(paramOfKey(k)) + "]" : keyNames[k];
                    path = "." + name + path;
                    l = nodes[l].parent;
                }

                std::string root;

                switch (nodes[l].kind)
                {
                case RootKind::Global: root = "g:" + keyNames[nodes[l].key]; break;
                case RootKind::Input: root = "in:" + keyNames[nodes[l].key]; break;
                case RootKind::Param: root = "#" + std::to_string(nodes[l].key); break;
                case RootKind::Site: root = "@" + std::to_string(nodes[l].key); break;
                case RootKind::Unknown: root = "?"; break;
                }

                return root + path;
            }
        }
    }
}
