#include "stdafx.h"

#include "texture.h"

#include "logger.h"
#include "serialization.h"

#include "Renderer/renderer.h"

#include "Helpers/openglobject.h"

#include "Compiler/ecsserialization.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Texture";

        std::vector<std::string> split(const std::string &s, char delim)
        {
            std::vector<std::string> result;
            std::stringstream ss (s);
            std::string item;

            while (getline (ss, item, delim))
            {
                result.push_back (item);
            }

            return result;
        }
    }

    void Texture2DComponent::setTexture(const std::string& textureName)
    {
        if (this->textureName != textureName)
        {
            this->textureName = textureName;

            if (ecsRef)
            {
                ecsRef->sendEvent(TextureChangedEvent{entityId});
            }
        }
    }

    void Texture2DComponent::setOverlappingColor(const constant::Vector3D& color, float ratio)
    {
        if (areNotAlmostEqual(ratio, overlappingColorRatio) or overlappingColor != color)
        {
            this->overlappingColor = color;
            this->overlappingColorRatio = ratio;

            if (ecsRef)
            {
                ecsRef->sendEvent(TextureChangedEvent{entityId});
            }
        }
    }

    void Texture2DComponentSystem::init()
    {
        baseMaterialPreset.shader = masterRenderer->getShader("simpleTexture");

        baseMaterialPreset.nbTextures = 1;

        baseMaterialPreset.uniformMap.emplace("sWidth", "ScreenWidth");
        baseMaterialPreset.uniformMap.emplace("sHeight", "ScreenHeight");

        baseMaterialPreset.setSimpleMesh({3, 2, 1, 1, 3, 1});

        atlasMaterialPreset.shader = masterRenderer->getShader("atlasTexture");

        atlasMaterialPreset.nbTextures = 1;

        atlasMaterialPreset.uniformMap.emplace("sWidth", "ScreenWidth");
        atlasMaterialPreset.uniformMap.emplace("sHeight", "ScreenHeight");

        atlasMaterialPreset.setSimpleMesh({3, 2, 1, 4, 1, 3, 1});

        auto group = registerGroup<PositionComponent, Texture2DComponent>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE("Texture 2D System", "Add entity " << entity->id << " to ui - texture 2D group !");

            auto pos = entity->get<PositionComponent>();
            auto tex = entity->get<Texture2DComponent>();

            entityRenderCalls[entity->id] = createRenderCall(pos, tex);
            entitiesInRenderGroup.push_back(entity->id);

            std::sort(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end());

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_INFO("Texture 2D System", "Remove entity " << id << " of ui - texture 2D group !");

            // Remove render call from the map
            entityRenderCalls.erase(id);
            entitiesInRenderGroup.erase(std::remove(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), id), entitiesInRenderGroup.end());

            changed = true;
        });
    }

    void Texture2DComponentSystem::execute()
    {
        if (not changed)
        {
            return;
        }

        std::vector<_unique_id> updateQueue;
        std::vector<_unique_id> temp;

        temp.assign(textureUpdateSet.begin(), textureUpdateSet.end());

        std::sort(temp.begin(), temp.end());

        std::set_intersection(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), temp.begin(), temp.end(),
                          std::back_inserter(updateQueue));

        int processedCount = 0;
        int failedCount = 0;

        for (const auto& entityId : updateQueue)
        {
            LOG_MILE(DOM, "Processing entity ID: " << entityId);

            auto entity = ecsRef->getEntity(entityId);

            if (not entity)
            {
                LOG_WARNING(DOM, "Entity " << entityId << " NOT FOUND in ECS! Skipping...");
                failedCount++;
                continue;
            }

            LOG_MILE(DOM, "Entity " << entityId << " found successfully");

            auto ui = entity->get<PositionComponent>();
            auto obj = entity->get<Texture2DComponent>();

            LOG_MILE(DOM, "Position - x: " << ui->x << ", y: " << ui->y
                     << ", width: " << ui->width << ", height: " << ui->height
                     << ", visible: " << ui->visible);
            LOG_MILE(DOM, "Texture name: " << obj->textureName);

            // Store render call directly in the system's map (no component needed!)
            entityRenderCalls[entityId] = createRenderCall(ui, obj);

            processedCount++;
        }

        renderCallList.clear();

        // Build render call list from the system's map
        renderCallList.reserve(entityRenderCalls.size());

        LOG_MILE(DOM, "Building render call list from " << entityRenderCalls.size() << " stored render calls");

        for (const auto& [entityId, renderCall] : entityRenderCalls)
        {
            renderCallList.push_back(renderCall);
        }

        finishChanges();
    }

    RenderCall Texture2DComponentSystem::createRenderCall(CompRef<PositionComponent> ui, CompRef<Texture2DComponent> obj)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        call.setRenderStage(renderStage);

        call.setViewport(obj->viewport);

        auto textureName = split(obj->textureName, '.');

        // No '.' detected in the texture name so it is not an atlas texture, proceed to generate a simple texture
        if (textureName.size() == 1)
        {
            if (masterRenderer->hasMaterial(obj->textureName))
            {
                call.setMaterial(masterRenderer->getMaterialID(obj->textureName));
            }
            else
            {
                Material simpleShapeMaterial = baseMaterialPreset;

                simpleShapeMaterial.textureId[0] = masterRenderer->getTexture(obj->textureName).id;

                call.setMaterial(masterRenderer->registerMaterial(obj->textureName, simpleShapeMaterial));
            }

            if (masterRenderer->getTexture(obj->textureName).transparent)
            {
                call.setOpacity(OpacityType::Additive);
            }
            else
            {
                call.setOpacity(OpacityType::Opaque);
            }

            call.data.resize(11);

            call.data[0] = ui->x;
            call.data[1] = ui->y;
            call.data[2] = ui->z;
            call.data[3] = ui->width;
            call.data[4] = ui->height;
            call.data[5] = ui->rotation;
            call.data[6] = obj->opacity;
            call.data[7] = obj->overlappingColor.x;
            call.data[8] = obj->overlappingColor.y;
            call.data[9] = obj->overlappingColor.z;
            call.data[10] = obj->overlappingColorRatio;
        }
        // A '.' was detected, textureName referres to an atlas texture
        else if (textureName.size() == 2)
        {
            auto baseTexture = textureName[0];
            auto atlasTextureName = textureName[1];

            if (masterRenderer->hasMaterial(baseTexture))
            {
                call.setMaterial(masterRenderer->getMaterialID(baseTexture));
            }
            else
            {
                Material altasShapeMaterial = atlasMaterialPreset;

                altasShapeMaterial.textureId[0] = masterRenderer->getTexture(baseTexture).id;

                call.setMaterial(masterRenderer->registerMaterial(baseTexture, altasShapeMaterial));
            }

            if (masterRenderer->getTexture(baseTexture).transparent)
            {
                call.setOpacity(OpacityType::Additive);
            }
            else
            {
                call.setOpacity(OpacityType::Opaque);
            }

            auto atlasTexture = masterRenderer->getAtlasTexture(baseTexture, atlasTextureName);

            auto limits = atlasTexture.getTextureLimit();

            call.data.resize(15);

            call.data[0] = ui->x;
            call.data[1] = ui->y;
            call.data[2] = ui->z;
            call.data[3] = ui->width;
            call.data[4] = ui->height;
            call.data[5] = ui->rotation;
            call.data[6] = limits.x;
            call.data[7] = limits.y;
            call.data[8] = limits.z;
            call.data[9] = limits.w;
            call.data[10] = obj->opacity;
            call.data[11] = obj->overlappingColor.x;
            call.data[12] = obj->overlappingColor.y;
            call.data[13] = obj->overlappingColor.z;
            call.data[14] = obj->overlappingColorRatio;
        }
        else
        {
            LOG_ERROR(DOM, "Invalid texture name: " << obj->textureName << ", skipping this");

            call.setVisibility(false);
            call.data.resize(0);
        }

        return call;
    }

    void Texture2DComponentSystem::onEvent(const PositionComponentChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void Texture2DComponentSystem::onEvent(const TextureChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void Texture2DComponentSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        textureUpdateSet.insert(entityId);

        changed = true;
    }
}