/**
 * @file group.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-09-02
 *
 * @copyright Copyright (c) 2022
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "uniqueid.h"
#include "sparseset.h"

#include "entity.h"
#include "componentregistry.h"
#include "eventlistener.h"

#include "logger.h"

#include <vector>
#include <set>
#include <algorithm>

// Todo to replace with the thread pool manager
#include <functional>

#include "Memory/threadpool.h"
#include "Memory/parallelfor.h"

namespace pg
{
    // Type forwarding
    // class ComponentRegistry;

    // Todo find why sometime the component list gets invalidated
    template <typename Type>
    struct Getter
    {
        Getter(_unique_id entityId) : owner(nullptr), id(entityId) { LOG_THIS_MEMBER("Ecs Group"); }
        // Getter(Type* value) : value(value) { LOG_THIS_MEMBER("Ecs Group"); }

        Type* get() const { LOG_THIS_MEMBER("Ecs Group"); return owner->atEntity(id); }
        void set(const ComponentSet<Type>* owner) { LOG_THIS_MEMBER("Ecs Group"); this->owner = owner; }

        const ComponentSet<Type>* owner;
        const _unique_id id;
    };

    template <typename... Types>
    struct GroupElement : public Getter<Types>...
    {
        GroupElement(EntityRef entity, EntitySystem *const ecsRef, const _unique_id& entityId) : Getter<Types>(entityId)..., entity(entity), ecsRef(ecsRef), entityId(entityId) { LOG_THIS_MEMBER("Ecs Group"); }
        // GroupElement(Entity *entity, EntitySystem *const ecsRef, const _unique_id& entityId, Types*... values) : Getter<Types>(values)..., entity(entity), ecsRef(ecsRef), entityId(entityId) { LOG_THIS_MEMBER("Ecs Group"); }

        // Todo fix this !
        // template <typename Type>
        // Type* get() const { LOG_THIS_MEMBER("Ecs Group"); return static_cast<const Getter<Type>*>(this)->get(); }

        template <typename Type>
        CompRef<Type> get() const { LOG_THIS_MEMBER("Ecs Group"); return CompRef<Type>(entity.get<Type>(), entityId, ecsRef, true); }

        template <typename Type>
        void set(const ComponentSet<Type> *owner) { LOG_THIS_MEMBER("Ecs Group"); static_cast<Getter<Type>*>(this)->set(owner); }

        inline EntitySystem* world() const noexcept { LOG_THIS_MEMBER("Ecs Group"); return ecsRef; }

        EntityRef entity;
        EntitySystem *const ecsRef = nullptr;
        const _unique_id entityId;
        bool toBeDeleted = false;
    };

    template <typename Type, typename... Types>
    struct SetHolder
    {
        SetHolder(const SparseSet* set, void(*f)(const SparseSet*, GroupElement<Type, Types...>&, size_t)) : set(set), setElement(f) { LOG_THIS_MEMBER("Ecs Group"); }

        const SparseSet* set;
        void (*setElement) (const SparseSet*, GroupElement<Type, Types...>&, size_t);
    };

    template <typename Set, typename Type, typename... Types>
    void getFromSet(const SparseSet* set, GroupElement<Type, Types...>& element, size_t id)
    {
        LOG_THIS("Ecs Group");

        const auto& pos = set->find(id);

        if (pos != 0)
            element.set(static_cast<const Set*>(set));
        else
            element.toBeDeleted = true;
    }

    template <typename GroupName>
    struct OnCompCreatedCheckForGroup
    {
        EntityRef entity;
    };

    template <typename GroupName>
    struct OnCompDeletionCheckForGroup
    {
        _unique_id id;

        std::set<Entity::EntityHeld> compList;
    };

    struct AbstractGroup
    {
        virtual ~AbstractGroup() {}
    };

    template <typename Type, typename... Types>
    struct Group : public AbstractGroup, Listener<OnCompCreatedCheckForGroup<Group<Type, Types...>>>, Listener<OnCompDeletionCheckForGroup<Group<Type, Types...>>>
    {
        virtual void onEvent(const OnCompCreatedCheckForGroup<Group<Type, Types...>>& event) override
        {
            LOG_THIS_MEMBER("Ecs Group");

            auto entity = event.entity;
            auto& id = entity.id;

            if (isEntityInGroup(entity) and elements.atEntity(id) == nullptr)
            {
                LOG_MILE("Group", "Entity " << id << " is in group " << this->id);

                GroupElement<Type, Types...> element(entity, this->world(), id);

                for (size_t j = 0; j < nbOfSets; j++)
                {
                    setList[j]->setElement(setList[j]->set, element, id);
                }

                for (const auto& callback : onAddGroup)
                    callback(entity);

                if (not element.toBeDeleted)
                    elements.addComponent(id, element);
            }
        }

        virtual void onEvent(const OnCompDeletionCheckForGroup<Group<Type, Types...>>& event) override
        {
            LOG_THIS_MEMBER("Ecs Group");

            if (registry and std::includes(event.compList.begin(), event.compList.end(), compIdList.begin(), compIdList.end()))
            {
                LOG_MILE("Group", "Entity " << event.id << " is in group " << this->id);

                for (const auto& callback : onDelGroup)
                    callback(registry->world(), event.id);

                elements.removeComponent(event.id);
            }
        }

        Group(_unique_id id) : id(id) { LOG_THIS_MEMBER("Ecs Group"); }
        virtual ~Group() { LOG_THIS_MEMBER("Ecs Group"); }

        void setRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Ecs Group");

            this->registry = registry;
            static_cast<Listener<OnCompCreatedCheckForGroup<Group<Type, Types...>>>*>(this)->setRegistry(registry);
            static_cast<Listener<OnCompDeletionCheckForGroup<Group<Type, Types...>>>*>(this)->setRegistry(registry);
            registry->storeGroup<Type, Types...>(this);
        }

        void addOnGroup(const std::function<void(EntityRef)>& callback)
        {
            LOG_THIS_MEMBER("Ecs Group");

            onAddGroup.push_back(callback);

            for (const auto& element : elements.viewComponents())
            {
                callback(element->entity);
            }
        }

        void removeOfGroup(const std::function<void(EntitySystem*, _unique_id)>& callback)
        {
            LOG_THIS_MEMBER("Ecs Group");

            onDelGroup.push_back(callback);
        }

        template <typename Comp>
        void checkGroupTypeExistence()
        {
            if (not registry->hasTypeId<Comp>())
            {
                LOG_WARNING("ECS", "Component [" << typeid(Comp).name() << "] is not registered in the ECS, attaching it to the default flag system instead");
                LOG_WARNING("ECS", "This is a costly operation to do during runtime, you should register the component in the ECS using registerFlagComponent<Type>()");

                registry->registerFlagComponent<Comp>();
            }
        }

        template <typename Comp, typename Comp2, typename... Comps>
        void checkGroupTypeExistence()
        {
            checkGroupTypeExistence<Comp>();
            checkGroupTypeExistence<Comp2, Comps...>();
        }

        void process();

        template <typename Set>
        inline void addInList(SetHolder<Type, Types...> **list, size_t index, const Set& set)
        {
            LOG_THIS_MEMBER("Ecs Group");

            list[index] = new SetHolder<Type, Types...> ( &set, &getFromSet<Set, Type, Types...> );
        }

        template <typename Set>
        inline void addEventToSet(Set setN);

        template <typename Set>
        inline void populateList(SetHolder<Type, Types...> **list, size_t index, Set setN)
        {
            LOG_THIS_MEMBER("Ecs Group");

            addEventToSet(setN);

            compIdList.emplace(setN->getId());

            addInList(list, index, setN->components);
        }

        template <typename Set, typename... Sets>
        inline void populateList(SetHolder<Type, Types...> **list, size_t index, Set setN, Sets... sets)
        {
            LOG_THIS_MEMBER("Ecs Group");

            addEventToSet(setN);

            compIdList.emplace(setN->getId());

            addInList(list, index, setN->components);

            populateList(list, index + 1, sets...);
        }

        inline bool isEntityInGroup(EntityRef entity) const
        {
            LOG_THIS_MEMBER("Ecs Group");

            return std::includes(entity->componentList.begin(), entity->componentList.end(), compIdList.begin(), compIdList.end());
        }

        inline EntitySystem* world() const noexcept { LOG_THIS_MEMBER("Ecs Group"); return registry->world(); }

        _unique_id id;
        ComponentRegistry* registry;
        GroupSet<GroupElement<Type, Types...>> elements;
        std::set<_unique_id> compIdList;

        constexpr static size_t nbOfSets = sizeof...(Types) + 1;
        SetHolder<Type, Types...> *setList[nbOfSets];

        std::vector<std::function<void(EntityRef)>> onAddGroup;
        std::vector<std::function<void(EntitySystem*, _unique_id)>> onDelGroup;
    };
}