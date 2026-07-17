#include "stdafx.h"

#include "RoundedRect2DObject.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

void serializeRoundedRect2DObjectWithSetters(VM* vm, ObjInstance* table, RoundedRect2DObject* component)
{
    // Get component context
    _unique_id entityId = component->entityId;

    LOG_MILE("ECS Serialization", "Generating setters for RoundedRect2DObject on entity " << entityId);

    // Generate setter methods for each property using macros
    REGISTER_FLOAT_SETTER(vm, table, component, setCornerRadius);
    // TODO: Add setter registration for colors (constant::Vector4D)
}

// Register RoundedRect2DObject serializer at static initialization time
REGISTER_COMPONENT_SERIALIZER(RoundedRect2DObject, serializeRoundedRect2DObjectWithSetters);

bool attachRoundedRect2DObject(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    float cornerRadius = 0.0f;
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};

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

        if (key == "cornerRadius")
            cornerRadius = detail::extractFloatArg(args, i + 1);
    }

    // Attach RoundedRect2DObject with parsed parameters
    auto comp = ecs->_attach<RoundedRect2DObject>(entity);

    comp->setCornerRadius(cornerRadius);
    comp->setColors(colors);

    LOG_INFO("ECS Serialization", "Attached RoundedRect2DObject to entity " << entity->id);

    return true;
}

// Register the RoundedRect2DObject component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(RoundedRect2DObject, attachRoundedRect2DObject);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct RoundedRect2DObjectProxyMetadataRegistrar
{
    RoundedRect2DObjectProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "RoundedRect2DObject";
        metadata.componentSize = sizeof(RoundedRect2DObject);

        // Property: cornerRadius
        metadata.properties.emplace("cornerRadius", PropertyMetadata{
            "cornerRadius",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getCornerRadius());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setCornerRadius(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                return std::to_string(c->getCornerRadius());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                c->setCornerRadius(std::stof(val));
            }
        });

        // Property: colors
        metadata.properties.emplace("colors", PropertyMetadata{
            "colors",
            pg::PropertyType::Vector4D,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                // Convert Vector4D to table
                VM::GlobalCell* cell = vm->findGlobalCell("__Table");
                if (cell == nullptr or not cell->defined) return INT_VAL(0);

                Klass* tableClass = vm->asClass(cell->value);
                Value tableValue = vm->createInstance(tableClass);
                ObjInstance* table = vm->asInstance(tableValue);

                auto vec = c->getColors();

                table->setField("x", makeFloatValue(vec.x), vm, true);
                table->setField("y", makeFloatValue(vec.y), vm, true);
                table->setField("z", makeFloatValue(vec.z), vm, true);
                table->setField("w", makeFloatValue(vec.w), vm, true);

                return tableValue;
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                // Convert table or vector to Vector4D
                if (IS_INSTANCE(val))
                {
                    ObjInstance* table = vm->asInstance(val);

                    float x = static_cast<float>(AS_DOUBLE(table->getField("x")));
                    float y = static_cast<float>(AS_DOUBLE(table->getField("y")));
                    float z = static_cast<float>(AS_DOUBLE(table->getField("z")));
                    float w = static_cast<float>(AS_DOUBLE(table->getField("w")));

                    c->setColors(constant::Vector4D(x, y, z, w));
                }
                else if (IS_VECTOR(val))
                {
                    ObjVector* vec = vm->asVector(val);

                    float x = vec->fields.size() > 0 ? static_cast<float>(AS_DOUBLE(vec->fields[0])) : 0.0f;
                    float y = vec->fields.size() > 1 ? static_cast<float>(AS_DOUBLE(vec->fields[1])) : 0.0f;
                    float z = vec->fields.size() > 2 ? static_cast<float>(AS_DOUBLE(vec->fields[2])) : 0.0f;
                    float w = vec->fields.size() > 3 ? static_cast<float>(AS_DOUBLE(vec->fields[3])) : 0.0f;

                    c->setColors(constant::Vector4D(x, y, z, w));
                }
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                auto vec = c->getColors();
                return std::to_string(vec.x) + "," + std::to_string(vec.y) + "," + std::to_string(vec.z); + "," + std::to_string(vec.w);
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<RoundedRect2DObject*>(comp);
                // Parse comma-separated "x,y,z,w"
                float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

                std::istringstream ss(val);
                std::string tok;
                if (std::getline(ss, tok, ',')) x = std::stof(tok);
                if (std::getline(ss, tok, ',')) y = std::stof(tok);
                if (std::getline(ss, tok, ',')) z = std::stof(tok);
                if (std::getline(ss, tok, ',')) w = std::stof(tok);

                c->setColors(constant::Vector4D(x, y, z, w));
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for RoundedRect2DObject");
    }
};

static RoundedRect2DObjectProxyMetadataRegistrar s_roundedRect2DObjectProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_RoundedRect2DObject_registration() {
    // Being called is enough to force linking
}
