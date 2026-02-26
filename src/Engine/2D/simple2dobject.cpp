#include "stdafx.h"

#define STB_IMAGE_IMPLEMENTATION

#include "simple2dobject.h"

#include "glm/gtc/matrix_transform.hpp"

#include "logger.h"

#include "Helpers/openglobject.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Shape 2D";
    }

    void Simple2DObjectSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        Material simpleShapeMaterial;

        simpleShapeMaterial.shader = masterRenderer->getShader("2DShapes");

        simpleShapeMaterial.nbTextures = 0;

        simpleShapeMaterial.uniformMap.emplace("sWidth", "ScreenWidth");
        simpleShapeMaterial.uniformMap.emplace("sHeight", "ScreenHeight");

        simpleShapeMaterial.setSimpleMesh({3, 2, 1, 4});

        materialId = masterRenderer->registerMaterial(simpleShapeMaterial);

        auto group = registerGroup<PositionComponent, Simple2DObject>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE("Simple 2D Object System", "Add entity " << entity->id << " to ui - 2d shape group !");

            auto ui = entity->get<PositionComponent>();
            auto obj = entity->get<Simple2DObject>();

            entityRenderCalls[entity->id] = createRenderCall(ui, obj);
            entitiesInRenderGroup.push_back(entity->id);
            std::sort(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end());

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* /*ecsRef*/, _unique_id id) {
            LOG_MILE("Simple 2D Object System", "Remove entity " << id << " of ui - 2d shape group !");

            entityRenderCalls.erase(id);
            entitiesInRenderGroup.erase(std::remove(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), id), entitiesInRenderGroup.end());

            changed = true;
        });
    }

    void Simple2DObjectSystem::execute()
    {
        if (not changed)
            return;

        std::vector<_unique_id> updateQueue;
        std::vector<_unique_id> temp;

        temp.assign(shapeUpdateSet.begin(), shapeUpdateSet.end());
        std::sort(temp.begin(), temp.end());

        std::set_intersection(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(),
                              temp.begin(), temp.end(),
                              std::back_inserter(updateQueue));

        shapeUpdateSet.clear();

        for (const auto& entityId : updateQueue)
        {
            auto entity = ecsRef->getEntity(entityId);

            if (not entity)
                continue;

            auto ui = entity->get<PositionComponent>();
            auto obj = entity->get<Simple2DObject>();

            entityRenderCalls[entityId] = createRenderCall(ui, obj);
        }

        renderCallList.clear();

        renderCallList.reserve(entityRenderCalls.size());

        for (const auto& [id, renderCall] : entityRenderCalls)
        {
            renderCallList.push_back(renderCall);
        }

        finishChanges();
    }

    RenderCall Simple2DObjectSystem::createRenderCall(CompRef<PositionComponent> ui, CompRef<Simple2DObject> obj)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        // Todo
        // if (obj->colors.w == 255.0f)
        //     call.setOpacity(OpacityType::Opaque);
        // else
        //     call.setOpacity(OpacityType::Additive);

        // Cannot set to opaque because the z testing get messed up if you don't order it properly with other systems
        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        call.setMaterial(materialId);

        call.setViewport(obj->viewport);

        call.data.resize(10);

        call.data[0] = ui->x;
        call.data[1] = ui->y;
        call.data[2] = ui->z;
        call.data[3] = ui->width;
        call.data[4] = ui->height;
        call.data[5] = ui->rotation;
        call.data[6] = obj->colors.x;
        call.data[7] = obj->colors.y;
        call.data[8] = obj->colors.z;
        call.data[9] = obj->colors.w;

        return call;
    }

    void Simple2DObjectSystem::onEvent(const PositionComponentChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void Simple2DObjectSystem::onEvent(const Simple2DObjectChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void Simple2DObjectSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        shapeUpdateSet.insert(entityId);

        changed = true;
    }

    // ---------------------------------------------------------------------------
    // RoundedRect2DObject serialize / deserialize
    // ---------------------------------------------------------------------------

    template <>
    void serialize(Archive& archive, const RoundedRect2DObject& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(RoundedRect2DObject::getType());

        serialize(archive, "cornerRadius", value.cornerRadius);
        serialize(archive, "colors", value.colors);

        archive.endSerialization();
    }

    template <>
    RoundedRect2DObject deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
            return RoundedRect2DObject{};
        }

        LOG_INFO(DOM, "Deserializing a RoundedRect2DObject");

        auto cornerRadius = deserialize<float>(serializedString["cornerRadius"]);
        auto colors = deserialize<constant::Vector4D>(serializedString["colors"]);

        return RoundedRect2DObject{cornerRadius, colors};
    }

    // ---------------------------------------------------------------------------
    // RoundedRect2DObjectSystem
    // ---------------------------------------------------------------------------

    void RoundedRect2DObjectSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        Material mat;

        mat.shader = masterRenderer->getShader("RoundedRect");

        mat.nbTextures = 0;

        mat.uniformMap.emplace("sWidth", "ScreenWidth");
        mat.uniformMap.emplace("sHeight", "ScreenHeight");

        // Instance layout: worldPos(3), size(2), rotation(1), color(4), cornerRadius(1) = 11 floats
        mat.setSimpleMesh({3, 2, 1, 4, 1});

        materialId = masterRenderer->registerMaterial(mat);

        auto group = registerGroup<PositionComponent, RoundedRect2DObject>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE("Rounded Rect 2D System", "Add entity " << entity->id << " to rounded rect group!");
            updateQueue.push(entity->id);
            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE("Rounded Rect 2D System", "Remove entity " << id << " from rounded rect group!");
            auto entity = ecsRef->getEntity(id);
            ecsRef->detach<RoundedRect2DRenderCall>(entity);
            changed = true;
        });
    }

    void RoundedRect2DObjectSystem::execute()
    {
        if (not changed)
            return;

        while (not updateQueue.empty())
        {
            auto entityId = updateQueue.front();

            auto entity = ecsRef->getEntity(entityId);

            if (not entity)
            {
                updateQueue.pop();
                continue;
            }

            auto ui  = entity->get<PositionComponent>();
            auto obj = entity->get<RoundedRect2DObject>();

            if (entity->has<RoundedRect2DRenderCall>())
            {
                entity->get<RoundedRect2DRenderCall>()->call = createRenderCall(ui, obj);
            }
            else
            {
                ecsRef->_attach<RoundedRect2DRenderCall>(entity, createRenderCall(ui, obj));
            }

            updateQueue.pop();
        }

        renderCallList.clear();

        const auto& renderCallView = view<RoundedRect2DRenderCall>();

        renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            renderCallList.push_back(renderCall->call);
        }

        finishChanges();
    }

    RenderCall RoundedRect2DObjectSystem::createRenderCall(CompRef<PositionComponent> ui, CompRef<RoundedRect2DObject> obj)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        call.setMaterial(materialId);

        call.setViewport(obj->viewport);

        // 11 floats: x, y, z, width, height, rotation, r, g, b, a, cornerRadius
        call.data.resize(11);

        call.data[0]  = ui->x;
        call.data[1]  = ui->y;
        call.data[2]  = ui->z;
        call.data[3]  = ui->width;
        call.data[4]  = ui->height;
        call.data[5]  = ui->rotation;
        call.data[6]  = obj->colors.x;
        call.data[7]  = obj->colors.y;
        call.data[8]  = obj->colors.z;
        call.data[9]  = obj->colors.w;
        call.data[10] = obj->cornerRadius;

        return call;
    }

    void RoundedRect2DObjectSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(event.id);

        if (not entity or not entity->has<RoundedRect2DObject>())
            return;

        updateQueue.push(event.id);

        changed = true;
    }
}
