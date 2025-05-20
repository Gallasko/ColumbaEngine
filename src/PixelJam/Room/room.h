#pragma once

#include <vector>

#include "ECS/system.h"

#include "Tiled_Lib/MapData.h"

#include "2D/position.h"
#include "2D/collisionsystem.h"
#include "2D/texture.h"

// Todo remove this it is only for debug of the room triggers
#include "2D/simple2dobject.h"

#include "Database/weapondatabase.h"
#include "Database/enemydatabase.h"

#include "Characters/player.h"

namespace pg
{
    /**
     * @brief Represents a spike trap that cycles through three states:
     *        In → Showing → Out → (loops).
     *
     * States:
     * - In:     Spikes are hidden; safe to walk on.
     * - Showing:Spikes are starting to emerge; visual warning only.
     * - Out:    Spikes are fully out; dangerous.
     */
    struct RoomSpike
    {

        /**
         * @brief The state of the spike trap.
         */
        enum class State {
            None,
            In,
            Showing,
            Out
        };

        /**
         * @brief The spike data from tiled
         */
        Spike spike;
        float inDuration;
        float showingDuration;
        float outDuration;

        float timer; // Time elapsed in the current state
        State state;
        std::vector<SpikeImage> spike_images_;
        std::vector<CompList<PositionComponent, UiAnchor, Texture2DComponent>> textures;
        EntitySystem * ecsRef;

        void changeState(State newState) {
            state = newState;
            //std::cout << "Change state to " << static_cast<int>(state) << std::endl;

            for (const auto & tex : textures) {
                auto texComp = tex.get<Texture2DComponent>();
                texComp->setTexture(getTextureName().textureName);

                if (state == State::Out) {
                    ecsRef->attach<SpikeFlag>(tex.entity.entity);
                }
                else {
                    ecsRef->detach<SpikeFlag>(tex.entity.entity);
                }
            }


        }

        SpikeImage getTextureName() const{
            //std::cout << static_cast<int>(state) << std::endl;
            int index = (static_cast<int>(state) - 1) % spike_images_.size();

           // std::cout << index << std::endl;

            return spike_images_[index];
        }

        void addTexture(const CompList<PositionComponent, UiAnchor, Texture2DComponent> & tex) {
            textures.push_back(tex);
        }

        RoomSpike(const Spike &s, const std::vector<SpikeImage> &spikeImages) : spike(s), inDuration(s.inDuration), showingDuration(s.showingDuration), outDuration(s.outDuration), timer(0), state(State::None),
                                                                         spike_images_(spikeImages) {
            // Apply cycle offset
            float timeLeft = s.timerOffset;

            while (timeLeft >= 0.0f) {
                switch (state) {
                    case State::In:
                        if (timeLeft < inDuration) {
                            timer = timeLeft;
                            return;
                        }
                        timeLeft -= inDuration;
                        changeState(State::Showing);
                        break;

                    case State::Showing:
                        if (timeLeft < showingDuration) {
                            timer = timeLeft;
                            return;
                        }
                        timeLeft -= showingDuration;
                        changeState(State::Out);
                        break;

                    case State::Out:
                        if (timeLeft < outDuration) {
                            timer = timeLeft;
                            return;
                        }
                        timeLeft -= outDuration;
                        changeState(State::In);
                        break;
                    case State::None:
                        changeState(State::In);
                        break;
                }
            }
        }

        /**
       * @brief Updates the spike state machine based on elapsed time.
       *
       * @param dt Delta time in seconds since the last update.
       */
        void update(const float dt) {
            timer += dt;

            switch (state) {
                case State::In:
                    if (timer >= inDuration) {
                        timer -= inDuration;
                        changeState(State::Showing);
                    }
                    break;

                case State::Showing:
                    if (timer >= showingDuration) {
                        timer -= showingDuration;
                        changeState(State::Out);
                    }
                    break;

                case State::Out:
                    if (timer >= outDuration) {
                        timer -= outDuration;
                        changeState(State::In);
                    }
                    break;
                case State::None:
                    changeState(State::In);
                    break;
            }

            //std::cout << "Update spike " << std::to_string(static_cast<int>(state)) << std::endl;
        }
    };

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

