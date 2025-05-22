#pragma once

#include <memory>
#include <map>
#include <unordered_map>
#include <functional>
#include <any>

#include "sparseset.h"
#include "entity.h"

#include "logger.h"
#include "serialization.h"

#include "uniqueid.h"

#include "Memory/elementtype.h"

namespace pg
{
    class InputSystem;
    class ClassInstance;
    class InterpreterSystem;

    template <class T>
    class HasStaticName
    {
        template <class U, class = typename std::enable_if<!std::is_member_pointer<decltype(&U::getType)>::value>::type>
            static std::true_type check(int);
        template <class>
            static std::false_type check(...);
    public:
        static constexpr bool value = decltype(check<T>(0))::value;
    };

    template <typename Type>
    struct Own;

    template <typename Type, typename... Types>
    struct Group;

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

        virtual void onCreation(EntityRef entity) override
        {
            LOG_THIS_MEMBER("Component");

            ecsRef = entity->world();
            entityId = entity->id;
        }

        EntitySystem* ecsRef = nullptr;
        _unique_id entityId = 0;
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

    struct StandardEvent
    {
        StandardEvent(const std::string& name = "Noop") : name(name) {}

        template <typename Type>
        StandardEvent(const std::string& name, const std::string& valueName, const Type& value) : name(name)
        {
            values[valueName] = ElementType{value};
        }

        template <typename Type, typename... Args>
        StandardEvent(const std::string& name, const std::string& valueName, const Type& value, const Args&... args) : StandardEvent(name, args...)
        {
            values[valueName] = ElementType{value};
        }

        // Todo need to make a ElementType ctor to avoid a copy

        StandardEvent(const StandardEvent& other) : name(other.name), values(other.values) {}

        StandardEvent& operator=(const StandardEvent& other)
        {
            name = other.name;
            values = other.values;

            return *this;
        }

        std::string name;

        std::unordered_map<std::string, ElementType> values;
    };

    template <>
    void serialize(Archive& archive, const StandardEvent& event);

    class ComponentRegistry
    {
    public:
        ComponentRegistry(EntitySystem *ecs);

        ~ComponentRegistry();

        /**
         * @brief Store an owner in this registry
         *
         * @tparam Type Type of the owner
         * @param owner A pointer to a class that owns a component
         */
        template <typename Type>
        void store(Own<Type>* owner) noexcept;

        /**
         * @brief Unstore an owner from this registry
         *
         * @tparam Type Type of the owner
         * @param owner A pointer to a class that owns a component
         */
        template <typename Type>
        void unstore(Own<Type>* owner) noexcept;

        template <typename Type>
        Own<Type>* retrieve() const
        {
            LOG_THIS_MEMBER("Component Registry");

            const auto& id = getTypeId<Type>();
            // Todo catch errors when component doesnt exist in storage or no system own the component (so it doesnt exist in storage !)

            // Todo see if there is a performance hit to keep this function or does it get optimized as it should be always false in production code
            // Block to find if a group is already registered in the ecs

            // Todo if we try to attach a component not own by a system
            // We should attach it to a default system (It will mainly be used for things such as flag)
            // But we should warn the user that it is happening during compilation
            // #pragma message ("Warning goes here")
            // as they could try to create a component while the owner system was not initialized yet !
#ifdef PROD
            if (const auto& it = componentStorageMap.find(id); it == componentStorageMap.end())
            {
                LOG_ERROR("Component Registry", "Trying to retrieve a system that doesn't exist, Exiting");

                throw std::runtime_error(Strfy() << "System  [" << typeid(Type).name() << "] is not registered");
            }
#endif

            return static_cast<Own<Type>*>(componentStorageMap.at(id));
        }

        template <typename Event, typename EventListener>
        void addEventListener(EventListener* listener)
        {
            LOG_THIS_MEMBER("Component Registry");

            const auto& id = getTypeId<Event>();

            eventStorageMap[id].emplace((intptr_t)listener, [listener](const std::any& event)
            {
                listener->onEvent(std::any_cast<const Event&>(event));
            });
        }

        template <typename Event, typename EventListener>
        void removeEventListener(EventListener* listener)
        {
            LOG_THIS_MEMBER("Component Registry");

            const auto& id = getTypeId<Event>();

            if (const auto& it = eventStorageMap[id].find((intptr_t)listener); it != eventStorageMap[id].end())
            {
                eventStorageMap[id].erase(it);
            }
        }

        void addEventListener(_unique_id eventId, InterpreterSystem *listener);

        void removeEventListener(_unique_id eventId, InterpreterSystem *listener);

        template <typename EventListener>
        void addStandardEventListener(const std::string& name, EventListener* listener)
        {
            LOG_THIS_MEMBER("Component Registry");

            standardEventStorageMap[name].emplace((intptr_t)listener, [listener](const StandardEvent& event)
            {
                listener->onEvent(event);
            });
        }

