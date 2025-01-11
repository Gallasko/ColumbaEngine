#pragma once

#include "Renderer/renderer.h"

#include "UI/uisystem.h"

namespace pg
{
    enum class ProgressBarFillDirection : uint8_t
    {
        // Todo make it go through different cardinal direction ! (Add to the right to the left to up to down)
        Horizontal = 0,
        Vertical = 1,
        // Todo ! For this to work we need to add a grayscale texture and the different opacity of the texture to cycle through all the different fill position
        Custom
    };

    struct ProgressBarComponent : public Ctor
    {
        ProgressBarComponent(const std::string& emptyTextureName, const std::string& fullTextureName, float fillRatio = 1.0f) : emptyTextureName(emptyTextureName), fullTextureName(fullTextureName), percent(fillRatio) { }
        ProgressBarComponent(const ProgressBarComponent &rhs) : emptyTextureName(rhs.emptyTextureName), fullTextureName(rhs.fullTextureName), entityId(rhs.entityId), ecsRef(rhs.ecsRef), percent(rhs.percent), direction(rhs.direction) { }
        virtual ~ProgressBarComponent() {}

        ProgressBarComponent& operator=(const ProgressBarComponent& other)
        {
            emptyTextureName = other.emptyTextureName;
            fullTextureName = other.fullTextureName;

            percent = other.percent;
            direction = other.direction;

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

        inline static std::string getType() { return "ProgressBarComponent"; } 

        void setFillPercent(float percent)
        {
            if (this->percent != percent)
            {
                this->percent = percent;

                if (ecsRef)
                {
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
                }   
            }
        }

        void setFillDirection(const ProgressBarFillDirection& direction)
        {
            if (this->direction != direction)
            {
                this->direction = direction;

                if (ecsRef)
                {
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
                }   
            }
        }

        // Todo make those private
        std::string emptyTextureName;
        std::string fullTextureName;

        _unique_id entityId = 0;

        EntitySystem *ecsRef = nullptr;

        float percent = 1.0f;

        ProgressBarFillDirection direction = ProgressBarFillDirection::Horizontal;
    };

    template <>
    void serialize(Archive& archive, const ProgressBarComponent& value);

    template <>
    ProgressBarComponent deserialize(const UnserializedObject& serializedString);

    struct ProgressBarRenderCall
    {
        ProgressBarRenderCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    struct ProgressBarComponentSystem : public AbstractRenderer, System<Own<ProgressBarComponent>, Own<ProgressBarRenderCall>, Listener<EntityChangedEvent>, Ref<UiComponent>, NamedSystem, InitSys>
    {
        ProgressBarComponentSystem(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render) { }

        virtual std::string getSystemName() const override { return "Progress Bar System"; }

        virtual void init() override;

        virtual void execute() override;

        RenderCall createRenderCall(CompRef<UiComponent> ui, CompRef<ProgressBarComponent> obj);

        virtual void onEvent(const EntityChangedEvent& event) override;

        void onEventUpdate(_unique_id entityId);

        // Use this material preset if a material is not specified when creating a texture component !
        Material baseMaterialPreset;

        // Use this material preset if a material is not specified when creating an atlas texture component !
        Material atlasMaterialPreset;
    };

    /** Helper that create an entity with an Ui component and a Progress Bar component */
    template <typename Type>
    CompList<UiComponent, ProgressBarComponent> makeProgressBar(Type *ecs, float width, float height, const std::string& emptyTexture, const std::string& fullTexture, float fillRatio = 1.0f)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<UiComponent>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto tex = ecs->template attach<ProgressBarComponent>(entity, emptyTexture, fullTexture, fillRatio);

        return CompList<UiComponent, ProgressBarComponent>(entity, ui, tex);
    }
}