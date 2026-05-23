#pragma once

#include "Renderer/genericrendersys.h"

#include "2D/position.h"
#include "pgconstant.h"

#include "logger.h"

#include "Components/Texture2DComponent.generated.h"
#include "Components/ViewportComponent.generated.h"

namespace pg
{
    struct Texture2DComponentSystem : public GenericRenderSystem<Texture2DComponent, TextureChangedEvent, PositionComponent, PositionSettledEvent, ViewportComponent, ViewportComponentChangedEvent>
    {
        Texture2DComponentSystem(MasterRenderer* masterRenderer) : GenericRenderSystem(masterRenderer) { }

        virtual std::string getSystemName() const override { return "Ui Texture System"; }

        virtual void setup() override;

        virtual RenderCall createRenderCall(CompRef<Texture2DComponent> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp) override;

        // Use this material preset if a material is not specified when creating a texture component !
        Material baseMaterialPreset;

        // Use this material preset if a material is not specified when creating an atlas texture component !
        Material atlasMaterialPreset;
    };

    /** Helper that create an entity with a Pos component and a Texture component */
    template <typename Type>
    CompList<PositionComponent, ViewportComponent, Texture2DComponent> make2DTexture(Type *ecs, float width, float height, const std::string& name)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto tex = ecs->template attach<Texture2DComponent>(entity, name);

        return {entity, ui, vp, tex};
    }

    /** Helper that create an entity with an Ui component and a Texture component */
    template <typename Type>
    CompList<PositionComponent, UiAnchor, ViewportComponent, Texture2DComponent> makeUiTexture(Type *ecs, float width, float height, const std::string& name)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto tex = ecs->template attach<Texture2DComponent>(entity, name);

        return {entity, ui, anchor, vp, tex};
    }

}