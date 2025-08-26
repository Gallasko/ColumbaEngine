#pragma once

#include "ECS/system.h"
#include "UI/prefab.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"

// Todo add this in /Prefab instead

namespace pg
{
    /// Creates a 1‑entity prefab that draws a hollow rectangle by
    /// composing four thin rect shapes.  You can then toggle
    /// prefab.get<Prefab>()->setVisibility(true/false).
    inline CompList<PositionComponent, UiAnchor, Prefab> makeSelectionOutlinePrefab(EntitySystem* ecs, float thickness, const constant::Vector4D& color={0.0f, 0.0f, 0.0f, 255.0f}, bool visible = true)
    {
        // 1) Create the root prefab (anchored at 0,0 by default).
        auto outline = makeAnchoredPrefab(ecs, 0.f, 0.f);
        auto outlinePrefab = outline.get<Prefab>();
        outlinePrefab->setVisibility(visible);
        auto outlineAnchor = outline.get<UiAnchor>();

        // 2) Top edge
        {
            auto edge = makeUiSimple2DShape(
                ecs,
                Shape2D::Square,
                /*w=*/1.f,         // width will be driven by constraint
                /*h=*/thickness,
                color);

            edge.get<PositionComponent>()->setVisibility(visible);

            auto a = edge.get<UiAnchor>();
            a->setTopAnchor(outlineAnchor->top);
            a->setLeftAnchor(outlineAnchor->left);
            a->setWidthConstrain(PosConstrain{outline.entity.id, AnchorType::Width});
            a->setZConstrain(PosConstrain{outline.entity.id, AnchorType::Z});

            outlinePrefab->addToPrefab(edge.entity);
        }

        // 3) Bottom edge
        {
            auto edge = makeUiSimple2DShape(
                ecs,
                Shape2D::Square,
                /*w=*/1.f,
                /*h=*/thickness,
                color);

            edge.get<PositionComponent>()->setVisibility(visible);

            auto a = edge.get<UiAnchor>();
            a->setBottomAnchor(outlineAnchor->bottom);
            a->setLeftAnchor(outlineAnchor->left);
            a->setWidthConstrain(PosConstrain{outline.entity.id, AnchorType::Width});
            a->setZConstrain(PosConstrain{outline.entity.id, AnchorType::Z});

            outlinePrefab->addToPrefab(edge.entity);
        }

        // 4) Left edge
        {
            auto edge = makeUiSimple2DShape(
                ecs,
                Shape2D::Square,
                /*w=*/thickness,
                /*h=*/1.f,
                color);

                edge.get<PositionComponent>()->setVisibility(visible);

            auto a = edge.get<UiAnchor>();
            a->setTopAnchor(outlineAnchor->top);
            a->setLeftAnchor(outlineAnchor->left);
            a->setHeightConstrain(PosConstrain{outline.entity.id, AnchorType::Height});
            a->setZConstrain(PosConstrain{outline.entity.id, AnchorType::Z});

            outlinePrefab->addToPrefab(edge.entity);
        }

        // 5) Right edge
        {
            auto edge = makeUiSimple2DShape(
                ecs,
                Shape2D::Square,
                /*w=*/thickness,
                /*h=*/1.f,
                color);

            edge.get<PositionComponent>()->setVisibility(visible);

            auto a = edge.get<UiAnchor>();
            a->setTopAnchor(outlineAnchor->top);
            a->setRightAnchor(outlineAnchor->right);
            a->setHeightConstrain(PosConstrain{outline.entity.id, AnchorType::Height});
            a->setZConstrain(PosConstrain{outline.entity.id, AnchorType::Z});

            outlinePrefab->addToPrefab(edge.entity);
        }

        return outline;
    }
}
