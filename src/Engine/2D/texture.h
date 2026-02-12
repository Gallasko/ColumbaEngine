#pragma once

#include "Renderer/renderer.h"

#include "2D/position.h"
#include "pgconstant.h"

#include "logger.h"

#include "Components/Texture2DComponent.generated.h"

namespace pg
{
    struct TextureRenderCall
    {
        TextureRenderCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    struct Texture2DComponentSystem : public AbstractRenderer, System<Own<Texture2DComponent>, Own<TextureRenderCall>, Listener<PositionComponentChangedEvent>, Listener<TextureChangedEvent>, Ref<PositionComponent>, InitSys>
    {
        Texture2DComponentSystem(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render) { }

        virtual std::string getSystemName() const override { return "Ui Texture System"; }

        virtual void init() override;

        virtual void execute() override;

        RenderCall createRenderCall(CompRef<PositionComponent> ui, CompRef<Texture2DComponent> obj);

        virtual void onEvent(const PositionComponentChangedEvent& event) override;
        virtual void onEvent(const TextureChangedEvent& event) override;

        void onEventUpdate(_unique_id entityId);

        // Use this material preset if a material is not specified when creating a texture component !
        Material baseMaterialPreset;

        // Use this material preset if a material is not specified when creating an atlas texture component !
        Material atlasMaterialPreset;

        std::queue<_unique_id> textureUpdateQueue;
    };

    /** Helper that create an entity with a Pos component and a Texture component */
    template <typename Type>
    CompList<PositionComponent, Texture2DComponent> make2DTexture(Type *ecs, float width, float height, const std::string& name)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto tex = ecs->template attach<Texture2DComponent>(entity, name);

        return CompList<PositionComponent, Texture2DComponent>(entity, ui, tex);
    }

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