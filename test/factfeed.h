#pragma once

#include <functional>
#include <string>

#include "ECS/entitysystem.h"
#include "ECS/system.h"
#include "Systems/gamefacts.h"

namespace pg
{
    namespace test
    {
        // Relays every changed fact to a callback, the way a scene does from its WorldFactsUpdate listener
        struct FactFeed : public System<Listener<WorldFactsUpdate>, StoragePolicy>
        {
            virtual std::string getSystemName() const override { return "Fact Feed"; }

            virtual void onEvent(const WorldFactsUpdate& event) override
            {
                for (const auto& name : event.changedFacts)
                {
                    const auto& it = event.factMap->find(name);

                    if (it != event.factMap->end() and onFact)
                        onFact(name, it->second);
                }
            }

            std::function<void(const std::string&, const ElementType&)> onFact;
        };

        // A save system loads from and saves to save/systems.sz whatever the ecs save path is:
        // start from no fact and never write the file back, so a test neither reads nor leaves a state
        inline WorldFacts* createTestFacts(EntitySystem *ecs)
        {
            auto facts = ecs->createSystem<WorldFacts>();

            ecs->getComponentRegistry()->unregisterSystemSave(facts->getSystemName());

            facts->factMap.clear();
            facts->factMetadata.clear();

            return facts;
        }
    }
}
