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

#include "UI/prefab.h"
#include "UI/ttftext.h"

namespace pg
{
    template<typename EcsType>
    CompList<PositionComponent, UiAnchor, Prefab> makeOutlinedTTFText(
        EcsType* ecs,
        float x, float y, float z,
        const std::string& fontPath,
        const std::string& text,
        float scale = 1.0f,
        constant::Vector4D fillColor    = {255,255,255,255},
        constant::Vector4D outlineColor = {0,0,0,255},
        int thickness                   = 2,
        size_t viewport                 = 0
    )
    {
        // 1) Create the prefab root
        auto anchorEnt = makeAnchoredPrefab(ecs, x, y, z);

        auto ui = anchorEnt.template get<PositionComponent>();
        auto pfEnt = anchorEnt.template get<Prefab>();
        auto anchor = anchorEnt.template get<UiAnchor>();

        // Helper to make one TTF child at an offset
        auto createChild = [&](float dx, float dy, const constant::Vector4D& color) {
            auto outlineTTF = makeTTFText(ecs, 0, 0, z, fontPath, text, scale, color);

            outlineTTF.template get<TTFText>()->setViewport(viewport);

            auto canchor = outlineTTF.template get<UiAnchor>();

            canchor->setTopAnchor(anchor->top);
            canchor->setTopMargin(dy);
            canchor->setLeftAnchor(anchor->left);
            canchor->setLeftMargin(dx);
            canchor->setZConstrain(PosConstrain{anchorEnt.entity.id, AnchorType::Z, PosOpType::Sub, 1});

            // add into prefab
            pfEnt->addToPrefab(outlineTTF.entity);
            return outlineTTF.entity;
        };

        // 2) Four outline copies
        createChild(+thickness,  0.f, outlineColor);
        createChild(-thickness,  0.f, outlineColor);
        createChild( 0.f,        +thickness, outlineColor);
        createChild( 0.f,       -thickness, outlineColor);

        // 3) The center (fill) copy, and mark as main
        auto mainTTF = makeTTFText(ecs, x, y, z + 1, fontPath, text, scale, fillColor);

        mainTTF.template get<TTFText>()->setViewport(viewport);

        // auto manchor = mainTTF.template get<UiAnchor>();
        // manchor->setTopAnchor(anchor->top);
        // manchor->setLeftAnchor(anchor->left);
        // manchor->setZConstrain(PosConstrain{pfEnt.entity.id, AnchorType::Z, PosOpType::Add, 1});

        pfEnt->setMainEntity(mainTTF.entity);

        return CompList<PositionComponent, UiAnchor, Prefab>(anchorEnt.entity, ui, anchor, pfEnt);
    }

    struct HoleFlag : public Ctor {
        HoleFlag() {}
        HoleFlag(const HoleFlag& rhs) : ecsRef(rhs.ecsRef), entityId(rhs.entityId) {}

        HoleFlag& operator=(const HoleFlag& rhs)
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
    struct CollectibleFlag : public Ctor
    {
        CollectibleFlag() : type(CollectibleType::Gold) {}
        CollectibleFlag(Weapon weapon) : type(CollectibleType::Weapon), weapon(weapon) {}
        CollectibleFlag(const CollectibleFlag& rhs) : ecsRef(rhs.ecsRef), entityId(rhs.entityId), type(rhs.type), weapon(rhs.weapon) {}

