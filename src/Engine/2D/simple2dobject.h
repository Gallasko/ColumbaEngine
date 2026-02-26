#pragma once

#include "logger.h"

#include "position.h"

#include "Renderer/renderer.h"

#include "pgconstant.h"

#include "Components/Simple2DObject.generated.h"

#include <unordered_set>

// Todo this system can be faulty during scene changing !

namespace pg
{
    struct Simple2DObjectSystem : public AbstractRenderer, System<Own<Simple2DObject>, Listener<PositionComponentChangedEvent>, Listener<Simple2DObjectChangedEvent>, InitSys>
    {
        Simple2DObjectSystem(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render) { }
        virtual ~Simple2DObjectSystem() { }

        virtual std::string getSystemName() const override { return "Shape 2D System"; }

        virtual void init() override;

        virtual void execute() override;

        RenderCall createRenderCall(CompRef<PositionComponent> ui, CompRef<Simple2DObject> obj);

        virtual void onEvent(const PositionComponentChangedEvent& event) override;
        virtual void onEvent(const Simple2DObjectChangedEvent& event) override;
        void onEventUpdate(_unique_id entityId);

        uint64_t materialId = 0;

        std::unordered_set<_unique_id> shapeUpdateSet;
        std::unordered_map<_unique_id, RenderCall> entityRenderCalls;
        std::vector<_unique_id> entitiesInRenderGroup;
    };

    template <typename Type>
    CompList<PositionComponent, Simple2DObject> makeSimple2DShape(Type *ecs, const Shape2D& shape, float width = 0.0f, float height = 0.0f, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        LOG_THIS("Simple 2D Shape");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto tex = ecs->template attach<Simple2DObject>(entity, shape, colors);

        return {entity, ui, tex};
    }

    template <typename Type>
    CompList<PositionComponent, UiAnchor, Simple2DObject> makeUiSimple2DShape(Type *ecs, const Shape2D& shape, float width = 0.0f, float height = 0.0f, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        LOG_THIS("Simple 2D Shape");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        ui->setWidth(width);
        ui->setHeight(height);

        auto tex = ecs->template attach<Simple2DObject>(entity, shape, colors);

        return {entity, ui, anchor, tex};
    }

    // ---------------------------------------------------------------------------
    // Rounded rectangle
    // ---------------------------------------------------------------------------

    struct RoundedRect2DObject : public Ctor
    {
        RoundedRect2DObject(float cornerRadius = 0.0f) : cornerRadius(cornerRadius) {}
        RoundedRect2DObject(float cornerRadius, const constant::Vector4D& c) : cornerRadius(cornerRadius), colors(c) {}

        RoundedRect2DObject(const RoundedRect2DObject& rhs)
            : cornerRadius(rhs.cornerRadius), colors(rhs.colors), viewport(rhs.viewport), id(rhs.id), ecsRef(rhs.ecsRef) {}
        virtual ~RoundedRect2DObject() {}

        inline static std::string getType() { return "RoundedRect2DObject"; }

        virtual void onCreation(EntityRef entity) { id = entity.id; ecsRef = entity.ecsRef; }

        void setColors(const constant::Vector4D& c)
        {
            colors = c;
            if (ecsRef) ecsRef->sendEvent(EntityChangedEvent{id});
        }

        void setOpacity(float alpha)
        {
            colors.w = alpha;
            if (ecsRef) ecsRef->sendEvent(EntityChangedEvent{id});
        }

        void setCornerRadius(float radius)
        {
            cornerRadius = radius;
            if (ecsRef) ecsRef->sendEvent(EntityChangedEvent{id});
        }

        void setViewport(size_t vp)
        {
            if (viewport != vp)
            {
                viewport = vp;
                if (ecsRef) ecsRef->sendEvent(EntityChangedEvent{id});
            }
        }

        float cornerRadius = 0.0f;
        constant::Vector4D colors {255.0f, 255.0f, 255.0f, 255.0f};
        size_t viewport = 0;
        _unique_id id;
        EntitySystem* ecsRef = nullptr;
    };

    template <>
    void serialize(Archive& archive, const RoundedRect2DObject& value);

    template <>
    RoundedRect2DObject deserialize(const UnserializedObject& serializedString);

    struct RoundedRect2DRenderCall
    {
        RoundedRect2DRenderCall(const RenderCall& call) : call(call) {}
        RenderCall call;
    };

    struct RoundedRect2DObjectSystem : public AbstractRenderer, System<Own<RoundedRect2DObject>, Own<RoundedRect2DRenderCall>, Listener<EntityChangedEvent>, InitSys>
    {
        RoundedRect2DObjectSystem(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render) {}
        virtual ~RoundedRect2DObjectSystem() {}

        virtual std::string getSystemName() const override { return "Rounded Rect 2D System"; }

        virtual void init() override;
        virtual void execute() override;

        RenderCall createRenderCall(CompRef<PositionComponent> ui, CompRef<RoundedRect2DObject> obj);

        virtual void onEvent(const EntityChangedEvent& event) override;

        uint64_t materialId = 0;
        std::queue<_unique_id> updateQueue;
    };

    template <typename Type>
    CompList<PositionComponent, RoundedRect2DObject> makeRoundedRect2DShape(Type* ecs, float cornerRadius, float width = 0.0f, float height = 0.0f, const constant::Vector4D& colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);
        ui->setWidth(width);
        ui->setHeight(height);

        ecs->template attach<UiAnchor>(entity);


        auto obj = ecs->template attach<RoundedRect2DObject>(entity, cornerRadius, colors);

        return {entity, ui, obj};
    }
}