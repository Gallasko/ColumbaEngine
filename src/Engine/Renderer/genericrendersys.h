#pragma once

#include <algorithm>
#include <unordered_set>

#include "renderer.h"

#include "ECS/system.h"

namespace pg
{
    template <typename OwnedComp, typename Event1, typename Comp2, typename Event2>
    struct GenericRenderSystem : public AbstractRenderer, System<Own<OwnedComp>, Listener<Event1>, Listener<Event2>, InitSys>
    {
        GenericRenderSystem(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render) { }

        virtual void setup() = 0;

        virtual void init() override final
        {
            setup();

            auto group = this->template registerGroup<OwnedComp, Comp2>();

            group->addOnGroup([this](EntityRef entity) {
                auto c1 = entity->template get<OwnedComp>();
                auto c2 = entity->template get<Comp2>();

                entityRenderCalls[entity->id] = createRenderCall(c1, c2);
                entitiesInRenderGroup.push_back(entity->id);

                std::sort(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end());

                changed = true;
            });

            group->removeOfGroup([this](EntitySystem*, _unique_id id) {
                // Remove render call from the map
                entityRenderCalls.erase(id);
                entitiesInRenderGroup.erase(std::remove(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), id), entitiesInRenderGroup.end());

                changed = true;
            });
        }

        virtual void execute() override final
        {
            if (not changed)
            {
                return;
            }

            std::vector<_unique_id> updateQueue;
            std::vector<_unique_id> temp;

            temp.assign(updateSet.begin(), updateSet.end());

            std::sort(temp.begin(), temp.end());

            std::set_intersection(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), temp.begin(), temp.end(),
                            std::back_inserter(updateQueue));

            // Clear the update set after processing
            updateSet.clear();

            for (const auto& entityId : updateQueue)
            {
                auto entity = this->ecsRef->getEntity(entityId);

                if (not entity)
                {
                    LOG_WARNING("GenericRenderSystem", "Entity " << entityId << " NOT FOUND in ECS! Skipping...");
                    continue;
                }

                auto c1 = entity->template get<OwnedComp>();
                auto c2 = entity->template get<Comp2>();

                // Store render call directly in the system's map (no component needed!)
                entityRenderCalls[entityId] = createRenderCall(c1, c2);
            }

            renderCallList.clear();

            // Build render call list from the system's map
            renderCallList.reserve(entityRenderCalls.size());

            for (const auto& [entityId, renderCall] : entityRenderCalls)
            {
                renderCallList.push_back(renderCall);
            }

            finishChanges();
        }

        virtual RenderCall createRenderCall(CompRef<OwnedComp>, CompRef<Comp2>) = 0;

        virtual void onEvent(const Event1& event) override final
        {
            onEventUpdate(event.id);
        }

        virtual void onEvent(const Event2& event) override final
        {
            onEventUpdate(event.id);
        }

        void onEventUpdate(_unique_id entityId)
        {
            updateSet.insert(entityId);

            changed = true;
        }

        std::unordered_set<_unique_id> updateSet;

        // Map of entity ID to render call - owned by the system directly
        std::unordered_map<_unique_id, RenderCall> entityRenderCalls;
        std::vector<_unique_id> entitiesInRenderGroup;
    };
}