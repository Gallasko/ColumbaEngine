#pragma once

#include "logger.h"

#include "position.h"

#include "Renderer/renderer.h"

#include "constant.h"

// Todo this system can be faulty during scene changing !

namespace pg
{
    enum class Shape2D : uint8_t
    {
        Triangle = 0,
        Square,
        Circle,
        None
    };

    struct Simple2DObject : public Ctor
    {
        Simple2DObject(const Shape2D& shape) : shape(shape) { }
        Simple2DObject(const Shape2D& shape, const constant::Vector4D& c) : shape(shape), colors(c) { }

        Simple2DObject(const Simple2DObject &rhs) : shape(rhs.shape), colors(rhs.colors), id(rhs.id), ecsRef(rhs.ecsRef) { }
        virtual ~Simple2DObject() {}

        inline static std::string getType() { return "Simple2DObject"; }

        virtual void onCreation(EntityRef entity) { id = entity.id; ecsRef = entity.ecsRef; }

        void setColors(const constant::Vector4D& colors)
        {
            this->colors = colors;

            if (ecsRef)
            {
                ecsRef->sendEvent(EntityChangedEvent{id});
            }
        }

        void setOpacity(float alpha)
        {
            colors.w = alpha;

            if (ecsRef)
            {
                ecsRef->sendEvent(EntityChangedEvent{id});
            }
        }

        void setViewport(size_t viewport)
        {
            if (this->viewport != viewport)
            {
                this->viewport = viewport;

                if (ecsRef)
                {
                    ecsRef->sendEvent(EntityChangedEvent{id});
                }
            }
        }

        Shape2D shape;

        // Todo be specific for each shape (rect = [width, height], circle = [origin, radius], triangle = [base, height] + rotation arg)
        // constant::Vector2D size {10.0f, 10.0f};

        // Todo make the colors normalized (0.0f <-> 1.0f) to not do the division in the shader
        constant::Vector4D colors {255.0f, 255.0f, 255.0f, 255.0f};

        size_t viewport = 0;

        _unique_id id;

        EntitySystem *ecsRef = nullptr;
    };

    template <>
    void serialize(Archive& archive, const Shape2D& value);

    template <>
    void serialize(Archive& archive, const Simple2DObject& value);

    template <>
    Shape2D deserialize(const UnserializedObject& serializedString);

    template <>
    Simple2DObject deserialize(const UnserializedObject& serializedString);

    struct Simple2DRenderCall
    {
        Simple2DRenderCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    struct Simple2DObjectSystem : public AbstractRenderer, System<Own<Simple2DObject>, Own<Simple2DRenderCall>, Listener<EntityChangedEvent>, InitSys>
    {
        Simple2DObjectSystem(MasterRenderer* masterRenderer) : AbstractRenderer(masterRenderer, RenderStage::Render) { }
        virtual ~Simple2DObjectSystem() { }

        virtual std::string getSystemName() const override { return "Shape 2D System"; }

        virtual void init() override;

        virtual void execute() override;

        RenderCall createRenderCall(CompRef<PositionComponent> ui, CompRef<Simple2DObject> obj);

        virtual void onEvent(const EntityChangedEvent& event) override;

        uint64_t materialId = 0;

        std::queue<_unique_id> shapeUpdateQueue;
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
}