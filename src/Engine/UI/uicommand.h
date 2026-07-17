#pragma once

#include "Memory/elementtype.h"

#include <string>

namespace pg
{
    /**
     * UICommand
     *
     * String-keyed intent emitted by UI when the user does something
     * (clicks a button, drags a slot, ticks a checkbox). Game systems subscribe
     * to UICommandEvent and dispatch on `id`; UI never includes a gameplay header.
     *
     * Args use the engine's `ElementType` tagged variant so a future editor can
     * introspect each arg's type via `ElementType::UnionType` without RTTI.
     *
     * Convention: command ids are dotted, lowercase, namespaced by domain:
     *   "mission.start", "machine.setRecipe", "depot.purchaseSlot".
     */
    struct UICommand
    {
        std::string id;
        ElementMap args;

        UICommand() = default;
        UICommand(std::string id) : id(std::move(id)) {}
        UICommand(std::string id, ElementMap args) : id(std::move(id)), args(std::move(args)) {}

        bool hasArg(const std::string& key) const { return args.find(key) != args.end(); }

        const ElementType& arg(const std::string& key) const
        {
            static const ElementType empty;
            auto it = args.find(key);
            return it != args.end() ? it->second : empty;
        }

        // Typed accessors with fallback. Mirror the helpers in prefabfactory.h.
        int argInt(const std::string& key, int fallback = 0) const
        {
            auto it = args.find(key);
            if (it == args.end() or it->second.isEmpty())
                return fallback;

            const auto& v = it->second;
            switch (v.type)
            {
                case UnionType::INT:
                    return v.get<int>();
                case UnionType::SIZE_T:
                    return static_cast<int>(v.get<size_t>());
                case UnionType::FLOAT:
                    return static_cast<int>(v.get<float>());
                case UnionType::DOUBLE:
                    return static_cast<int>(v.get<double>());
                default:
                    return fallback;
            }
        }

        size_t argSize(const std::string& key, size_t fallback = 0) const
        {
            auto it = args.find(key);
            if (it == args.end() or it->second.isEmpty())
                return fallback;

            const auto& v = it->second;
            switch (v.type)
            {
                case UnionType::SIZE_T:
                    return v.get<size_t>();
                case UnionType::INT:
                    return static_cast<size_t>(v.get<int>());
                default:
                    return fallback;
            }
        }

        float argFloat(const std::string& key, float fallback = 0.0f) const
        {
            auto it = args.find(key);
            if (it == args.end() or it->second.isEmpty())
                return fallback;

            const auto& v = it->second;
            switch (v.type)
            {
                case UnionType::FLOAT:
                    return v.get<float>();
                case UnionType::DOUBLE:
                    return static_cast<float>(v.get<double>());
                case UnionType::INT:
                    return static_cast<float>(v.get<int>());
                case UnionType::SIZE_T:
                    return static_cast<float>(v.get<size_t>());
                default:
                    return fallback;
            }
        }

        std::string argString(const std::string& key, const std::string& fallback = "") const
        {
            auto it = args.find(key);
            if (it == args.end() or it->second.isEmpty() or it->second.type != UnionType::STRING)
                return fallback;

            return it->second.get<std::string>();
        }

        bool argBool(const std::string& key, bool fallback = false) const
        {
            auto it = args.find(key);
            if (it == args.end() or it->second.isEmpty() or it->second.type != UnionType::BOOL)
                return fallback;

            return it->second.get<bool>();
        }
    };

    struct UICommandEvent
    {
        UICommand cmd;

        UICommandEvent() = default;
        UICommandEvent(UICommand c) : cmd(std::move(c)) {}
        UICommandEvent(std::string id) : cmd(std::move(id)) {}
        UICommandEvent(std::string id, ElementMap args) : cmd(std::move(id), std::move(args)) {}
    };
}
