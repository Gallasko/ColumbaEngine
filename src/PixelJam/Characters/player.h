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
        Listener<PlayerHitEvent>, Listener<PlayerInvincibilityEndEvent>, Listener<PlayerDodgeEndEvent>>
    {
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

        virtual void onEvent(const PlayerDodgeEndEvent& event) override
        {
            player->get<PlayerFlag>()->inDodge = false;
        }

        virtual void onProcessEvent(const OnMouseClick& event) override;

        virtual void onProcessEvent(const ConfiguredKeyEvent<GameKeyConfig>& event) override
        {
            switch (event.value)
            {
            case GameKeyConfig::MoveLeft:
                if (not leftTimer->running)
                    leftTimer->start();
                // if (rightTimer->running)
                //     rightTimer->stop();
                break;
            case GameKeyConfig::MoveRight:
                if (not rightTimer->running)
                    rightTimer->start();
                // if (leftTimer->running)
                //     leftTimer->stop();
                break;
            case GameKeyConfig::MoveUp:
                if (not upTimer->running)
                    upTimer->start();
                // if (bottomTimer->running)
                //     bottomTimer->stop();
                break;
            case GameKeyConfig::MoveDown:
                if (not bottomTimer->running)
                    bottomTimer->start();
                // if (upTimer->running)
                //     upTimer->stop();
                break;

            case GameKeyConfig::Interact:
                tryCollect();
                break;

            case GameKeyConfig::Dodge:
                tryDodge();
                break;

            default:
                break;
            }
        }

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
        }

        void tryCollect();

        void tryDodge()
        {
            if (player->get<PlayerFlag>()->inDodge)
                return;

            player->get<PlayerFlag>()->inDodge = true;
            
            dodgeTimer->start();
        }

        void printWeapon(const Weapon& weapon);

        virtual void onEvent(const PlayerMoveUp&) override
        {
            movePlayer(0.f, -movespeed);
        }

        virtual void onEvent(const PlayerMoveDown&) override
        {
            movePlayer(0.f, movespeed);
        }

        virtual void onEvent(const PlayerMoveLeft&) override
        {
            movePlayer(-movespeed, 0.f);
        }

        virtual void onEvent(const PlayerMoveRight&) override
        {
            movePlayer(movespeed, 0.f);
        }

        void movePlayer(float x, float y);

        EntityRef player;

        CompRef<Timer> upTimer;
        CompRef<Timer> leftTimer;
        CompRef<Timer> bottomTimer;
        CompRef<Timer> rightTimer;

        bool invincibility = false;

        CompRef<Timer> invicibilityTimer;
        CompRef<Timer> dodgeTimer;
        
        AsepriteFile playerAnimation; // anim

        std::unordered_map<std::string, EntityRef> uiElements;
        float health = 5.0f;

        float movespeed = 4.f;
    };

} // namespace pg
