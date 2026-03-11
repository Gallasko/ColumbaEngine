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

        Material* newMaterial(const std::string& name)
        {
            return &materials[name];
        }

        void applyMaterial(RenderCall& call, const std::string& materialName, const std::vector<std::string>& textures = {})
        {
            std::string materialKey = "__" + materialName;

            for (const auto& texture : textures)
            {
                materialKey += "_" + texture;
            }

            Material material = materials[materialName];

            if (textures.size() != material.nbTextures)
            {
                LOG_ERROR("SimpleRenderer", "Material '" << materialName << "' has " << material.nbTextures <<
                    " textures, but " << textures.size() << " were provided.");
            }

            // Set the material of the call with the correct textures
            if (masterRenderer->hasMaterial(materialKey))
            {
                call.setMaterial(masterRenderer->getMaterialID(materialKey));
            }
            else
            {
                for (size_t i = 0; i < textures.size(); ++i)
                {
                    material.textureId[i] = masterRenderer->getTexture(textures[i]).id;
                }

                call.setMaterial(masterRenderer->registerMaterial(materialKey, material));
            }

            // Resize the data to the number of attributes
            call.data.resize(material.nbAttributes);
        }

        virtual void setupRenderer() = 0;

        virtual RenderCall createRenderCall(CompRef<Comps>... comps) = 0;

        std::queue<_unique_id> updateQueue;

        Own<SimpleRenderCall> renderCallOwner;

        std::unordered_map<std::string, Material> materials;
    };
}