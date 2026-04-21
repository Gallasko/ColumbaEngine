#pragma once

#include "logger.h"

#include "position.h"

#include "Renderer/genericrendersys.h"

#include "pgconstant.h"

#include "Components/Simple2DObject.generated.h"
#include "Components/RoundedRect2DObject.generated.h"
#include "Components/ViewportComponent.generated.h"

// Todo this system can be faulty during scene changing !

namespace pg
{
    struct Simple2DObjectSystem : public GenericRenderSystem<Simple2DObject, Simple2DObjectChangedEvent, PositionComponent, PositionComponentChangedEvent, ViewportComponent, ViewportComponentChangedEvent>
    {
        Simple2DObjectSystem(MasterRenderer* masterRenderer) : GenericRenderSystem(masterRenderer) { }
        virtual ~Simple2DObjectSystem() { }

        virtual std::string getSystemName() const override { return "Shape 2D System"; }

        virtual void setup() override;

        virtual RenderCall createRenderCall(CompRef<Simple2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp) override;

        uint64_t materialId = 0;
    };

    template <typename Type>
    CompList<PositionComponent, ViewportComponent, Simple2DObject> makeSimple2DShape(Type *ecs, const Shape2D& shape, float width = 0.0f, float height = 0.0f, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        LOG_THIS("Simple 2D Shape");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto tex = ecs->template attach<Simple2DObject>(entity, shape, colors);

        return {entity, ui, vp, tex};
    }

    template <typename Type>
    CompList<PositionComponent, UiAnchor, ViewportComponent, Simple2DObject> makeUiSimple2DShape(Type *ecs, const Shape2D& shape, float width = 0.0f, float height = 0.0f, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        LOG_THIS("Simple 2D Shape");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto tex = ecs->template attach<Simple2DObject>(entity, shape, colors);

        return {entity, ui, anchor, vp, tex};
    }

    // ---------------------------------------------------------------------------
    // Rounded rectangle
    // ---------------------------------------------------------------------------

    struct RoundedRect2DObjectSystem : public GenericRenderSystem<RoundedRect2DObject, RoundedRect2DObjectChangedEvent, PositionComponent, PositionComponentChangedEvent, ViewportComponent, ViewportComponentChangedEvent>
    {
        RoundedRect2DObjectSystem(MasterRenderer* masterRenderer) : GenericRenderSystem(masterRenderer) {}
        virtual ~RoundedRect2DObjectSystem() {}

        virtual std::string getSystemName() const override { return "Rounded Rect 2D System"; }

        virtual void setup() override;

        virtual RenderCall createRenderCall(CompRef<RoundedRect2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp) override;

        uint64_t materialId = 0;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, ViewportComponent, RoundedRect2DObject> makeRoundedRect2DShape(Type* ecs, float cornerRadius, float width = 0.0f, float height = 0.0f, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);
        ui->setWidth(width);
        ui->setHeight(height);

        auto anchor = ecs->template attach<UiAnchor>(entity);
        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto obj = ecs->template attach<RoundedRect2DObject>(entity, cornerRadius, colors);

        return {entity, ui, anchor, vp, obj};
    }
}