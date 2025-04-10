#define STB_IMAGE_IMPLEMENTATION

#include "simple2dobject.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "logger.h"

#include "Helpers/openglobject.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Shape 2D";
    }

    /**
     * @brief Specialization of the serialize function for Shape2D
     *
     * @param archive A references to the archive
     * @param value The shape 2d value
     */
    template <>
    void serialize(Archive& archive, const Shape2D& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization("Shape2D");

        std::string shapeString;

        switch (value)
        {
            case Shape2D::Triangle:
                shapeString = "Triangle"; break;
            case Shape2D::Square:
                shapeString = "Square"; break;
            case Shape2D::Circle:
                shapeString = "Circle"; break;
            case Shape2D::None:
                shapeString = "None"; break;
            default:
                LOG_ERROR(DOM, "Invalid anchor dir value");
                shapeString = "None";
                break;
        };

        serialize(archive, "shape", shapeString);

        archive.endSerialization();
    }

    /**
     * @brief Specialization of the serialize function for Simple2DObject
     *
     * @param archive A references to the archive
     * @param value The simple 2d object value
     */
    template <>
    void serialize(Archive& archive, const Simple2DObject& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(Simple2DObject::getType());

        serialize(archive, "shape", value.shape);
        serialize(archive, "colors", value.colors);

        archive.endSerialization();
    }

    template <>
    Shape2D deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an Shape2D");

            auto shapeValue = deserialize<std::string>(serializedString["shape"]);

            if (shapeValue == "Triangle")
                return Shape2D::Triangle;
            else if (shapeValue == "Square")
                return Shape2D::Square;
            else if (shapeValue == "Circle")
                return Shape2D::Circle;
            else if (shapeValue == "None")
                return Shape2D::None;

            return Shape2D::None;
        }

        return Shape2D::None;
    }

    template <>
    Simple2DObject deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an Simple2DObject");

            auto shape = deserialize<Shape2D>(serializedString["shape"]);
            auto colors = deserialize<constant::Vector4D>(serializedString["colors"]);

            return Simple2DObject{shape, colors};
        }

        return Simple2DObject{Shape2D::None};
    }

    void Simple2DObjectSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        Material simpleShapeMaterial;

        simpleShapeMaterial.shader = masterRenderer->getShader("2DShapes");

        simpleShapeMaterial.nbTextures = 0;

        simpleShapeMaterial.uniformMap.emplace("sWidth", "ScreenWidth");
        simpleShapeMaterial.uniformMap.emplace("sHeight", "ScreenHeight");

        simpleShapeMaterial.setSimpleMesh({3, 2, 4});

        materialId = masterRenderer->registerMaterial(simpleShapeMaterial);

        auto group = registerGroup<PositionComponent, Simple2DObject>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_INFO("Simple 2D Object System", "Add entity " << entity->id << " to ui - 2d shape group !");

            shapeUpdateQueue.push(entity->id);

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_INFO("Simple 2D Object System", "Remove entity " << id << " of ui - 2d shape group !");

            auto entity = ecsRef->getEntity(id);

            ecsRef->detach<Simple2DRenderCall>(entity);

            changed = true;
        });
    }

    void Simple2DObjectSystem::execute()
    {
        if (not changed)
            return;

        while (not shapeUpdateQueue.empty())
        {
            auto entityId = shapeUpdateQueue.front();

            auto entity = ecsRef->getEntity(entityId);

            if (not entity)
            {
                shapeUpdateQueue.pop();
                continue;
            }

            auto ui = entity->get<PositionComponent>();
            auto obj = entity->get<Simple2DObject>();

            if (entity->has<Simple2DRenderCall>())
            {
                entity->get<Simple2DRenderCall>()->call = createRenderCall(ui, obj);
            }
            else
            {
                ecsRef->attach<Simple2DRenderCall>(entity, createRenderCall(ui, obj));
            }

            shapeUpdateQueue.pop();
        }

        renderCallList.clear();

        const auto& renderCallView = view<Simple2DRenderCall>();

        renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            renderCallList.push_back(renderCall->call);
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

        call.data.resize(9);

        call.data[0] = ui->x;
        call.data[1] = ui->y;
        call.data[2] = ui->z;
        call.data[3] = ui->width;
        call.data[4] = ui->height;
        call.data[5] = obj->colors.x;
        call.data[6] = obj->colors.y;
        call.data[7] = obj->colors.z;
        call.data[8] = obj->colors.w;

        return call;
    }

    void Simple2DObjectSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(event.id);

        if (not entity or not entity->has<Simple2DObject>())
            return;

        shapeUpdateQueue.push(event.id);

        changed = true;
    }
}
