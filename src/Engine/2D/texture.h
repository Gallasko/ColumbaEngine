#pragma once

#include "Renderer/renderer.h"

#include "2D/position.h"
#include "constant.h"

#include "logger.h"

namespace pg
{
    struct Texture2DComponent : public Ctor
    {
        Texture2DComponent(const std::string& textureName) : textureName(textureName) { }
        Texture2DComponent(const Texture2DComponent &rhs) : textureName(rhs.textureName), entityId(rhs.entityId), ecsRef(rhs.ecsRef), opacity(rhs.opacity), overlappingColor(rhs.overlappingColor), overlappingColorRatio(rhs.overlappingColorRatio) { }
        virtual ~Texture2DComponent() {}

        Texture2DComponent& operator=(const Texture2DComponent& other)
        {
            textureName = other.textureName;

            opacity = other.opacity;

            overlappingColor = other.overlappingColor;
            overlappingColorRatio = other.overlappingColorRatio;

            if (ecsRef)
            {
                ecsRef->sendEvent(EntityChangedEvent{entityId});
            }   

            return *this;
        }

        virtual void onCreation(EntityRef entity) override
        {
            ecsRef = entity->world();

            entityId = entity->id;
        }

        inline static std::string getType() { return "Texture2DComponent"; } 

        void setTexture(const std::string& textureName)
        {
            if (this->textureName != textureName)
            {
                this->textureName = textureName;

                if (ecsRef)
                {
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
                }   
            }
        }

        void setOverlappingColor(const constant::Vector3D& color, float ratio)
        {
            if (ratio != overlappingColorRatio or overlappingColor != color)
            {
                this->overlappingColor = color;
                this->overlappingColorRatio = ratio;

                if (ecsRef)
                {
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
                }   
            }
        }

        void setOpacity(float opacity)
        {
            if (this->opacity != opacity)
            {
                this->opacity = opacity;

                if (ecsRef)
                {
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
                }   
            }
        }

        void setViewport(size_t viewport)
        {
            if (this->viewport != viewport)
            {
                this->viewport = viewport;

                if (ecsRef)
                {
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
                }
            }
        }

        // Todo make those private
        std::string textureName;

        _unique_id entityId = 0;

        EntitySystem *ecsRef = nullptr;

        float opacity = 1.0f;

        constant::Vector3D overlappingColor = {0.0f, 0.0f, 0.0f};
        float overlappingColorRatio = 0.0f;

        size_t viewport = 0;
    };

    template <>
    void serialize(Archive& archive, const Texture2DComponent& value);

    template <>
    Texture2DComponent deserialize(const UnserializedObject& serializedString);

    struct TextureRenderCall
    {
        TextureRenderCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    struct Texture2DComponentSystem : public AbstractRenderer, System<Own<Texture2DComponent>, Own<TextureRenderCall>, Listener<EntityChangedEvent>, Ref<PositionComponent>, InitSys>
    {
        Texture2DComponentSystem(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render) { }

        virtual std::string getSystemName() const override { return "Ui Texture System"; }

        virtual void init() override;

        virtual void execute() override;

        RenderCall createRenderCall(CompRef<PositionComponent> ui, CompRef<Texture2DComponent> obj);

        virtual void onEvent(const EntityChangedEvent& event) override;

        void onEventUpdate(_unique_id entityId);

        // Use this material preset if a material is not specified when creating a texture component !
        Material baseMaterialPreset;

        // Use this material preset if a material is not specified when creating an atlas texture component !
        Material atlasMaterialPreset;

        std::queue<_unique_id> textureUpdateQueue;
    };

    /** Helper that create an entity with an Ui component and a Texture component */
    template <typename Type>
    CompList<PositionComponent, UiAnchor, Texture2DComponent> makeUiTexture(Type *ecs, float width, float height, const std::string& name)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto tex = ecs->template attach<Texture2DComponent>(entity, name);

        return CompList<PositionComponent, UiAnchor, Texture2DComponent>(entity, ui, anchor, tex);
    }

}