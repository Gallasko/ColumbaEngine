#pragma once

// Forward declarations for EntitySystem to reduce header dependencies
// Include this instead of entitysystem.h when you only need pointers/references

#include <cstdint>
#include <string>

namespace pg
{
    // Core ECS types
    using _unique_id = uint64_t;

    // Forward declarations
    class EntitySystem;
    class Entity;
    class EntityRef;
    class ComponentRegistry;
    struct AbstractSystem;
    class CommandDispatcher;
    class SaveManager;
    class InterpreterSystem;
    class Environment;
    class ClassInstance;
    class StandardSystemImpl;
    class StandardComponent;
    struct VM;

    // Component reference template
    template <typename Comp>
    class CompRef;

    // Events
    struct ResizeEvent { float width, height; };
}