    struct RoomDoorHolder
    {
        _unique_id entityId;
        Door door;
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
        std::vector<RoomDoorHolder> doors;
        std::vector<Spawner> spawners;
    };


    struct EnterRoomEvent
    {
        int roomIndex;
    };
    
    struct ResetRoomEvent {};

    struct RoomSystem : public System<Own<RoomTriggerFlag>, Listener<EnemyDeathEvent>, Listener<SpawnWaveEvent>, Listener<EnterRoomEvent>, InitSys, Listener<TickEvent>, Listener<ResetRoomEvent>>
    {
        RoomSystem(WeaponDatabase* weaponDb, EnemyDatabase* enemyDb) : weaponDb(weaponDb), enemyDb(enemyDb)
        {
        }

        void startLevel()
        {
            auto pos = playerSpawn.positionSPixels;
            ecsRef->sendEvent(SpawnPlayerEvent{pos.x, pos.y});
        }

        virtual void init() override
        {
            auto ent = ecsRef->createEntity();

            spawnTimer = ecsRef->attach<Timer>(ent);

            spawnTimer->interval = 1000;
            spawnTimer->oneShot = true;
            spawnTimer->callback = makeCallable<SpawnWaveEvent>();
        }

        virtual void onEvent(const ResetRoomEvent&) override
        {
            for (auto& room : rooms)
            {
                room.second.state = RoomState::Unexplored;

                for (auto& door : room.second.doors)
                {
                    auto doorEnt = ecsRef->getEntity(door.entityId);
                    
                    doorEnt->get<Simple2DObject>()->setColors({0.f, 125.f, 0.f, 80.f});

                    ecsRef->detach<WallFlag>(doorEnt);
                    // ecsRef->detach<CollisionComponent>(doorEnt);
                }
            }

            currentRoom = -1;

            spawnTimer->stop();

            startLevel();
        }

        const SpawnData& selectRandomSpawn(const std::vector<SpawnData>& spawns)
        {
            // Calculate the total weight (sum of spawn probabilities)
            float totalWeight = 0.0f;
            for (const auto& spawn : spawns)
            {
                totalWeight += spawn.spawnProba;
            }

            // Generate a random number in the range [0, totalWeight)
            float randomWeight = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * totalWeight;

            // Select the spawn based on the random weight
            float cumulativeWeight = 0.0f;
            for (const auto& spawn : spawns)
            {
                cumulativeWeight += spawn.spawnProba;
                if (randomWeight <= cumulativeWeight)
                {
                    return spawn;
                }
            }

            // Fallback in case of rounding errors (shouldn't happen)
            return spawns.back();
        }

        virtual void onEvent(const SpawnWaveEvent&) override
        {
            auto it = rooms.find(currentRoom);

            if (it == rooms.end())
            {
                LOG_ERROR("RoomSystem", "Room not found");
                return;
            }

            std::vector<EnemySpawnData> enemiesToSpawn;

            size_t nbSpawnedEnemiesInLoop = 0;
            bool fastBreak = false;

            do
            {
                for (const auto& spawner : it->second.spawners)
                {
                    if (nbSpawnedEnemies + nbSpawnedEnemiesInLoop >= static_cast<size_t>(it->second.data.nbEnemy))
                    {
                        LOG_INFO("RoomSystem", "Max number of enemies spawned");
                        fastBreak = true;
                        break;
                    }

                    auto spawnData = selectRandomSpawn(spawner.spawns);

                    auto enemyData = enemyDb->getEnemy(spawnData.enemyId);

                    if (not enemyData.canSpawn)
                    {
                        LOG_ERROR("RoomSystem", "Enemy cannot spawn");
                        continue;
                    }

                    try
                    {
                        enemyData.weapon = weaponDb->getWeapon(enemyData.weaponId);
                    }
                    catch (const std::exception& e)
                    {
                        LOG_ERROR("RoomSystem", "Weapon not found: " << enemyData.weaponId << ": " << e.what());
                    }

                    EnemySpawnData enemySpawnData;

                    enemySpawnData.enemy = enemyData;
                    enemySpawnData.x = spawner.posXInSPixels;
                    enemySpawnData.y = spawner.posYInSPixels;

                    enemiesToSpawn.push_back(enemySpawnData);

                    nbSpawnedEnemiesInLoop++;
                }

            } while (nbSpawnedEnemiesInLoop == 0 and not fastBreak);

            nbSpawnedEnemies += nbSpawnedEnemiesInLoop;

            ecsRef->sendEvent(SpawnEnemiesEvent{enemiesToSpawn});
        }