        template <typename EventListener>
        void removeStandardEventListener(const std::string& name, EventListener* listener)
        {
            LOG_THIS_MEMBER("Component Registry");

            if (const auto& it = standardEventStorageMap[name].find((intptr_t)listener); it != standardEventStorageMap[name].end())
            {
                standardEventStorageMap[name].erase(it);
            }
        }

        void addStandardEventListener(EntityRef entity, const std::string& name, std::function<void(const StandardEvent&)> callback)
        {
            LOG_THIS_MEMBER("Component Registry");

            standardEventStorageMap[name].emplace(entity.id, callback);
        }

        void removeStandardEventListener(EntityRef entity, const std::string& name)
        {
            LOG_THIS_MEMBER("Component Registry");

            if (const auto& it = standardEventStorageMap[name].find(entity.id); it != standardEventStorageMap[name].end())
            {
                standardEventStorageMap[name].erase(it);
            }
        }

        void addEventListener(EntityRef entity, _unique_id eventId, std::function<void(const std::any&)> callback)
        {
            LOG_THIS_MEMBER("Component Registry");

            eventStorageMap[eventId].emplace(entity.id, callback);
        }

        void removeEventListener(EntityRef entity, _unique_id eventId)
        {
            LOG_THIS_MEMBER("Component Registry");

            if (const auto& it = eventStorageMap[eventId].find(entity.id); it != eventStorageMap[eventId].end())
            {
                eventStorageMap[eventId].erase(it);
            }
        }

        template <typename Event>
        void processEvent(const Event& event)
        {
            LOG_THIS_MEMBER("Component Registry");

            const std::any a = event;

            const auto& id = getTypeId<Event>();

            for (auto& eventListener : eventStorageMap[id])
            {
                eventListener.second(a);
            }
        }

        inline bool hasGroup(_unique_id groupId) const
        {
            return groupStorageMap.count(groupId) > 0;
        }

        template <typename Type, typename... Types>
        void storeGroup(Group<Type, Types...>* group) noexcept
        {
            LOG_THIS_MEMBER("Component Registry");

            // Todo see if there is a performance hit to keep this function or does it get optimized as it should be always false in production code
            // Block to find if a group is already registered in the ecs

            const auto& id = getTypeId<Group<Type, Types...>>();

#ifdef PROD
            if (const auto& it = groupStorageMap.find(id); it != groupStorageMap.end())
            {
                LOG_ERROR("Component Registry", "Trying to recreate a group that already existing with id: " << id << "Exiting");
                // Todo see if we need to throw an exception or not
                // throw std::runtime_error("Group already registered");
                return;
            }
#endif

            groupStorageMap.emplace(id, group);
        }

        template <typename Type, typename... Types>
        Group<Type, Types...>* retrieveGroup() const
        {
            LOG_THIS_MEMBER("Component Registry");

            const auto& id = getTypeId<Group<Type, Types...>>();

            // Todo see if there is a performance hit to keep this function or does it get optimized as it should be always false in production code
            // Block to find if a group is already registered in the ecs
#ifdef PROD
            if (const auto& it = groupStorageMap.find(id); it == groupStorageMap.end())
            {
                LOG_ERROR("Component Registry", "Trying to retrieve a group that doesn't exist, Exiting");

                throw std::runtime_error(Strfy() <<  "Group [" << typeid(Type).name() << "] is not registered");
            }
#endif

            return static_cast<Group<Type, Types...>*>(groupStorageMap.at(id));
        }

        template <typename Type>
        _unique_id getTypeId() const noexcept
        {
            auto globalId = getGlobalGenericId<Type>();

            auto it = idMap.find(globalId);

            // Todo add a variable to keep track of the running state of the ECS
            if (it == idMap.end())
            {
                LOG_MILE("ID", "Generating a new id for" << typeid(Type).name());

                return idMap[globalId] = idGenerator.generateId();
            }

            return it->second;

            // This can't work as the static make this id the same through all the different object
            // static const _unique_id id = idGenerator.generateId();
            // return id;
        }

        template <typename Type>
        void removeTypeId()
        {
            auto globalId = getGlobalGenericId<Type>();

            LOG_MILE("ID", "Removing Type: " << typeid(Type).name() << " has global id: " << globalId);

            auto it = idMap.find(globalId);

            if (it == idMap.end())
            {
                idMap.erase(globalId);
            }
        }

        void removeTypeId(_unique_id id);

        inline void detachComponentFromEntity(Entity* entity, _unique_id id) const
        {
            componentDeleteMap.at(id)(entity);
        }

        inline void serializeComponentFromEntity(Archive& archive, const Entity* entity, _unique_id id) const
        {
            componentSerializeMap.at(id)(archive, entity);
        }

