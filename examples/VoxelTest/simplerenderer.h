#pragma once

#include <algorithm>
#include <unordered_set>

#include "Renderer/renderer.h"
#include "ECS/entitysystem_fwd.h"  // for ResizeEvent

namespace pg
{
    template <typename... Comps>
    struct SimpleRenderer : public AbstractRenderer, public System<InitSys, Listener<EntityChangedEvent>, Listener<ResizeEvent>>
    {
        SimpleRenderer(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render)
        {
        }

        virtual void setupRenderer() = 0;

        virtual void init() override final
        {
            setupRenderer();

            auto group = registerGroup<Comps...>();

            group->addOnGroup([this](EntityRef entity) {
                entityRenderCalls[entity->id] = createRenderCall(entity->get<Comps>()...);
                entitiesInRenderGroup.push_back(entity->id);

                std::sort(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end());

                changed = true;
            });

            group->removeOfGroup([this](EntitySystem*, _unique_id id) {
                entityRenderCalls.erase(id);
                entitiesInRenderGroup.erase(std::remove(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), id), entitiesInRenderGroup.end());

                changed = true;
            });
        }

        virtual void execute() override final
        {
            if (not changed)
                return;

            std::vector<_unique_id> updateQueue;
            std::vector<_unique_id> temp;

            temp.assign(updateSet.begin(), updateSet.end());

            std::sort(temp.begin(), temp.end());

            std::set_intersection(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), temp.begin(), temp.end(),
                            std::back_inserter(updateQueue));

            updateSet.clear();

            for (const auto& entityId : updateQueue)
            {
                auto entity = ecsRef->getEntity(entityId);

                if (not entity)
                {
                    LOG_WARNING("SimpleRenderer", "Entity " << entityId << " NOT FOUND in ECS! Skipping...");
                    continue;
                }

                entityRenderCalls[entityId] = createRenderCall(entity->get<Comps>()...);
            }

            renderCallList.clear();

            renderCallList.reserve(entityRenderCalls.size());

            for (const auto& [entityId, renderCall] : entityRenderCalls)
            {
                renderCallList.push_back(renderCall);
            }

            finishChanges();
        }

        virtual RenderCall createRenderCall(CompRef<Comps>... comps) = 0;

        virtual void onEvent(const EntityChangedEvent& event) override final
        {
            updateSet.insert(event.id);

            changed = true;
        }

        virtual void onEvent(const ResizeEvent& event) override final
        {
            onResize(event.width, event.height);
        }

        virtual void onResize(float /*width*/, float /*height*/) {}

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

            call.data.resize(material.nbAttributes);
        }

        std::unordered_set<_unique_id> updateSet;

        std::unordered_map<_unique_id, RenderCall> entityRenderCalls;
        std::vector<_unique_id> entitiesInRenderGroup;

        std::unordered_map<std::string, Material> materials;
    };
}
