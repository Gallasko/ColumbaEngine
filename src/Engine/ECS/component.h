#pragma once

#include "entityref.h"
#include "standardevent.h"

/**
 * Macro that add all default members to a component
 *
 * @param TypeName Name of the type to add the members to
 */
#define DEFAULT_COMPONENT_MEMBERS(TypeName)             \
    TypeName()                               = default; \
    TypeName(const TypeName&)                = default; \
    TypeName(TypeName&&) noexcept            = default; \
    TypeName& operator=(const TypeName&)     = default; \
    TypeName& operator=(TypeName&&) noexcept = default; \
    ~TypeName() override                     = default;


namespace pg
{
    class EntitySystem;

    /**
     * @brief Structure tag used to specify onCreation member on a component
     *
     * @todo find a better class name
     */
    struct Ctor
    {
        virtual void onCreation(EntityRef entity) = 0;

        virtual ~Ctor() {}
    };

    /**
     * @brief Structure tag used to specify onCreation member on a component
     *
     */
    struct Component : public Ctor
    {
        Component() = default;
        Component(const Component& other) : ecsRef(other.ecsRef), entityId(other.entityId) {}

        Component& operator=(const Component& other)
        {
            ecsRef = other.ecsRef;
            entityId = other.entityId;

            return *this;
        }

        virtual ~Component() {}

        // Todo Is EntityRef copy here really necessary (for the ref id recomputting) or can we just pass it by ref
        virtual void onCreation(EntityRef entity) override;

        EntitySystem* ecsRef = nullptr;
        _unique_id entityId = 0;

        template <typename Type>
        void setValue(Type& currentValue, const Type& value);
    };

    /**
     * @brief Structure tag used to specify onCreation member on a component
     *
     * @todo find a better class name
     */
    struct Dtor
    {
        virtual void onDeletion(EntityRef entity) = 0;

        virtual ~Dtor() {}
    };

    /**
     * @brief Structure tag used to specify onCreation member on a component
     *
     * @todo find a better class name
     */
    struct Copy
    {
        virtual void onCopy(EntityRef entity) = 0;

        virtual ~Copy() {}
    };

    /**
     * @brief Structure tag used to specify a component as a singleton component
     *
     * When this tag is set for a component. That means that the component should only be created once
     * and when a system or a component need this component we can use the same refererence everywhere.
     *
     * So when a component with this tag is created we can directly save the ref in the ecs and used
     * it throughout !
     *
     * @todo make this
     */
    struct SingletonComp
    {

    };

    /**
     * @brief Standard Component - a named component with dynamic properties
     *
     * This component type uses a string-based type name and a map of properties
     * instead of C++ types, allowing for dynamic component creation at runtime.
     */
    struct StandardComponent : public Component
    {
        StandardComponent(const std::string& typeName) : typeName(typeName) {}

        StandardComponent(const std::string& typeName, const StandardComponent& other) : StandardComponent(other)
        {
            this->typeName = typeName;
        }

        DEFAULT_COMPONENT_MEMBERS(StandardComponent)

        std::string typeName;
        ElementMap properties;

        // Helper to get/set properties
        template<typename T>
        void set(const std::string& key, const T& value)
        {
            properties[key] = ElementType{value};
        }

        // Set property and send a change event
        // Sends a StandardEvent with name "Changed<ComponentTypeName>"
        // Event contains: "propertyName" -> name of the changed property
        //                 "oldValue" -> previous value (if it existed)
        //                 "newValue" -> new value
        //                 "entityId" -> ID of the entity owning this component
        template<typename T>
        void setWithEvent(const std::string& key, const T& value)
        {
            // Check if property exists and save old value
            bool hadOldValue = has(key);
            ElementType oldValue;
            if (hadOldValue)
            {
                oldValue = properties[key];
            }

            // Set the new value
            ElementType newValue{value};
            properties[key] = newValue;

            // Send change event if we have access to the ECS
            if (ecsRef)
            {
                // Create event name: "Changed" + typeName (e.g., "ChangedPosition", "ChangedHealth")
                std::string eventName = "Changed" + typeName;

                // Build the event with all relevant information
                StandardEvent changeEvent(eventName);
                changeEvent.values["propertyName"] = ElementType{key};
                changeEvent.values["newValue"] = newValue;
                changeEvent.values["entityId"] = ElementType{entityId};

                if (hadOldValue)
                {
                    changeEvent.values["oldValue"] = oldValue;
                }

                sendStandardEvent(changeEvent);
            }
        }

        void sendStandardEvent(const StandardEvent& event);

        template<typename T>
        T get(const std::string& key) const
        {
            auto it = properties.find(key);
            if (it != properties.end())
                return it->second.get<T>();
            return T{};
        }

        bool has(const std::string& key) const
        {
            return properties.find(key) != properties.end();
        }

        // Get type name for this component
        static std::string getType() { return "StandardComponent"; }
    };
}