        inline void deserializeComponentToEntity(const UnserializedObject& serializedString, EntityRef entity) const
        {
            const auto& name = serializedString.getObjectType();

            const auto& it = componentDeserializeMap.find(name);

            LOG_MILE("Component Registry", "Trying to deserialize component: " << name);

            if (it != componentDeserializeMap.end())
            {
                it->second(serializedString, entity);
            }
            else
            {
                LOG_ERROR("Component Registry", "Couldn't deserialize comp: " << name);
            }
        }

        inline void detachComponentFromEntity(const std::string& name, EntityRef entity) const
        {
            const auto& it = componentDetachMap.find(name);

            if (it != componentDetachMap.end())
            {
                it->second(entity);
            }
            else
            {
                LOG_ERROR("Component Registry", "Couldn't detach comp: " << name);
            }
        }

        void saveSystem(std::function<void(Archive&)> f, const std::string& objectName)
        {
            Serializer::ClassSerializer ar(&systemSerializer, objectName);

            f(ar.archive);
        }

        bool loadSystem(std::function<void(const UnserializedObject&)> f, const std::string& objectName)
        {
            const auto& map = systemSerializer.getSerializedMap();

            const auto& it = map.find(objectName);

            if (it != map.end())
            {
                f(UnserializedObject(it->second, objectName));
                return true;
            }
            else
            {
                LOG_WARNING("Registry", "Cannot load system: " << objectName << " system is not saved (This should happend in first load !)");
            }

            return false;
        }

        inline EntitySystem* world() const noexcept { return ecsRef; }

        // Common singleton system
    public:
        mutable UniqueIdGenerator idGenerator;

    public:
        inline size_t componentStorageMapSize() const noexcept     { return componentStorageMap.size(); }
        inline size_t componentDeleteMapSize() const noexcept      { return componentDeleteMap.size(); }
        inline size_t componentSerializeMapSize() const noexcept   { return componentSerializeMap.size(); }
        inline size_t componentDeserializeMapSize() const noexcept { return componentDeserializeMap.size(); }
        inline size_t groupStorageMapSize() const noexcept         { return groupStorageMap.size(); }

        template <typename Event>
        inline size_t eventStorageMapSize() const noexcept
        {
            const auto& id = getTypeId<Event>();

            if (const auto& it = eventStorageMap.find(id); it != eventStorageMap.end())
            {
                return it->second.size();
            }

            LOG_ERROR("Component Registry", "Could not find event: " << typeid(Event).name());

            return 0;
        }

        void saveRegistry() const { systemSerializer.save(); }

    private:
        template <typename Type>
        _unique_id getGlobalGenericId() const noexcept
        {
            static const _unique_id id = globalIdGenerator.generateId();
            return id;
        }

        static UniqueIdGenerator globalIdGenerator;

        mutable std::unordered_map<_unique_id, _unique_id> idMap;

    private:
        EntitySystem* const ecsRef;

        std::unordered_map<_unique_id, void*> componentStorageMap;
        std::unordered_map<_unique_id, std::function<void(Entity*)>> componentDeleteMap;
        std::unordered_map<_unique_id, std::function<void(Archive&, const Entity*)>> componentSerializeMap;
        std::unordered_map<std::string, std::function<void(const UnserializedObject&, EntityRef)>> componentDeserializeMap;
        std::unordered_map<std::string, std::function<void(EntityRef)>> componentDetachMap;
        std::unordered_map<_unique_id, void*> groupStorageMap;
        std::unordered_map<_unique_id, std::unordered_map<intptr_t, std::function<void(const std::any&)>>> eventStorageMap;
        std::unordered_map<std::string, std::unordered_map<intptr_t, std::function<void(const StandardEvent&)>>> standardEventStorageMap;

        Serializer systemSerializer;
    };

    template<>
    void ComponentRegistry::processEvent(const StandardEvent& event);

    // Todo setting a ref to a component that is not own by another system raises an exception std::map ::at: out of range
    // Todo add a function to check if the component is already in the registry and log an error if it is not
    template <class Type>
    struct Ref
    {
        // Take an unique id only to have the same signature as Own object
        Ref() { LOG_THIS_MEMBER("Ref"); }

        Ref(Own<Type> *ref) : ref(ref) { LOG_THIS_MEMBER("Ref"); }

        // Todo maybe remove the virtual destructor as the only thing that inherit Ref is Own
        // And it only inherits it to be stored in the registry.
        virtual ~Ref() { LOG_THIS_MEMBER("Ref"); }

        void setRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Ref");

            ref = registry->retrieve<Type>();
        }

        template <typename... Args>
        inline Type* internalCreateComponent(Entity* entity, Args&&... args)
        {
            LOG_THIS_MEMBER("Ref");

            return ref->internalCreateComponent(entity, std::forward<Args>(args)...);
        }

