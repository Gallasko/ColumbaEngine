#include "stdafx.h"

#include "PositionComponent.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

void serializePositionComponentWithSetters(VM* vm, ObjInstance* table, PositionComponent* component)
{
    // Get component context
    _unique_id entityId = component->entityId;

    LOG_MILE("ECS Serialization", "Generating setters for PositionComponent on entity " << entityId);

    // Generate setter methods for each property using macros
    REGISTER_FLOAT_SETTER(vm, table, component, setX);
    REGISTER_FLOAT_SETTER(vm, table, component, setY);
    REGISTER_FLOAT_SETTER(vm, table, component, setZ);
    REGISTER_FLOAT_SETTER(vm, table, component, setWidth);
    REGISTER_FLOAT_SETTER(vm, table, component, setHeight);
    REGISTER_FLOAT_SETTER(vm, table, component, setRotation);
    REGISTER_BOOL_SETTER(vm, table, component, setVisible);
    REGISTER_BOOL_SETTER(vm, table, component, setObservable);
}

// Register PositionComponent serializer at static initialization time
REGISTER_COMPONENT_SERIALIZER(PositionComponent, serializePositionComponentWithSetters);

bool attachPositionComponent(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float rotation = 0.0f;
    bool visible = true;
    bool observable = true;

    // Process key-value pairs
    for (int i = 0; i < argCount; i += 2)
    {
        if (i + 1 >= argCount) break;

        if (not IS_STRING(args[i]))
        {
            LOG_ERROR("ECS Serialization", "attachComp expects string keys for properties");
            continue;
        }

        auto key = vm->asString(args[i]);

        if (key == "x")
            x = detail::extractFloatArg(args, i + 1);
        else if (key == "y")
            y = detail::extractFloatArg(args, i + 1);
        else if (key == "z")
            z = detail::extractFloatArg(args, i + 1);
        else if (key == "width")
            width = detail::extractFloatArg(args, i + 1);
        else if (key == "height")
            height = detail::extractFloatArg(args, i + 1);
        else if (key == "rotation")
            rotation = detail::extractFloatArg(args, i + 1);
        else if (key == "visible")
            visible = detail::extractBoolArg(args, i + 1);
        else if (key == "observable")
            observable = detail::extractBoolArg(args, i + 1);
    }

    // Attach PositionComponent with parsed parameters
    auto comp = ecs->_attach<PositionComponent>(entity);

    comp->setX(x);
    comp->setY(y);
    comp->setZ(z);
    comp->setWidth(width);
    comp->setHeight(height);
    comp->setRotation(rotation);
    comp->setVisible(visible);
    comp->setObservable(observable);

    LOG_INFO("ECS Serialization", "Attached PositionComponent to entity " << entity->id);

    return true;
}

// Register the Position component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(Position, attachPositionComponent);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct PositionComponentProxyMetadataRegistrar
{
    PositionComponentProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "PositionComponent";
        metadata.componentSize = sizeof(PositionComponent);

        // Property: x
        metadata.properties.emplace("x", PropertyMetadata{
            "x",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getX());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setX(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getX());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setX(std::stof(val));
            }
        });

        // Property: y
        metadata.properties.emplace("y", PropertyMetadata{
            "y",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getY());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setY(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getY());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setY(std::stof(val));
            }
        });

        // Property: z
        metadata.properties.emplace("z", PropertyMetadata{
            "z",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getZ());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setZ(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getZ());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setZ(std::stof(val));
            }
        });

        // Property: width
        metadata.properties.emplace("width", PropertyMetadata{
            "width",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getWidth());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setWidth(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getWidth());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setWidth(std::stof(val));
            }
        });

        // Property: height
        metadata.properties.emplace("height", PropertyMetadata{
            "height",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getHeight());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setHeight(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getHeight());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setHeight(std::stof(val));
            }
        });

        // Property: rotation
        metadata.properties.emplace("rotation", PropertyMetadata{
            "rotation",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getRotation());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setRotation(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getRotation());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setRotation(std::stof(val));
            }
        });

        // Property: visible
        metadata.properties.emplace("visible", PropertyMetadata{
            "visible",
            pg::PropertyType::Bool,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeBoolValue(c->getVisible());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                c->setVisible(AS_BOOL(val));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getVisible());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setVisible(val == "true" or val == "1");
            }
        });

        // Property: observable
        metadata.properties.emplace("observable", PropertyMetadata{
            "observable",
            pg::PropertyType::Bool,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeBoolValue(c->getObservable());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<PositionComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                c->setObservable(AS_BOOL(val));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<PositionComponent*>(comp);
                return std::to_string(c->getObservable());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<PositionComponent*>(comp);
                c->setObservable(val == "true" or val == "1");
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for PositionComponent");
    }
};

static PositionComponentProxyMetadataRegistrar s_positionComponentProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_PositionComponent_registration() {
    // Being called is enough to force linking
}
