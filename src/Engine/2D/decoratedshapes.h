#pragma once

#include "logger.h"

#include "position.h"

#include "Renderer/genericrendersys.h"

#include "pgconstant.h"

#include "Components/HatchRect2DObject.generated.h"
#include "Components/DottedLine2DObject.generated.h"
#include "Components/ViewportComponent.generated.h"

namespace pg
{
    // ---------------------------------------------------------------------------
    // Hatch fill (45-degree by default)
    // ---------------------------------------------------------------------------

    struct HatchRect2DObjectSystem : public GenericRenderSystem<HatchRect2DObject, HatchRect2DObjectChangedEvent, PositionComponent, PositionSettledEvent, ViewportComponent, ViewportComponentChangedEvent>
    {
        HatchRect2DObjectSystem(MasterRenderer* masterRenderer) : GenericRenderSystem(masterRenderer) {}
        virtual ~HatchRect2DObjectSystem() {}

        virtual std::string getSystemName() const override { return "Hatch Rect 2D System"; }

        virtual void setup() override;

        virtual RenderCall createRenderCall(CompRef<HatchRect2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp) override;

        uint64_t materialId = 0;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, ViewportComponent, HatchRect2DObject> makeHatchRect2DShape(Type* ecs, float width, float height, const constant::Vector4D& colors, float spacing = 6.0f, float lineWidth = 2.0f)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);
        ui->setWidth(width);
        ui->setHeight(height);

        auto anchor = ecs->template attach<UiAnchor>(entity);
        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto obj = ecs->template attach<HatchRect2DObject>(entity, colors, spacing, lineWidth);

        return {entity, ui, anchor, vp, obj};
    }

    // ---------------------------------------------------------------------------
    // Dotted leader
    // ---------------------------------------------------------------------------

    struct DottedLine2DObjectSystem : public GenericRenderSystem<DottedLine2DObject, DottedLine2DObjectChangedEvent, PositionComponent, PositionSettledEvent, ViewportComponent, ViewportComponentChangedEvent>
    {
        DottedLine2DObjectSystem(MasterRenderer* masterRenderer) : GenericRenderSystem(masterRenderer) {}
        virtual ~DottedLine2DObjectSystem() {}

        virtual std::string getSystemName() const override { return "Dotted Line 2D System"; }

        virtual void setup() override;

        virtual RenderCall createRenderCall(CompRef<DottedLine2DObject> obj, CompRef<PositionComponent> ui, CompRef<ViewportComponent> vp) override;

        uint64_t materialId = 0;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, ViewportComponent, DottedLine2DObject> makeDottedLine2DShape(Type* ecs, float length, const constant::Vector4D& colors, float period = 4.0f, float dotRadius = 0.75f)
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);
        ui->setWidth(length);
        ui->setHeight(2.0f * dotRadius + 1.0f);

        auto anchor = ecs->template attach<UiAnchor>(entity);
        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto obj = ecs->template attach<DottedLine2DObject>(entity, colors, period, dotRadius);

        return {entity, ui, anchor, vp, obj};
    }
}