        inline void internalRemoveComponent(Entity* entity)
        {
            LOG_THIS_MEMBER("Ref");

            ref->internalRemoveComponent(entity);
        }

        inline typename ComponentSet<Type>::ComponentSetList view() const
        {
            LOG_THIS_MEMBER("Ref");

            return ref->view();
        }

        inline _unique_id getId() const
        {
            return ref->getId();
        }

        Own<Type> *ref;
    };

    template <class Type>
    struct Own : public Ref<Type>
    {
        Own() : Ref<Type>(this)
        {
            LOG_THIS_MEMBER("Own");
        }

        virtual ~Own() { LOG_THIS_MEMBER("Own"); }

        /**
         * @brief Add this owner object to the registry.
         *
         * @param registry The registry where to add the owner object.
         */
        void setRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Own");

            LOG_INFO("Own", "Type: " << typeid(Type).name() << " get the id: " << registry->getTypeId<Type>());

            // Store a pointer to this owner object in the registry
            registry->store<Type>(this);
        }

        /**
         * @brief Remove this owner object of the registry.
         *
         * @param registry The registry where to delete the owner object.
         */
        void unsetRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Own");

            LOG_INFO("Own", "Type: " << typeid(Type).name() << " as the id: " << registry->getTypeId<Type>());

            // Store a pointer to this owner object in the registry
            registry->unstore<Type>(this);
        }

        /**
         * @brief Create a new component of this type in the underlaying sparse set.
         *
         * @tparam Args Types of the arguments to pass to the constructor of the component.
         *
         * @param entity The entity to attach the new component to.
         * @param args Arguments to pass to the constructor of the component.
         *
         * @return Type* A pointer to the newly created component.
         */
        template <typename... Args>
        inline Type* internalCreateComponent(Entity* entity, Args&&... args)
        {
            LOG_THIS_MEMBER("Own");

            // Todo check if the entity already posses the component and just remplace it instead of creating a whole new component (rn attaching an already existing comp twice crashes, it appeares twice in the comp list)

            // Create a new component and store it in a sparse set along with the entity id using it
            auto comp = components.addComponent(entity, std::forward<Args>(args)...);

            // Add the component to the entity
            entity->componentList.emplace(_componentId);

            // Call the on component creation callbacks to register the component in potential groups
            for (const auto& callback : onComponentCreation)
                callback.second(entity);

            return comp;
        }

        /**
         * @brief Remove a component of this type from the underlaying sparse set.
         *
         * @param entity The entity to remove the component from.
         */
        inline void internalRemoveComponent(Entity* entity)
        {
            LOG_THIS_MEMBER("Own");

            // Call the on component deletion callbacks to remove the component from potential groups
            for (const auto& callback : onComponentDeletion)
                callback.second(entity);

            // Erase the component from the entity
            auto it = std::find(entity->componentList.begin(), entity->componentList.end(), _componentId);

            if (it != entity->componentList.end())
                entity->componentList.erase(it);

            // Remove the component from the sparse set
            if (components.has(entity->id))
                components.removeComponent(entity);
        }

        /**
         * @brief Get the component of an entity from the underlaying sparse set.
         *
         * @param id The id of the entity that has the component.
         *
         * @return A pointer to the component.
         */
        inline Type* getComponent(_unique_id id) const
        {
            return components.atEntity(id);
        }

        inline typename ComponentSet<Type>::ComponentSetList view() const
        {
            LOG_THIS_MEMBER("Own");

            return components.viewComponents();
        }

        /**
         * @brief Get the id of the component.
         *
         * @return _unique_id the id of the component.
         */
        inline _unique_id getId() const
        {
            return _componentId;
        }

        ComponentSet<Type> components;

        std::map<_unique_id, void(*)(EntityRef)> onComponentCreation;
        std::map<_unique_id, void(*)(EntityRef)> onComponentDeletion;

        _unique_id _componentId = 0;
    };

    template <typename Comp>
    struct CompRef
    {
        CompRef() : initialized(false), component(nullptr), entityId(0), ecsRef(nullptr) {}

        CompRef(Comp* component, _unique_id id, const EntitySystem* ecs, bool initialized = true) : initialized(initialized), component(component), entityId(id), ecsRef(ecs) {}

        CompRef(const CompRef& rhs)
        {
            (*this) = rhs;
        }

        void operator=(const CompRef& rhs);

        // Todo always check if the component was not initialized in between calls to make sure to update the correct one
        Comp* operator->();

        operator Comp*();

        inline bool empty() const { return component == nullptr; }

        Entity* getEntity() const;

        bool initialized;
        Comp* component;
        _unique_id entityId;
        const EntitySystem* ecsRef;
    };
}