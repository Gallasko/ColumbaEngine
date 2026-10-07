#include "stdafx.h"

#include "ThemeComponent.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

bool attachThemeComponent(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    std::string element = "";

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

    }

    // Attach ThemeComponent with parsed parameters
    auto comp = ecs->_attach<ThemeComponent>(entity);

    comp->setElement(element);

    LOG_INFO("ECS Serialization", "Attached ThemeComponent to entity " << entity->id);

    return true;
}

// Register the Theme component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(Theme, attachThemeComponent);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct ThemeComponentProxyMetadataRegistrar
{
    ThemeComponentProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "ThemeComponent";

        metadata.retriever = [](EntitySystem* ecs, _unique_id entityId) -> void* {
            return ecs->getComponent<ThemeComponent>(entityId);
        };

        // Property: element
        metadata.addProperty(PropertyMetadata{
            "element",
            pg::PropertyType::String,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<ThemeComponent*>(comp);
                return vm->createString(c->getElement());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<ThemeComponent*>(comp);
                c->setElement(vm->asString(val));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<ThemeComponent*>(comp);
                return c->getElement();
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<ThemeComponent*>(comp);
                c->setElement(val);
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for ThemeComponent");
    }
};

static ThemeComponentProxyMetadataRegistrar s_themeComponentProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_ThemeComponent_registration() {
    // Being called is enough to force linking
}