        virtual void onEvent(const EnemyDeathEvent& event) override
        {
            nbSlayedEnemies++;

            auto it = rooms.find(currentRoom);

            if (it == rooms.end())
            {
                LOG_ERROR("RoomSystem", "Room not found");
                return;
            }

            if (nbSlayedEnemies >= static_cast<size_t>(it->second.data.nbEnemy))
            {
                it->second.state = RoomState::Completed;

                for (auto& door : it->second.doors)
                {
                    auto doorEnt = ecsRef->getEntity(door.entityId);

                    doorEnt->get<Simple2DObject>()->setColors({0.f, 125.f, 0.f, 80.f});

                    ecsRef->detach<WallFlag>(doorEnt);
                    // ecsRef->detach<CollisionComponent>(doorEnt);
                }
            }
            else if (nbSlayedEnemies == nbSpawnedEnemies)
            {
                spawnTimer->start();
            }
        }

        bool roomHasValidSpawners(const std::vector<Spawner>& spawners) const
        {
            if (spawners.empty())
            {
                return false;
            }

            for (const auto& spawner : spawners)
            {
                for (const auto& spawn : spawner.spawns)
                {
                    auto enemy = enemyDb->getEnemy(spawn.enemyId);

                    if (enemy.canSpawn)
                    {
                        return true;
                    }
                }
            }

            return false;
        }

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

            if (not roomHasValidSpawners(it->second.spawners))
            {
                LOG_ERROR("RoomSystem", "Room has no valid spawners");
                return;
            }

            if (it->second.state == RoomState::Unexplored)
            {
                it->second.state = RoomState::Active;
            }

            for (const auto& door : it->second.doors)
            {
                auto doorEnt = ecsRef->getEntity(door.entityId);

                doorEnt->get<Simple2DObject>()->setColors({125.f, 0.f, 0.f, 80.f});

                // Todo attching it here doesn't work for some reason (no collision)
                // ecsRef->attach<CollisionComponent>(doorEnt, 0);

                ecsRef->attach<WallFlag>(doorEnt);
            }

            currentRoom = event.roomIndex;

            nbSlayedEnemies = 0;
            nbSpawnedEnemies = 0;

            spawnTimer->start();
        }

