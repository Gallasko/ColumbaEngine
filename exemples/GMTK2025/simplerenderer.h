#pragma once

#include "Renderer/renderer.h"

namespace pg
{
    template <typename... Comps>
    struct SimpleRenderer : public AbstractRenderer, public System<InitSys, Listener<EntityChangedEvent>>
    {
        struct SimpleRenderCall
        {
            SimpleRenderCall(const RenderCall& call) : call(call) {}

            RenderCall call;
        };

        ComponentRegistry* registryPtr = nullptr;

        SimpleRenderer(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render)
        {
        }

        ~SimpleRenderer()
        {
            unregisterComponents(&renderCallOwner, registryPtr, tag<Own<SimpleRenderCall>>{});
        }

        void init() override
        {
            registryPtr = registry;

            registerComponents(&renderCallOwner, registry, tag<Own<SimpleRenderCall>>{});

            setupRenderer();

            auto group = registerGroup<Comps...>();

            group->addOnGroup([this](EntityRef entity) {
                updateQueue.push(entity->id);
                changed = true;
            });

            group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
                auto entity = ecsRef->getEntity(id);

                if (entity->has<SimpleRenderCall>())
                    ecsRef->detach<SimpleRenderCall>(entity);

                changed = true;
            });
        }

        void execute() override
        {
            if (not changed)
                return;

            while (not updateQueue.empty())
            {
                auto entityId = updateQueue.front();

                auto entity = ecsRef->getEntity(entityId);

                if (not entity)
                {
                    updateQueue.pop();
                    continue;
                }

                if (entity->has<SimpleRenderCall>())
                {
                    entity->get<SimpleRenderCall>()->call = createRenderCall(entity->get<Comps>()...);
                }
                else
                {
                    ecsRef->_attach<SimpleRenderCall>(entity, createRenderCall(entity->get<Comps>()...));
                }

                updateQueue.pop();
            }

            renderCallList.clear();

            const auto& renderCallView = renderCallOwner.view();

            renderCallList.reserve(renderCallView.nbComponents());

            for (const auto& renderCall : renderCallView)
            {
                renderCallList.push_back(renderCall->call);
            }

            finishChanges();
        }

        void onEvent(const EntityChangedEvent& event) override
        {
            auto entity = ecsRef->getEntity(event.id);

            if (not entity or not (entity->has<Comps>() or ...))
                return;

            updateQueue.push(event.id);

            changed = true;
        }

        virtual void setupRenderer() = 0;

        virtual RenderCall createRenderCall(CompRef<Comps>... comps) = 0;

        std::queue<_unique_id> updateQueue;

        Own<SimpleRenderCall> renderCallOwner;
    };
}