#include "stdafx.h"

#include "Simple2DObject.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

bool attachSimple2DObject(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    Shape2D shape = Shape2D::Triangle;
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};

    // Process key-value pairs
    for (int i = 0; i < argCount; i += 2)
    {
        if (i + 1 >= argCount)
            break;

        if (not IS_STRING(args[i]))
        {
            LOG_ERROR("ECS Serialization", "attachComp expects string keys for properties");
            continue;
        }

        auto key = vm->asString(args[i]);

        if (key == "shape")
        {
            if (IS_STRING(args[i + 1]))
            {
                auto shapeEnumIt = stringToShape2D.find(vm->asString(args[i + 1]));
                if (shapeEnumIt != stringToShape2D.end())
                    shape = shapeEnumIt->second;
            }
        }
    }

    // Attach Simple2DObject with parsed parameters
    auto comp = ecs->_attach<Simple2DObject>(entity);

    comp->setShape(shape);
    comp->setColors(colors);

    LOG_INFO("ECS Serialization", "Attached Simple2DObject to entity " << entity->id);

    return true;
}

// Register the Simple2DObject component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(Simple2DObject, attachSimple2DObject);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct Simple2DObjectProxyMetadataRegistrar
{
    Simple2DObjectProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "Simple2DObject";

        metadata.retriever = [](EntitySystem* ecs, _unique_id entityId) -> void* {
            return ecs->getComponent<Simple2DObject>(entityId);
        };

        // Property: shape
        metadata.addProperty(PropertyMetadata{
            "shape",
            pg::PropertyType::String,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<Simple2DObject*>(comp);
                (void)vm;
                return vm->createString(shape2DToString.at(c->getShape()));
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<Simple2DObject*>(comp);
                auto enumIt = stringToShape2D.find(vm->asString(val));
                if (enumIt != stringToShape2D.end())
                    c->setShape(enumIt->second);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<Simple2DObject*>(comp);
                return shape2DToString.at(c->getShape());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<Simple2DObject*>(comp);
                auto enumIt = stringToShape2D.find(val);
                if (enumIt != stringToShape2D.end())
                    c->setShape(enumIt->second);
            }
        });

        // Property: colors
        metadata.addProperty(PropertyMetadata{
            "colors",
            pg::PropertyType::Vector4D,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<Simple2DObject*>(comp);
                // Convert Vector4D to table
                Value tableValue = vm->createTable();
                ObjInstance* table = vm->asInstance(tableValue);

                auto vec = c->getColors();

                table->setField("x", makeFloatValue(vec.x), vm, true);
                table->setField("y", makeFloatValue(vec.y), vm, true);
                table->setField("z", makeFloatValue(vec.z), vm, true);
                table->setField("w", makeFloatValue(vec.w), vm, true);

                return tableValue;
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<Simple2DObject*>(comp);
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
                auto* c = static_cast<Simple2DObject*>(comp);
                auto vec = c->getColors();
                return std::to_string(vec.x) + "," + std::to_string(vec.y) + "," + std::to_string(vec.z) + "," + std::to_string(vec.w);
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<Simple2DObject*>(comp);
                // Parse comma-separated "x,y,z,w"
                float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

                std::istringstream ss(val);
                std::string tok;
                if (std::getline(ss, tok, ','))
                    x = std::stof(tok);
                if (std::getline(ss, tok, ','))
                    y = std::stof(tok);
                if (std::getline(ss, tok, ','))
                    z = std::stof(tok);
                if (std::getline(ss, tok, ','))
                    w = std::stof(tok);

                c->setColors(constant::Vector4D(x, y, z, w));
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for Simple2DObject");
    }
};

static Simple2DObjectProxyMetadataRegistrar s_simple2DObjectProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_Simple2DObject_registration() {
    // Being called is enough to force linking
}