        void addPlayerSpawn(const SpawnPoint& spawn)
        {
            playerSpawn = spawn;
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

            auto rect = door.rectInSPixels;

            auto doorEnt = makeSimple2DShape(ecsRef, Shape2D::Square, rect.width, rect.height, {0.f, 125.f, 0.f, 80.f});

            doorEnt.get<PositionComponent>()->setX(rect.topLeftCornerX);
            doorEnt.get<PositionComponent>()->setY(rect.topLeftCornerY);
            doorEnt.get<PositionComponent>()->setZ(10);

            doorEnt.get<Simple2DObject>()->setViewport(1);

            ecsRef->attach<CollisionComponent>(doorEnt.entity, 0);

            it->second.doors.push_back(RoomDoorHolder{doorEnt.entity.id, door});
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

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        // Todo try detect if a user defines an execute and StoragePolicy at the same time and warn them
        virtual void execute() override {
            if (deltaTime == 0.f)
                return;

            // std::cout << "Update room " << deltaTime << std::endl;
           //std::cout << "Update spikes  " << std::endl;

            for (auto& spike : spikes) {
                spike.update(deltaTime / 1000.0f);
            }

            //std::cout << "Update spikes END " << std::endl;

            deltaTime = 0.f;
        }

        /**
         * Check if all rooms have valid triggers and spawners
         * Used for debugging purposes
         */
        void checkRoomsIntegrity()
        {
            for (const auto& room : rooms)
            {
                if (room.second.triggers.size() != room.second.doors.size())
                {
                    LOG_WARNING("RoomSystem", "Room " << room.first << " has different number of triggers and doors");
                }

                if (not roomHasValidSpawners(room.second.spawners))
                {
                    LOG_WARNING("RoomSystem", "Room " << room.first << " has no valid spawners");
                }
            }
        }

        void addGold(Gold gold) {
            AsepriteLoader aseprite_loader;
            const auto anim = aseprite_loader.loadAnim("res/sprites/Gold_Pile.json");
            auto tex = makeUiTexture(ecsRef, anim.frames[0].widthInSPixels * 3, anim.frames[0].heightInSPixels * 3, anim.frames[0].textureName);
            auto texComp = tex.get<Texture2DComponent>();
            texComp->setViewport(1);

            auto posComp = tex.get<PositionComponent>();
            posComp->setX(gold.posXInSPixels - anim.frames[0].widthInSPixels * 1.5f);
            posComp->setY(gold.posYInSPixels- anim.frames[0].heightInSPixels * 1.5f);
            posComp->setZ(9);
        }

        std::vector<RoomSpike> spikes;

        void addSpike(RoomSpike & room_spike) {

            room_spike.ecsRef = ecsRef;

            for (int iWidth = 0; iWidth < room_spike.spike.rectTilesSpace.widthInTiles; ++iWidth) {
                for (int iHeight = 0; iHeight < room_spike.spike.rectTilesSpace.heightInTiles; ++iHeight) {

                    auto posX = room_spike.spike.rectInSPixels.topLeftCornerX + room_spike.spike.scaledTileWidth * iWidth;
                    auto posY = room_spike.spike.rectInSPixels.topLeftCornerY + room_spike.spike.scaledTileHeight * iHeight;

                    auto tex = makeUiTexture(ecsRef, room_spike.spike.scaledTileWidth, room_spike.spike.scaledTileHeight, room_spike.getTextureName().textureName);
                    auto texComp = tex.get<Texture2DComponent>();
                    texComp->setViewport(1);

                    auto posComp = tex.get<PositionComponent>();
                    posComp->setX(posX);
                    posComp->setY(posY);
                    posComp->setZ(9);

                    ecsRef->attach<CollisionComponent>(tex.entity, 12);

                    room_spike.addTexture(tex);
                }
            }

            spikes.push_back(room_spike);
        }

        WeaponDatabase* weaponDb;
        EnemyDatabase* enemyDb;

        std::map<int, Room> rooms;

        SpawnPoint playerSpawn;

        int currentRoom = -1;

        CompRef<Timer> spawnTimer;

        size_t nbSlayedEnemies = 0;
        size_t nbSpawnedEnemies = 0;

        float deltaTime = 0.f;
    };

    struct TestGridFlag : public Ctor
    {
        TestGridFlag(const constant::Vector4D& color) : color(color) {}
        TestGridFlag(const TestGridFlag& other) : color(other.color), ecsRef(other.ecsRef), entityId(other.entityId) {}

        TestGridFlag& operator=(const TestGridFlag& other)
        {
            color = other.color;

            ecsRef = other.ecsRef;
            entityId = other.entityId;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        constant::Vector4D color;

        EntitySystem* ecsRef;
        _unique_id entityId;
    };

    void drawDebugGrid(EntitySystem* ecs, int worldW, int worldH);
}