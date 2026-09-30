#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "ECS/system.h"
#include "Systems/gamefacts.h"

namespace chronicle
{
    // Subscriptions on the engine's WorldFacts: a path (or every path under a prefix) to a
    // handler, with an id to unsubscribe by. The engine sends one WorldFactsUpdate a frame with
    // the names that changed; the router calls the handlers of each, in subscription order.
    //
    // A scene wires its widgets here once and drops every id when it leaves, so what it
    // subscribed is gone with it (count() returns to what it was).
    struct FactRouter : public pg::System<pg::Listener<pg::WorldFactsUpdate>, pg::StoragePolicy>
    {
        using SubId = size_t;
        using Handler = std::function<void(const pg::ElementType&)>;
        using PrefixHandler = std::function<void(const std::string& path, const pg::ElementType&)>;

        virtual std::string getSystemName() const override { return "Chronicle Fact Router"; }

        // `path` exactly.
        SubId on(const std::string& path, Handler handler);

        // Every path that starts with `prefix` ("activity." sees "activity.train.yard.state").
        SubId onPrefix(const std::string& prefix, PrefixHandler handler);

        void off(SubId id);

        size_t count() const { return subscriptions.size(); }

        virtual void onEvent(const pg::WorldFactsUpdate& event) override;

    private:
        struct Subscription
        {
            std::string path;
            bool prefix = false;
            Handler handler;
            PrefixHandler prefixHandler;
        };

        SubId nextId = 1;
        std::map<SubId, Subscription> subscriptions;   // Ordered: handlers run in subscription order
    };
}
