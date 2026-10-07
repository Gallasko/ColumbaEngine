#include "stdafx.h"

#include "HatchRect2DObject.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

bool attachHatchRect2DObject(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};
    float spacing = 6.0f;
    float lineWidth = 2.0f;
    float angle = 45.0f;
    float cornerRadius = 0.0f;

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

        if (key == "spacing")
            spacing = detail::extractFloatArg(args, i + 1);
        else if (key == "lineWidth")
            lineWidth = detail::extractFloatArg(args, i + 1);
        else if (key == "angle")
            angle = detail::extractFloatArg(args, i + 1);
        else if (key == "cornerRadius")
            cornerRadius = detail::extractFloatArg(args, i + 1);
    }

    // Attach HatchRect2DObject with parsed parameters
    auto comp = ecs->_attach<HatchRect2DObject>(entity);

    comp->setColors(colors);
    comp->setSpacing(spacing);
    comp->setLineWidth(lineWidth);
    comp->setAngle(angle);
    comp->setCornerRadius(cornerRadius);

    LOG_INFO("ECS Serialization", "Attached HatchRect2DObject to entity " << entity->id);

    return true;
}

// Register the HatchRect2DObject component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(HatchRect2DObject, attachHatchRect2DObject);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct HatchRect2DObjectProxyMetadataRegistrar
{
    HatchRect2DObjectProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "HatchRect2DObject";

        metadata.retriever = [](EntitySystem* ecs, _unique_id entityId) -> void* {
            return ecs->getComponent<HatchRect2DObject>(entityId);
        };

        // Property: colors
        metadata.addProperty(PropertyMetadata{
            "colors",
            pg::PropertyType::Vector4D,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<HatchRect2DObject*>(comp);
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
                auto* c = static_cast<HatchRect2DObject*>(comp);
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
                auto* c = static_cast<HatchRect2DObject*>(comp);
                auto vec = c->getColors();
                return std::to_string(vec.x) + "," + std::to_string(vec.y) + "," + std::to_string(vec.z) + "," + std::to_string(vec.w);
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
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

        // Property: spacing
        metadata.addProperty(PropertyMetadata{
            "spacing",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getSpacing());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setSpacing(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                return std::to_string(c->getSpacing());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                c->setSpacing(std::stof(val));
            }
        });

        // Property: lineWidth
        metadata.addProperty(PropertyMetadata{
            "lineWidth",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getLineWidth());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setLineWidth(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                return std::to_string(c->getLineWidth());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                c->setLineWidth(std::stof(val));
            }
        });

        // Property: angle
        metadata.addProperty(PropertyMetadata{
            "angle",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getAngle());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setAngle(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                return std::to_string(c->getAngle());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                c->setAngle(std::stof(val));
            }
        });

        // Property: cornerRadius
        metadata.addProperty(PropertyMetadata{
            "cornerRadius",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getCornerRadius());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setCornerRadius(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                return std::to_string(c->getCornerRadius());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<HatchRect2DObject*>(comp);
                c->setCornerRadius(std::stof(val));
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for HatchRect2DObject");
    }
};

static HatchRect2DObjectProxyMetadataRegistrar s_hatchRect2DObjectProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_HatchRect2DObject_registration() {
    // Being called is enough to force linking
}
