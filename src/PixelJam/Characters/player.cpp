#include "player.h"

#include "UI/ttftext.h"

namespace pg
{
    void PlayerSystem::init()
    {
        AsepriteLoader aseprite_loader;
        playerAnimation = aseprite_loader.loadAnim("res/sprites/main-char.json");

        auto playerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 255.f, 0.f, 255.f});

        playerEnt.get<PositionComponent>()->setZ(10);
        playerEnt.get<PositionComponent>()->setVisibility(false);

        ecsRef->attach<EntityName>(playerEnt.entity, "Player");
        ecsRef->attach<PlayerFlag>(playerEnt.entity);
        ecsRef->attach<FollowCamera2D>(playerEnt.entity);

        playerEnt.get<Simple2DObject>()->setViewport(1);

        std::vector<size_t> collidableLayer = {0, 3, 5, 6};

        ecsRef->attach<CollisionComponent>(playerEnt.entity, 1, 1.0, collidableLayer);

        Weapon baseWeapon;

        baseWeapon.ammo = -1;

        ecsRef->attach<WeaponComponent>(playerEnt.entity, baseWeapon);

        player = playerEnt.entity;

        auto entity2 = ecsRef->createEntity();
        auto entity3 = ecsRef->createEntity();
        auto entity4 = ecsRef->createEntity();
        auto entity5 = ecsRef->createEntity();

        upTimer = ecsRef->attach<Timer>(entity2);
        leftTimer = ecsRef->attach<Timer>(entity3);
        bottomTimer = ecsRef->attach<Timer>(entity4);
        rightTimer = ecsRef->attach<Timer>(entity5);

        upTimer->interval = 10;
        leftTimer->interval = 10;
        bottomTimer->interval = 10;
        rightTimer->interval = 10;

        upTimer->callback = makeCallable<PlayerMoveUp>();
        leftTimer->callback = makeCallable<PlayerMoveLeft>();
        bottomTimer->callback = makeCallable<PlayerMoveDown>();
        rightTimer->callback = makeCallable<PlayerMoveRight>();

        upTimer->running = false;
        leftTimer->running = false;
        bottomTimer->running = false;
        rightTimer->running = false;

        auto entity6 = ecsRef->createEntity();

        invicibilityTimer = ecsRef->attach<Timer>(entity6);
        invicibilityTimer->oneShot = true;
        invicibilityTimer->interval = 1000;
        invicibilityTimer->callback = makeCallable<PlayerInvincibilityEndEvent>();

        auto entity7 = ecsRef->createEntity();

        dodgeTimer = ecsRef->attach<Timer>(entity7);
        dodgeTimer->oneShot = true;
        dodgeTimer->interval = 1000;
        dodgeTimer->callback = makeCallable<PlayerDodgeEndEvent>();

        auto playerHealthUi = makeTTFText(ecsRef, 0, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Health: " + std::to_string(static_cast<int>(health)), 0.4);
        playerHealthUi.get<TTFText>()->setViewport(2);

        uiElements["HealthUI"] = playerHealthUi.entity;
    }

    void PlayerSystem::onEvent(const PlayerHitEvent& event)
    {
        if (invincibility)
            return;

        health -= event.damage;

        if (uiElements["HealthUI"]->has<TTFText>())
        {
            uiElements["HealthUI"]->get<TTFText>()->setText("Health: " + std::to_string(static_cast<int>(health)));
        }

        invincibility = true;
        invicibilityTimer->start();
    }

    void PlayerSystem::movePlayer(float x, float y)
    {
        auto pos = player->get<PositionComponent>();

        auto collisionSys = ecsRef->getSystem<CollisionSystem>();

        constant::Vector2D posVec = {pos->x, pos->y};
        constant::Vector2D size = {pos->width, pos->height};

        // pos->setX(pos->x + x);
        // pos->setY(pos->y + y);

        // Todo fix this
        // This is a randabouty way to do it because attach CollisionComponent is buggy
        auto applX = sweepMove(collisionSys, {pos->x, pos->y}, size, {x, 0.0f}, {0});

        if (applX.entity and applX.entity->has<WallFlag>())
        {
            pos->setX(pos->x + applX.delta.x);
            // pos->setY(pos->y + appl.delta.y);
        }
        else
        {
            pos->setX(pos->x + x);
            // pos->setY(pos->y + y);
        }

        auto applY = sweepMove(collisionSys, {pos->x, pos->y}, size, {0.0f, y}, {0});

        if (applY.entity and applY.entity->has<WallFlag>())
        {
            // pos->setX(pos->x + appl.delta.x);
            pos->setY(pos->y + applY.delta.y);
        }
        else
        {
            // pos->setX(pos->x + x);
            pos->setY(pos->y + y);
        }

        // // 1) sweep X only
        // auto applX = sweepMove(collisionSys, {pos->x, pos->y}, size, {x, 0.0f}, {0});
        // pos->setX(pos->x + applX.delta.x);
        // // pos->setY(pos->y + applX.delta.y);

        // // 2) sweep Y only (using the **new** X position!)
        // auto applY = sweepMove(collisionSys, {pos->x, pos->y}, size, {0.0f, y}, {0});
        // // pos->setX(pos->x + applY.delta.x);
        // pos->setY(pos->y + applY.delta.y);


        // LOG_INFO("Player", "Hit X: " << applX.delta.x << ", hit Y: " << applY.delta.y);

        // // optional: detect if either axis was blocked
        // if (hitX or hitY)
        // {
        //     LOG_INFO("Player", "Hit wall on " << (hitX ? "X" : "") << (hitY ? "Y" : ""));
        // }
    }
} // namespace pg
