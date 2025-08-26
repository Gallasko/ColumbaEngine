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

    /// Creates a selection outline with resize handles at corners and edges
    inline CompList<PositionComponent, UiAnchor, Prefab> makeResizableSelectionOutline(
        EntitySystem* ecs,
        float thickness = 2.f,
        float handleSize = 8.f,
        const constant::Vector4D& outlineColor = {255.0f, 255.0f, 0.0f, 255.0f},
        const constant::Vector4D& handleColor = {255.0f, 255.0f, 255.0f, 255.0f},
        bool visible = true)
    {
        // Create the base outline
        auto outline = makeSelectionOutlinePrefab(ecs, thickness, outlineColor, visible);
        auto outlinePrefab = outline.get<Prefab>();

        // Helper function to create a resize handle
        auto createResizeHandle = [&](ResizeHandle handleType, AnchorType anchorX, AnchorType anchorY) {
            auto handle = makeUiSimple2DShape(
                ecs,
                Shape2D::Square,
                handleSize,
                handleSize,
                handleColor);

            handle.get<PositionComponent>()->setVisibility(visible);

            auto handleAnchor = handle.get<UiAnchor>();
            auto handleResizeComp = ecs->attach<ResizeHandleComponent>(handle.entity);
            handleResizeComp->handle = handleType;
            handleResizeComp->handleSize = handleSize;

            // Position the handle based on the anchor types
            if (anchorX == AnchorType::Left)
                handleAnchor->setLeftAnchor({outline.entity.id, AnchorType::Left, -handleSize/2});
            else if (anchorX == AnchorType::Right)
                handleAnchor->setRightAnchor({outline.entity.id, AnchorType::Right, -handleSize/2});
            else // Center
                handleAnchor->setHorizontalCenter({outline.entity.id, AnchorType::HorizontalCenter});

            if (anchorY == AnchorType::Top)
                handleAnchor->setTopAnchor({outline.entity.id, AnchorType::Top, -handleSize/2});
            else if (anchorY == AnchorType::Bottom)
                handleAnchor->setBottomAnchor({outline.entity.id, AnchorType::Bottom, -handleSize/2});
            else // Center
                handleAnchor->setVerticalCenter({outline.entity.id, AnchorType::VerticalCenter});

            handleAnchor->setZConstrain(PosConstrain{outline.entity.id, AnchorType::Z, PosOpType::Add, 1.0f});

            outlinePrefab->addToPrefab(handle.entity);
        };

        // Create the 8 resize handles
        createResizeHandle(ResizeHandle::TopLeft, AnchorType::Left, AnchorType::Top);
        createResizeHandle(ResizeHandle::Top, AnchorType::HorizontalCenter, AnchorType::Top);
        createResizeHandle(ResizeHandle::TopRight, AnchorType::Right, AnchorType::Top);
        createResizeHandle(ResizeHandle::Left, AnchorType::Left, AnchorType::VerticalCenter);
        createResizeHandle(ResizeHandle::Right, AnchorType::Right, AnchorType::VerticalCenter);
        createResizeHandle(ResizeHandle::BottomLeft, AnchorType::Left, AnchorType::Bottom);
        createResizeHandle(ResizeHandle::Bottom, AnchorType::HorizontalCenter, AnchorType::Bottom);
        createResizeHandle(ResizeHandle::BottomRight, AnchorType::Right, AnchorType::Bottom);

        // Create rotation handle above the entity
        auto rotationHandle = makeUiSimple2DShape(
            ecs,
            Shape2D::Square,
            handleSize,
            handleSize,
            {100.0f, 200.0f, 100.0f, 255.0f}); // Green color for rotation
        rotationHandle.get<PositionComponent>()->setVisibility(visible);
        auto rotHandleAnchor = rotationHandle.get<UiAnchor>();
        auto rotHandleComp = ecs->attach<RotationHandleComponent>(rotationHandle.entity);
        rotHandleComp->handle = RotationHandle::Rotation;
        rotHandleComp->handleSize = handleSize;
        rotHandleComp->distance = 40.0f;

        // Position rotation handle above the top center (above the top resize handle)
        rotHandleAnchor->setHorizontalCenter({outline.entity.id, AnchorType::HorizontalCenter});
        rotHandleAnchor->setTopAnchor({outline.entity.id, AnchorType::Top, -rotHandleComp->distance - handleSize});
        rotHandleAnchor->setZConstrain(PosConstrain{outline.entity.id, AnchorType::Z, PosOpType::Add, 1.0f});
        outlinePrefab->addToPrefab(rotationHandle.entity);

        return outline;
    }
}
