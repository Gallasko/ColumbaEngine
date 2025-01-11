#include "texture.h"

#include "logger.h"
#include "serialization.h"

#include "Renderer/renderer.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "Helpers/openglobject.h"

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

    template <>
    void serialize(Archive& archive, const Texture2DComponent& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(Texture2DComponent::getType());

        serialize(archive, "textureName", value.textureName);
        serialize(archive, "opacity", value.opacity);
        serialize(archive, "overlappingColor", value.overlappingColor);
        serialize(archive, "overlappingRatio", value.overlappingColorRatio);
    
        archive.endSerialization();
    }

    template <>
    Texture2DComponent deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an Texture2DComponent");

            auto textureName = deserialize<std::string>(serializedString["textureName"]);

            auto opacity = deserialize<float>(serializedString["opacity"]);

            auto overlappingColor = deserialize<constant::Vector3D>(serializedString["overlappingColor"]);
            auto overlappingColorRatio = deserialize<float>(serializedString["overlappingRatio"]);

            auto texture = Texture2DComponent{textureName};

            texture.opacity = opacity;
            texture.overlappingColor = overlappingColor;
            texture.overlappingColorRatio = overlappingColorRatio;

            return texture;
        }

        return Texture2DComponent{""};
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

        auto group = registerGroup<UiComponent, Texture2DComponent>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE("Texture 2D System", "Add entity " << entity->id << " to ui - texture 2D group !");

            auto ui = entity->get<UiComponent>();
            auto shape = entity->get<Texture2DComponent>();

            ecsRef->attach<TextureRenderCall>(entity, createRenderCall(ui, shape));

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE("Texture 2D System", "Remove entity " << id << " of ui - texture 2D group !");

            auto entity = ecsRef->getEntity(id);

            ecsRef->detach<TextureRenderCall>(entity);

            changed = true;
        });
    }

    void Texture2DComponentSystem::execute()
    {
        if (not changed)
            return;

        renderCallList.clear();

        const auto& renderCallView = view<TextureRenderCall>();

        renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            renderCallList.push_back(renderCall->call);
        }

        changed = false;
    }

    RenderCall Texture2DComponentSystem::createRenderCall(CompRef<UiComponent> ui, CompRef<Texture2DComponent> obj)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processUiComponent(ui);

        call.setRenderStage(renderStage);

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

            call.data[0] = ui->pos.x;
            call.data[1] = ui->pos.y;
            call.data[2] = ui->pos.z;
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

            call.data[0] = ui->pos.x;
            call.data[1] = ui->pos.y;
            call.data[2] = ui->pos.z;
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

    void Texture2DComponentSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void Texture2DComponentSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(entityId);
        
        if (not entity or not entity->has<TextureRenderCall>())
            return; 

        auto ui = entity->get<UiComponent>();
        auto shape = entity->get<Texture2DComponent>();

        entity->get<TextureRenderCall>()->call = createRenderCall(ui, shape);

        changed = true;
    }
}