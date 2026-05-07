#pragma once

#include "ECS/system.h"
#include "ECS/entitysystem.h"
#include "Memory/elementtype.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pg
{
    /**
     * GameDataView
     *
     * Engine-side, game-populated key-value store. The contract between game
     * logic and UI is exactly this surface plus UICommandEvent — UI never
     * reaches into game systems directly.
     *
     *   Game side  : set(path, value)
     *   UI side    : subscribe(path, onChange) / get(path)
     *
     * Paths are flat, dotted strings. For composite values (e.g. an ItemStack
     * with id+count) write a small path family, one ElementType per leaf:
     *   "depot.5,3.input.0.itemId"
     *   "depot.5,3.input.0.count"
     *
     * Delivery is synchronous: set() invokes every subscriber on that path
     * before returning. Game logic is single-threaded, so re-entrancy through
     * a subscriber is the caller's problem to manage (don't set the same path
     * from inside its own onChange).
     */
    class GameDataView : public System<>
    {
    public:
        using SubId = uint64_t;

        virtual std::string getSystemName() const override { return "Game Data View"; }

        void set(const std::string& path, ElementType value)
        {
            data[path] = value;

            auto it = subscribers.find(path);
            if (it == subscribers.end())
                return;

            for (const auto& sub : it->second)
            {
                if (sub.onChange)
                    sub.onChange(value);
            }
        }

        ElementType get(const std::string& path) const
        {
            auto it = data.find(path);
            return it != data.end() ? it->second : ElementType{};
        }

        bool has(const std::string& path) const
        {
            return data.find(path) != data.end();
        }

        SubId subscribe(const std::string& path, std::function<void(const ElementType&)> onChange)
        {
            SubId id = ++nextSubId;
            subscribers[path].push_back(Subscription{id, std::move(onChange)});
            return id;
        }

        void unsubscribe(SubId id)
        {
            for (auto& kv : subscribers)
            {
                auto& vec = kv.second;
                for (auto it = vec.begin(); it != vec.end(); ++it)
                {
                    if (it->id == id)
                    {
                        vec.erase(it);
                        return;
                    }
                }
            }
        }

        std::vector<std::string> listPaths() const
        {
            std::vector<std::string> paths;
            paths.reserve(data.size());
            for (const auto& kv : data)
                paths.push_back(kv.first);
            return paths;
        }

    private:
        struct Subscription
        {
            SubId id;
            std::function<void(const ElementType&)> onChange;
        };

        std::unordered_map<std::string, ElementType> data;
        std::unordered_map<std::string, std::vector<Subscription>> subscribers;
        SubId nextSubId = 0;
    };
}
