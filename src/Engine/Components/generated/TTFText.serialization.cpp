#include "stdafx.h"

#include "TTFText.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

void serializeTTFTextWithSetters(VM* vm, ObjInstance* table, TTFText* component)
{
    // Get component context
    _unique_id entityId = component->entityId;

    LOG_MILE("ECS Serialization", "Generating setters for TTFText on entity " << entityId);

    // Generate setter methods for each property using macros
    REGISTER_STRING_SETTER(vm, table, component, setText);
    REGISTER_STRING_SETTER(vm, table, component, setFontPath);
    REGISTER_FLOAT_SETTER(vm, table, component, setScale);
    auto colorsCustomSetter = [component](VM* vm, int argCount, Value* args) -> Value {
        // Special setter with default alpha
        if (argCount < 3)
            throw std::runtime_error("setColor expects at least 3 arguments: r, g, b, [a]");

        constant::Vector4D colors;
        for (int i = 0; i < 3; i++)
        {
            colors[i] = detail::extractFloatArg(args, i);
        }

        // Alpha (optional, default 255)
        colors[3] = 255.0f;
        if (argCount >= 4)
        {
            colors[3] = detail::extractFloatArg(args, 3);
        }

        component->setColors(colors);

        return INT_VAL(0);
    };
    table->setField("setColors", vm->createNativeFunction(colorsCustomSetter));
    REGISTER_BOOL_SETTER(vm, table, component, setWrap);
    REGISTER_FLOAT_SETTER(vm, table, component, setSpacing);
}

// Register TTFText serializer at static initialization time
REGISTER_COMPONENT_SERIALIZER(TTFText, serializeTTFTextWithSetters);

bool attachTTFText(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    std::string text = "";
    float textWidth = 0.0f;
    float textHeight = 0.0f;
    std::string fontPath = "";
    float scale = 1.0f;
    constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};
    bool wrap = false;
    float spacing = 0.0f;
    bool changed = false;

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

        if (key == "textWidth")
            textWidth = detail::extractFloatArg(args, i + 1);
        else if (key == "textHeight")
            textHeight = detail::extractFloatArg(args, i + 1);
        else if (key == "scale")
            scale = detail::extractFloatArg(args, i + 1);
        else if (key == "wrap")
            wrap = detail::extractBoolArg(args, i + 1);
        else if (key == "spacing")
            spacing = detail::extractFloatArg(args, i + 1);
        else if (key == "changed")
            changed = detail::extractBoolArg(args, i + 1);
    }

    // Attach TTFText with parsed parameters
    auto comp = ecs->_attach<TTFText>(entity);

    comp->setText(text);
    comp->textWidth = textWidth;
    comp->textHeight = textHeight;
    comp->setFontPath(fontPath);
    comp->setScale(scale);
    comp->setColors(colors);
    comp->setWrap(wrap);
    comp->setSpacing(spacing);
    comp->changed = changed;

    LOG_INFO("ECS Serialization", "Attached TTFText to entity " << entity->id);

    return true;
}

// Register the TTFText component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(TTFText, attachTTFText);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct TTFTextProxyMetadataRegistrar
{
    TTFTextProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "TTFText";
        metadata.componentSize = sizeof(TTFText);

        // Property: text
        metadata.properties.emplace("text", PropertyMetadata{
            "text",
            pg::PropertyType::String,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<TTFText*>(comp);
                return vm->createString(c->getText());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<TTFText*>(comp);
                c->setText(vm->asString(val));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<TTFText*>(comp);
                return c->getText();
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<TTFText*>(comp);
                c->setText(val);
            }
        });

        // Property: fontPath
        metadata.properties.emplace("fontPath", PropertyMetadata{
            "fontPath",
            pg::PropertyType::String,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<TTFText*>(comp);
                return vm->createString(c->getFontPath());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<TTFText*>(comp);
                c->setFontPath(vm->asString(val));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<TTFText*>(comp);
                return c->getFontPath();
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<TTFText*>(comp);
                c->setFontPath(val);
            }
        });

        // Property: scale
        metadata.properties.emplace("scale", PropertyMetadata{
            "scale",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<TTFText*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getScale());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<TTFText*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setScale(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<TTFText*>(comp);
                return std::to_string(c->getScale());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<TTFText*>(comp);
                c->setScale(std::stof(val));
            }
        });

        // Property: colors
        metadata.properties.emplace("colors", PropertyMetadata{
            "colors",
            pg::PropertyType::Vector4D,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<TTFText*>(comp);
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
                auto* c = static_cast<TTFText*>(comp);
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
                auto* c = static_cast<TTFText*>(comp);
                auto vec = c->getColors();
                return std::to_string(vec.x) + "," + std::to_string(vec.y) + "," + std::to_string(vec.z); + "," + std::to_string(vec.w);
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<TTFText*>(comp);
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

        // Property: wrap
        metadata.properties.emplace("wrap", PropertyMetadata{
            "wrap",
            pg::PropertyType::Bool,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<TTFText*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeBoolValue(c->getWrap());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<TTFText*>(comp);
                (void)vm; // Suppress unused parameter warning
                c->setWrap(AS_BOOL(val));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<TTFText*>(comp);
                return std::to_string(c->getWrap());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<TTFText*>(comp);
                c->setWrap(val == "true" or val == "1");
            }
        });

        // Property: spacing
        metadata.properties.emplace("spacing", PropertyMetadata{
            "spacing",
            pg::PropertyType::Float,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<TTFText*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeFloatValue(c->getSpacing());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<TTFText*>(comp);
                (void)vm; // Suppress unused parameter warning
                float v = IS_DOUBLE(val) ? static_cast<float>(AS_DOUBLE(val)) : static_cast<float>(AS_INT(val));
                c->setSpacing(v);
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<TTFText*>(comp);
                return std::to_string(c->getSpacing());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<TTFText*>(comp);
                c->setSpacing(std::stof(val));
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for TTFText");
    }
};

static TTFTextProxyMetadataRegistrar s_tTFTextProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_TTFText_registration() {
    // Being called is enough to force linking
}
