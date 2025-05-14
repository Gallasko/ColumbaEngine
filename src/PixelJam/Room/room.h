#pragma once

#include "ECS/system.h"

#include "Tiled_Lib/MapData.h"

#include "2D/position.h"
#include "2D/collisionsystem.h"
// Todo remove this it is only for debug of the room triggers
#include "2D/simple2dobject.h"

namespace pg
{
    struct RoomTriggerFlag
    {
        RoomTriggerFlag(int roomIndex) : roomIndex(roomIndex) {}
        RoomTriggerFlag(const RoomTriggerFlag& rhs) : roomIndex(rhs.roomIndex) {}

        RoomTriggerFlag& operator=(const RoomTriggerFlag& rhs)
        {
            roomIndex = rhs.roomIndex;

            return *this;
        }

        int roomIndex;
    };

    struct RoomTriggerHolder
    {
        EntityRef entity;
        RoomTrigger trigger;
    };

    enum class RoomState : uint8_t
    {
        Unexplored = 0,
        Active,
        Completed,
    };

    struct Room
    {
        Room(const RoomData& data) : data(data) {}
        Room(const Room& rhs) : data(rhs.data),
            state(rhs.state),
            triggers(rhs.triggers),
            doors(rhs.doors),
            spawners(rhs.spawners) {}

        Room& operator=(const Room& rhs)
        {
            data = rhs.data;
            state = rhs.state;
            triggers = rhs.triggers;
            doors = rhs.doors;
            spawners = rhs.spawners;

            return *this;
        }

        RoomData data;

        RoomState state = RoomState::Unexplored;

        std::vector<RoomTriggerHolder> triggers;
        std::vector<Door> doors;
        std::vector<Spawner> spawners;
    };


    struct EnterRoomEvent
    {
        int roomIndex;
    };

    struct RoomSystem : public System<Own<RoomTriggerFlag>, Listener<EnterRoomEvent>, StoragePolicy>
    {
        virtual void onEvent(const EnterRoomEvent& event) override
        {
            auto it = rooms.find(event.roomIndex);

            if (it == rooms.end())
            {
                LOG_ERROR("RoomSystem", "Room not found");
                return;
            }

            if (it->second.state == RoomState::Completed or it->second.state == RoomState::Active)
            {
                LOG_INFO("RoomSystem", "No need to trigger the room again");
                return;
            }

            if (it->second.state == RoomState::Unexplored)
            {
                it->second.state = RoomState::Active;
            }
        }

        void addRoom(const RoomData& data)
        {
            rooms.emplace(data.roomIndex, data);
        }

        void addRoomTrigger(const RoomTrigger& trigger)
        {
            auto it = rooms.find(trigger.roomIndex);
            if (it == rooms.end())
            {
                LOG_ERROR("RoomSystem", "Room not found for trigger");
                return;
            }

            auto triggerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, trigger.rectInSPixels.width, trigger.rectInSPixels.height, {125.f, 0.f, 0.f, 80.f});

            triggerEnt.get<PositionComponent>()->setX(trigger.rectInSPixels.topLeftCornerX);
            triggerEnt.get<PositionComponent>()->setY(trigger.rectInSPixels.topLeftCornerY);
            triggerEnt.get<PositionComponent>()->setZ(10);

            triggerEnt.get<Simple2DObject>()->setViewport(1);

            std::vector<size_t> collidableLayer = {1};

            ecsRef->attach<CollisionComponent>(triggerEnt.entity, 6, 1.0, collidableLayer);

            ecsRef->attach<RoomTriggerFlag>(triggerEnt.entity, trigger.roomIndex);

            it->second.triggers.push_back(RoomTriggerHolder{triggerEnt.entity, trigger});
        }

        void addDoor(const Door& door)
        {
            auto it = rooms.find(door.roomIndex);

            if (it == rooms.end())
            {
                LOG_ERROR("RoomSystem", "Room not found for door");
                return;
            }

            it->second.doors.push_back(door);
        }

        void addSpawner(const Spawner& spawner)
        {
            auto it = rooms.find(spawner.roomIndex);

            if (it == rooms.end())
            {
                LOG_ERROR("RoomSystem", "Room not found for spawner");
                return;
            }

            auto spawnerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50, 50, {125.f, 0.f, 125.f, 80.f});

            spawnerEnt.get<PositionComponent>()->setX(spawner.posXInSPixels);
            spawnerEnt.get<PositionComponent>()->setY(spawner.posYInSPixels);
            spawnerEnt.get<PositionComponent>()->setZ(10);

            spawnerEnt.get<Simple2DObject>()->setViewport(1);

            it->second.spawners.push_back(spawner);
        }

        void removeRoom(int roomIndex)
        {
            auto it = rooms.find(roomIndex);

            if (it == rooms.end())
            {
                LOG_ERROR("RoomSystem", "Room not found");
                return;
            }

            rooms.erase(it);
        }

        const Room& getRoom(int roomIndex) const
        {
            return rooms.at(roomIndex);
        }

        bool roomExists(int roomIndex) const
        {
            return rooms.find(roomIndex) != rooms.end();
        }

        void clear()
        {
            rooms.clear();
        }

        std::unordered_map<int, Room> rooms;
    };
}