#include "stdafx.h"

#include "commanddispatcher.h"

#include "entity.h"
#include "entitysystem.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Command Dispatcher";
    }

    /**
     * @brief Enqueue the creation of a new entity
     *
     * @return EntityRef A reference object to the newly created entity
     */
    EntityRef CommandDispatcher::createEntity()
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = new Entity(ecsRef->registry.idGenerator.generateId(), ecsRef);

        if (not entityCQueue.enqueue(EntityCommand{entity, EntityCommand::EntityCommandType::creation}))
        {
            LOG_ERROR(DOM, "Could not enqueue the creation of entity " << entity->id);
            return nullptr;
        }

        return {entity, false};
    }

    /**
     * @brief Enqueue the deletion of an entity
     *
     * @param entity The entity to be deleted
     */
    void CommandDispatcher::deleteEntity(Entity* entity)
    {
        LOG_THIS_MEMBER(DOM);

        if (not entityDQueue.enqueue(EntityCommand{entity, EntityCommand::EntityCommandType::deletion}))
        {
            LOG_ERROR(DOM, "Could not enqueue the deletion of entity " << entity->id);
        }
    }

    /**
     * @brief Process all the pending commands
     */
    void CommandDispatcher::process()
    {
        LOG_THIS_MEMBER(DOM);

        // Entity commands
        EntityCommand item1(nullptr, EntityCommand::EntityCommandType::creation);
        EntityCommand item2(nullptr, EntityCommand::EntityCommandType::deletion);

        bool found1 = entityCQueue.try_dequeue(item1);
        bool found2 = entityDQueue.try_dequeue(item2);

        // Component commands
        ComponentCreateCommand item3;
        ComponentDeleteCommand item4;

        bool found3 = componentCQueue.try_dequeue(item3);
        bool found4 = componentDQueue.try_dequeue(item4);

        // Put all the components in this map to be sure to only delete the components once even if two different system ask to remove it at the same time !
        std::map<Entity*, _unique_id> componentsToBeDeleted;

        // First try to delete all the components requested
        while (found4)
        {
            componentsToBeDeleted.emplace(item4.entity, item4.compId);
            // ecsRef->registry.detachComponentFromEntity(item4.entity, item4.compId);

            found4 = componentDQueue.try_dequeue(item4);
        }

        for (const auto& comp : componentsToBeDeleted)
        {
            ecsRef->registry.detachComponentFromEntity(comp.first, comp.second);
        }

        // Put all the entities in this set to be sure to only delete the entities once even if two different system ask to remove it at the same time !
        std::set<Entity*> entitiesToBeDeleted;

        // Then try to delete all the entities requested
        while (found2)
        {
            entitiesToBeDeleted.insert(item2.entity);
            // ecsRef->deleteEntityFromPool(item2.entity);

            found2 = entityDQueue.try_dequeue(item2);
        }

        for (auto entity : entitiesToBeDeleted)
        {
            ecsRef->deleteEntityFromPool(entity);
        }

        // Then try to create all the new entities requested
        while (found1)
        {
            ecsRef->addEntityToPool(item1.entity);
            delete item1.entity;

            found1 = entityCQueue.try_dequeue(item1);
        }

        // Finally try to create all the components requested
        while (found3)
        {
            if (not item3.entity.empty())
                item3.addInEcs(ecsRef, item3.entity, item3.component);

            found3 = componentCQueue.try_dequeue(item3);
        }
    }
}