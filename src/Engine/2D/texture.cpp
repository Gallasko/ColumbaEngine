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

    void Texture2DComponentSystem::setup()
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
    }

    RenderCall Texture2DComponentSystem::createRenderCall(CompRef<Texture2DComponent> obj, CompRef<PositionComponent> ui)
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

}