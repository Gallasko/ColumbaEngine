#pragma once

#include "ECS/system.h"

#include "2D/simple2dobject.h"

#include "Input/inputcomponent.h"
#include "2D/collisionsystem.h"

#include "Systems/coresystems.h"

#include "Systems/basicsystems.h"

#include "Weapons/weapon.h"

#include "2D/camera2d.h"

#include "../config.h"

#include "Aseprite_Lib/AsepriteLoader.h"

namespace pg
{
    struct WallFlag : public Ctor
    {
        WallFlag() {}
        WallFlag(const WallFlag& rhs) : ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        WallFlag& operator=(const WallFlag& rhs)
        {
            ecsRef = rhs.ecsRef;
            entityId = rhs.entityId;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        EntitySystem* ecsRef;
        _unique_id entityId;
    };

    struct PlayerFlag : public Ctor
    {
        PlayerFlag() {}
        PlayerFlag(const PlayerFlag& rhs) : ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        PlayerFlag& operator=(const PlayerFlag& rhs)
        {
            ecsRef = rhs.ecsRef;
            entityId = rhs.entityId;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        EntitySystem* ecsRef;
        _unique_id entityId;

        bool inDodge = false;
    };

    struct AllyBulletFlag : public Ctor
    {
        AllyBulletFlag(float dmg = 1) : damage(dmg) {}
        AllyBulletFlag(const AllyBulletFlag& rhs) : damage(rhs.damage), ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        AllyBulletFlag& operator=(const AllyBulletFlag& rhs)
        {
            damage = rhs.damage;
            ecsRef = rhs.ecsRef;
            entityId = rhs.entityId;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        float damage = 1;

        EntitySystem* ecsRef;
        _unique_id entityId;
    };

    enum class CollectibleType { Gold, Weapon };
    struct CollectibleFlag
    {
        CollectibleFlag() : type(CollectibleType::Gold) {}
        CollectibleFlag(Weapon weapon) : type(CollectibleType::Weapon), weapon(weapon) {}
        CollectibleFlag(const CollectibleFlag& rhs) : type(rhs.type), weapon(rhs.weapon) {}

        CollectibleFlag& operator=(const CollectibleFlag& rhs)
        {
            type = rhs.type;
            weapon = rhs.weapon;

            return *this;
        }

        CollectibleType type;
        Weapon weapon;
    };

    struct PlayerMoveUp {};
    struct PlayerMoveDown {};
    struct PlayerMoveLeft {};
    struct PlayerMoveRight {};

    struct SpawnPlayerEvent { float x; float y; };

    struct PlayerInvincibilityEndEvent {};

    struct PlayerDodgeEndEvent {};

    struct PlayerHitEvent { float damage; };

    // Todo bug bullet can stay stuck in a wall if fired from within the wall

    struct PlayerSystem : public System<QueuedListener<OnMouseClick>, QueuedListener<ConfiguredKeyEvent<GameKeyConfig>>, QueuedListener<ConfiguredKeyEventReleased<GameKeyConfig>>, InitSys,
        Listener<PlayerMoveUp>, Listener<PlayerMoveDown>, Listener<PlayerMoveLeft>, Listener<PlayerMoveRight>, Listener<SpawnPlayerEvent>,
        Listener<PlayerHitEvent>, Listener<PlayerInvincibilityEndEvent>, Listener<PlayerDodgeEndEvent>,
        Listener<TickEvent>>
    {
        AsepriteFile animFile;
        PlayerSystem(const AsepriteFile& animFile) : animFile(animFile) {}

        virtual std::string getSystemName() const override { return "Player System"; }

        virtual void init() override;

        virtual void onEvent(const PlayerHitEvent& event) override;

        virtual void onEvent(const PlayerInvincibilityEndEvent& event) override
        {
            invincibility = false;
        }

        virtual void onEvent(const SpawnPlayerEvent& event) override
        {
            player->get<PositionComponent>()->setX(event.x);
            player->get<PositionComponent>()->setY(event.y);

            player->get<PositionComponent>()->setVisibility(true);
        }

        virtual void onEvent(const PlayerDodgeEndEvent& event) override;

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void onProcessEvent(const OnMouseClick& event) override;

        virtual void onProcessEvent(const ConfiguredKeyEvent<GameKeyConfig>& event) override;

        virtual void onProcessEvent(const ConfiguredKeyEventReleased<GameKeyConfig>& event) override
        {
            switch (event.value)
            {
            case GameKeyConfig::MoveLeft:
                leftTimer->stop();
                break;
            case GameKeyConfig::MoveRight:
                rightTimer->stop();
                break;
            case GameKeyConfig::MoveUp:
                upTimer->stop();
                break;
            case GameKeyConfig::MoveDown:
                bottomTimer->stop();
                break;

            default:
                break;
            }

            if (leftTimer->running == false and rightTimer->running == false)
                lastMoveDir.x = 0.f;

            if (upTimer->running == false and bottomTimer->running == false)
                lastMoveDir.y = 0.f;
        }

        void tryCollect();

        void tryDodge();

        void tryHeal();

        void printWeapon(const Weapon& weapon);

        void updateHealthUi();
        void updateWeaponUi();

        virtual void onEvent(const PlayerMoveUp&) override;

        virtual void onEvent(const PlayerMoveDown&) override;

        virtual void onEvent(const PlayerMoveLeft&) override;

        virtual void onEvent(const PlayerMoveRight&) override;

        virtual void execute() override;

        void movePlayer(float x, float y, bool scaleToMovespeed = true);

        EntityRef player;

        CompRef<Timer> upTimer;
        CompRef<Timer> leftTimer;
        CompRef<Timer> bottomTimer;
        CompRef<Timer> rightTimer;

        bool invincibility = false;

        CompRef<Timer> invicibilityTimer;
        CompRef<Timer> dodgeTimer;

        std::unordered_map<std::string, EntityRef> uiElements;
        float health = 5.0f;

        float deltaTime = 0.0f;

        bool tryingToDash = false;
        float dashDuration = 200.f;
        float dashElapsed = 0.f;
        float dashDistance = 85.f;
        constant::Vector2D dashDir = {0.f, 0.f};

        constant::Vector2D lastMoveDir{0.f, 0.f};

        float movespeed = 4.f;
    };

} // namespace pg