        CollectibleFlag& operator=(const CollectibleFlag& rhs)
        {
            ecsRef = rhs.ecsRef;
            entityId = rhs.entityId;
            type = rhs.type;
            weapon = rhs.weapon;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        EntitySystem* ecsRef;
        _unique_id entityId;

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

    struct CameraShakeComponent : public Ctor
    {
        CameraShakeComponent(float duration = 0.0f, float magnitude = 0.0f) : duration(duration), magnitude(magnitude), elapsed(0.0f), active(false) {}

        float duration;
        float magnitude;
        float elapsed;
        bool active = false;

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        void shake(float duration, float magnitude)
        { 
            active = true;
            this->duration = duration;
            this->magnitude = magnitude; 
            elapsed = 0.0f;
        }

        EntitySystem* ecsRef = nullptr;
        _unique_id entityId = 0;
    };

    // Todo make an update sys that get the current delta time of the ecs

    // Todo Init sys should be defined after all the own if you want to register a group in init

    struct CameraShakeSystem : public System<Own<CameraShakeComponent>, InitSys, Listener<TickEvent>>
    {
        virtual std::string getSystemName() const override { return "Camera Shake"; }

        float deltaTime = 0.0f;

        virtual void init() override
        {
            registerGroup<CameraShakeComponent, BaseCamera2D>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        // Called every frame:
        virtual void execute() override
        {
            if (deltaTime == 0.0f)
                return;

            for (const auto& e : viewGroup<CameraShakeComponent, BaseCamera2D>())
            {
                auto shake = e->get<CameraShakeComponent>();
                auto cam   = e->get<BaseCamera2D>();

                if (not shake->active) continue;

                shake->elapsed += deltaTime;

                if (shake->elapsed >= shake->duration)
                {
                    // done shaking
                    cam->setOffset({0.f, 0.f});
                    shake->active = false;
                    continue;
                }

                // fall‑off: stronger at start, taper off
                float t = 1.0f - (shake->elapsed / shake->duration);
                float currentMag = shake->magnitude * t;

                // generate a random offset in [-currentMag, +currentMag]
                float ox = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.f - 1.f) * currentMag;
                float oy = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.f - 1.f) * currentMag;

                cam->setOffset({ox, oy});
            }

            deltaTime = 0.0f;
        }
    };

    struct GameStart
    {

    };

    struct GameEnd
    {
        bool win = false;
    };

    struct SnapCamera
    {
        float smoothFactor = 0.1f;
    };

    // Todo bug bullet can stay stuck in a wall if fired from within the wall

    struct PlayerSystem : public System<QueuedListener<OnMouseClick>, QueuedListener<ConfiguredKeyEvent<GameKeyConfig>>, QueuedListener<ConfiguredKeyEventReleased<GameKeyConfig>>, InitSys,
        Listener<PlayerMoveUp>, Listener<PlayerMoveDown>, Listener<PlayerMoveLeft>, Listener<PlayerMoveRight>, QueuedListener<SpawnPlayerEvent>,
        QueuedListener<PlayerHitEvent>, Listener<PlayerInvincibilityEndEvent>, Listener<PlayerDodgeEndEvent>,
        Listener<TickEvent>, QueuedListener<OnMouseMove>, QueuedListener<SnapCamera>>
    {
        AsepriteFile animFile;
        PlayerSystem(const AsepriteFile& animFile) : animFile(animFile) {}

        virtual std::string getSystemName() const override { return "Player System"; }

        virtual void init() override;

        virtual void onProcessEvent(const PlayerHitEvent& event) override;

        virtual void onEvent(const PlayerInvincibilityEndEvent& event) override;

        virtual void onProcessEvent(const SpawnPlayerEvent& event) override;

        virtual void onProcessEvent(const SnapCamera& event) override;

        virtual void onEvent(const PlayerDodgeEndEvent& event) override;

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        void updateCamera();

        virtual void onProcessEvent(const OnMouseMove& event) override;

        virtual void onProcessEvent(const OnMouseClick& event) override;

        void selectedRunningAnimation();

        virtual void onProcessEvent(const ConfiguredKeyEvent<GameKeyConfig>& event) override;

        virtual void onProcessEvent(const ConfiguredKeyEventReleased<GameKeyConfig>& event) override;

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

        EntityRef cursor;

        constant::Vector2D lastCameraPos {0.f, 0.f};

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
        float maxHealth = 5.0f;

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
