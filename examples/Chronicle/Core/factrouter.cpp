#include "factrouter.h"

#include <utility>

using namespace pg;

namespace chronicle
{
    FactRouter::SubId FactRouter::on(const std::string& path, Handler handler)
    {
        const SubId id = nextId++;

        Subscription sub;
        sub.path = path;
        sub.handler = std::move(handler);
        subscriptions.emplace(id, std::move(sub));

        return id;
    }

    FactRouter::SubId FactRouter::onPrefix(const std::string& prefix, PrefixHandler handler)
    {
        const SubId id = nextId++;

        Subscription sub;
        sub.path = prefix;
        sub.prefix = true;
        sub.prefixHandler = std::move(handler);
        subscriptions.emplace(id, std::move(sub));

        return id;
    }

    void FactRouter::off(SubId id)
    {
        subscriptions.erase(id);
    }

    void FactRouter::onEvent(const WorldFactsUpdate& event)
    {
        for (const auto& name : event.changedFacts)
        {
            auto fact = event.factMap->find(name);

            // A removed fact has nothing to hand over
            if (fact == event.factMap->end())
                continue;

            // A handler may subscribe or unsubscribe: walk a snapshot of the ids
            std::vector<SubId> ids;
            ids.reserve(subscriptions.size());

            for (const auto& [id, sub] : subscriptions)
                ids.push_back(id);

            for (SubId id : ids)
            {
                auto it = subscriptions.find(id);

                if (it == subscriptions.end())
                    continue;

                // A copy: the handler may unsubscribe itself while it runs
                const Subscription sub = it->second;

                if (sub.prefix)
                {
                    if (name.rfind(sub.path, 0) == 0)
                        sub.prefixHandler(name, fact->second);
                }
                else if (name == sub.path)
                {
                    sub.handler(fact->second);
                }
            }
        }
    }
}
