#include "stdafx.h"

#include "ViewportComponent.generated.h"

#include <sstream>

#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"

namespace pg
{

void serializeViewportComponentWithSetters(VM* vm, ObjInstance* table, ViewportComponent* component)
{
    // Get component context
    _unique_id entityId = component->entityId;

    LOG_MILE("ECS Serialization", "Generating setters for ViewportComponent on entity " << entityId);

    // Generate setter methods for each property using macros
    REGISTER_INT_SETTER(vm, table, component, setViewport);
}

// Register ViewportComponent serializer at static initialization time
REGISTER_COMPONENT_SERIALIZER(ViewportComponent, serializeViewportComponentWithSetters);

bool attachViewportComponent(VM* vm, EntitySystem* ecs, Entity* entity, int argCount, Value* args)
{
    size_t viewport = 0;

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

    }

    // Attach ViewportComponent with parsed parameters
    auto comp = ecs->_attach<ViewportComponent>(entity);

    comp->setViewport(viewport);

    LOG_INFO("ECS Serialization", "Attached ViewportComponent to entity " << entity->id);

    return true;
}

// Register the Viewport component attach handler
REGISTER_COMPONENT_ATTACH_HANDLER(Viewport, attachViewportComponent);

} // namespace pg

// ============================================================================
// Component Proxy Metadata Registration (Zero-Copy System)
// ============================================================================

namespace pg
{

struct ViewportComponentProxyMetadataRegistrar
{
    ViewportComponentProxyMetadataRegistrar()
    {
        pg::ComponentProxyMetadata metadata;
        metadata.componentTypeName = "ViewportComponent";
        metadata.componentSize = sizeof(ViewportComponent);

        // Property: viewport
        metadata.properties.emplace("viewport", PropertyMetadata{
            "viewport",
            pg::PropertyType::UnsignedInt,
            true,
            [](void* comp, VM* vm) -> Value {
                auto* c = static_cast<ViewportComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                return makeIntValue(c->getViewport());
            },
            [](void* comp, VM* vm, Value val) {
                auto* c = static_cast<ViewportComponent*>(comp);
                (void)vm; // Suppress unused parameter warning
                c->setViewport(static_cast<unsigned int>(AS_INT(val)));
            },
            [](void* comp) -> std::string {
                auto* c = static_cast<ViewportComponent*>(comp);
                return std::to_string(c->getViewport());
            },
            [](void* comp, const std::string& val) {
                auto* c = static_cast<ViewportComponent*>(comp);
                c->setViewport(static_cast<unsigned int>(std::stoul(val)));
            }
        });

        pg::ComponentProxyRegistry::instance().registerMetadata(metadata);
        LOG_INFO("ComponentProxy", "Registered proxy metadata for ViewportComponent");
    }
};

static ViewportComponentProxyMetadataRegistrar s_viewportComponentProxyMetadataRegistrar;

} // namespace pg

// Initialization function referenced from .generated.h
// This ensures this .serialization.cpp is linked and static initializers run
extern "C" void __init_ViewportComponent_registration() {
    // Being called is enough to force linking
}
