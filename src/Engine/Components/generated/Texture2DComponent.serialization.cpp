#include "stdafx.h"

#include "Texture2DComponent.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

void serializeTexture2DComponentWithSetters(VM* vm, ObjInstance* table, Texture2DComponent* component)
{
    // Get component context
    _unique_id entityId = component->entityId;

    LOG_MILE("ECS Serialization", "Generating setters for Texture2DComponent on entity " << entityId);

    // Generate setter methods for each property using macros
    REGISTER_STRING_SETTER(vm, table, component, setTextureName);
    REGISTER_FLOAT_SETTER(vm, table, component, setOpacity);
    auto overlappingColorCustomSetter = [component](VM* vm, int argCount, Value* args) -> Value {
        // Add special setter for overlappingColor (takes 4 args: r, g, b, ratio)
        // Create native function directly without polluting globals
        if (argCount != 4) return INT_VAL(0); // Expecting r, g, b, ratio

        float r = 0.0f, g = 0.0f, b = 0.0f, ratio = 0.0f;

        if (IS_DOUBLE(args[0])) r = static_cast<float>(AS_DOUBLE(args[0]));
        else if (IS_INT(args[0])) r = static_cast<float>(AS_INT(args[0]));

        if (IS_DOUBLE(args[1])) g = static_cast<float>(AS_DOUBLE(args[1]));
        else if (IS_INT(args[1])) g = static_cast<float>(AS_INT(args[1]));

        if (IS_DOUBLE(args[2])) b = static_cast<float>(AS_DOUBLE(args[2]));
        else if (IS_INT(args[2])) b = static_cast<float>(AS_INT(args[2]));

        if (IS_DOUBLE(args[3])) ratio = static_cast<float>(AS_DOUBLE(args[3]));
        else if (IS_INT(args[3])) ratio = static_cast<float>(AS_INT(args[3]));

        component->setOverlappingColor(constant::Vector3D{r, g, b}, ratio);
        return INT_VAL(0);
    };
    table->setField("setOverlappingColor", vm->createNativeFunction(overlappingColorCustomSetter));
    REGISTER_FLOAT_SETTER(vm, table, component, setOverlappingColorRatio);
}

// Register Texture2DComponent serializer at static initialization time
REGISTER_COMPONENT_SERIALIZER(Texture2DComponent, serializeTexture2DComponentWithSetters);

bool attachTexture2DComponent(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    std::string textureName = "";
    float opacity = 1.0f;
    constant::Vector3D overlappingColor = {0.0f, 0.0f, 0.0f};
    float overlappingColorRatio = 0.0f;

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

        if (key == "opacity")
            opacity = detail::extractFloatArg(args, i + 1);
        else if (key == "overlappingColorRatio")
            overlappingColorRatio = detail::extractFloatArg(args, i + 1);
    }

    // Attach Texture2DComponent with parsed parameters
    auto comp = ecs->_attach<Texture2DComponent>(entity);

    comp->setTextureName(textureName);
    comp->setOpacity(opacity);
    comp->setOverlappingColor(overlappingColor);
    comp->setOverlappingColorRatio(overlappingColorRatio);

    LOG_INFO("ECS Serialization", "Attached Texture2DComponent to entity " << entity->id);

    return true;
}

// Register the Texture2D component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(Texture2D, attachTexture2DComponent);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct Texture2DComponentProxyMetadataRegistrar
{
    Texture2DComponentProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "Texture2DComponent";
        metadata.componentSize = sizeof(Texture2DComponent);

        // Property: textureName
        metadata.properties.emplace("textureName", PropertyMetadata{
            "textureName",
            pg::PropertyType::String,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<Texture2DComponent*>(comp);
                return vm->createString(c->getTextureName());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                c->setTextureName(vm->asString(val));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<Texture2DComponent*>(comp);
                return c->getTextureName();
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                c->setTextureName(val);
            }
        });

        // Property: opacity
        metadata.properties.emplace("opacity", PropertyMetadata{
            "opacity",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<Texture2DComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getOpacity());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setOpacity(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<Texture2DComponent*>(comp);
                return std::to_string(c->getOpacity());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                c->setOpacity(std::stof(val));
            }
        });

        // Property: overlappingColor
        metadata.properties.emplace("overlappingColor", PropertyMetadata{
            "overlappingColor",
            pg::PropertyType::Vector3D,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<Texture2DComponent*>(comp);
                // Convert Vector3D to table
                VM::GlobalCell* cell = vm->findGlobalCell("__Table");
                if (cell == nullptr or not cell->defined) return INT_VAL(0);

                Klass* tableClass = vm->asClass(cell->value);
                Value tableValue = vm->createInstance(tableClass);
                ObjInstance* table = vm->asInstance(tableValue);

                auto vec = c->getOverlappingColor();

                table->setField("x", makeFloatValue(vec.x), vm, true);
                table->setField("y", makeFloatValue(vec.y), vm, true);
                table->setField("z", makeFloatValue(vec.z), vm, true);

                return tableValue;
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                // Convert table or vector to Vector3D
                if (IS_INSTANCE(val))
                {
                    ObjInstance* table = vm->asInstance(val);

                    float x = static_cast<float>(AS_DOUBLE(table->getField("x")));
                    float y = static_cast<float>(AS_DOUBLE(table->getField("y")));
                    float z = static_cast<float>(AS_DOUBLE(table->getField("z")));

                    c->setOverlappingColor(constant::Vector3D(x, y, z));
                }
                else if (IS_VECTOR(val))
                {
                    ObjVector* vec = vm->asVector(val);

                    float x = vec->fields.size() > 0 ? static_cast<float>(AS_DOUBLE(vec->fields[0])) : 0.0f;
                    float y = vec->fields.size() > 1 ? static_cast<float>(AS_DOUBLE(vec->fields[1])) : 0.0f;
                    float z = vec->fields.size() > 2 ? static_cast<float>(AS_DOUBLE(vec->fields[2])) : 0.0f;

                    c->setOverlappingColor(constant::Vector3D(x, y, z));
                }
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<Texture2DComponent*>(comp);
                auto vec = c->getOverlappingColor();
                return std::to_string(vec.x) + "," + std::to_string(vec.y) + "," + std::to_string(vec.z);
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                // Parse comma-separated "x,y,z"
                float x = 0.0f, y = 0.0f, z = 0.0f;

                std::istringstream ss(val);
                std::string tok;
                if (std::getline(ss, tok, ',')) x = std::stof(tok);
                if (std::getline(ss, tok, ',')) y = std::stof(tok);
                if (std::getline(ss, tok, ',')) z = std::stof(tok);

                c->setOverlappingColor(constant::Vector3D(x, y, z));
            }
        });

        // Property: overlappingColorRatio
        metadata.properties.emplace("overlappingColorRatio", PropertyMetadata{
            "overlappingColorRatio",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<Texture2DComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getOverlappingColorRatio());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setOverlappingColorRatio(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<Texture2DComponent*>(comp);
                return std::to_string(c->getOverlappingColorRatio());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<Texture2DComponent*>(comp);
                c->setOverlappingColorRatio(std::stof(val));
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for Texture2DComponent");
    }
};

static Texture2DComponentProxyMetadataRegistrar s_texture2DComponentProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_Texture2DComponent_registration() {
    // Being called is enough to force linking
}
