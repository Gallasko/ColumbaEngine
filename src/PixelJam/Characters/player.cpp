#include "player.h"

#include "UI/ttftext.h"
#include "2D/texture.h"
#include "2D/animator2d.h"

namespace pg
{
    Weapon getBaseWeapon()
    {
        Weapon weapon;

        weapon.name = "BaseWeapon";
        weapon.damage = 1;
        weapon.ammo = -1;

        return weapon;
    }

    std::vector<Animation2DKeyPoint> getAnimationKeypoint(const std::vector<AsepriteFrame>& frames)
    {
        std::vector<Animation2DKeyPoint> keypoints;

        size_t cumulativeAnimationDuration = 0;

        for (const auto& anim : frames)
        {
            keypoints.push_back({cumulativeAnimationDuration, {anim.textureName, 1}});
        
            cumulativeAnimationDuration += anim.durationInMilliseconds;
        }

        keypoints.push_back({cumulativeAnimationDuration, {frames.back().textureName, 1}});

        return keypoints;
    }

    void PlayerSystem::init()
    {
        // auto frame = animFile.frames[7];
        auto idleAnim = animFile["Idle_Front"];

        auto playerEnt = makeUiTexture(ecsRef, 44, 64, idleAnim[0].textureName);
        // auto playerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 255.f, 0.f, 255.f});

        playerEnt.get<PositionComponent>()->setZ(10);
        playerEnt.get<PositionComponent>()->setVisibility(false);

        ecsRef->attach<EntityName>(playerEnt.entity, "Player");
        ecsRef->attach<PlayerFlag>(playerEnt.entity);
        ecsRef->attach<FollowCamera2D>(playerEnt.entity);

        ecsRef->attach<Texture2DAnimationComponent>(playerEnt.entity, getAnimationKeypoint(idleAnim), true, true);

        playerEnt.get<Texture2DComponent>()->setViewport(1);

        std::vector<size_t> collidableLayer = {0, 3, 5, 6};

        ecsRef->attach<CollisionComponent>(playerEnt.entity, 1, 1.0, collidableLayer);

        ecsRef->attach<WeaponComponent>(playerEnt.entity, getBaseWeapon());

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
        dodgeTimer->interval = dashDuration;
        dodgeTimer->callback = makeCallable<PlayerDodgeEndEvent>();

        auto playerHealthUi = makeTTFText(ecsRef, 0, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Health: " + std::to_string(static_cast<int>(health)), 0.4);
        playerHealthUi.get<TTFText>()->setViewport(2);

        uiElements["HealthUI"] = playerHealthUi.entity;

        auto playerRemainingBulletUi = makeTTFText(ecsRef, 220, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", "Ammo: infinity", 0.4);
        playerRemainingBulletUi.get<TTFText>()->setViewport(2);

        uiElements["BulletUi"] = playerRemainingBulletUi.entity;
    }

    void PlayerSystem::onEvent(const PlayerHitEvent& event)
    {
        if (invincibility)
            return;

        health -= event.damage;

        updateHealthUi();

        invincibility = true;
        invicibilityTimer->start();
    }

    void PlayerSystem::onProcessEvent(const OnMouseClick& event)
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            auto pos = player->get<PositionComponent>();
            auto weaponEnt = player->get<WeaponComponent>();
            auto camera = player->get<BaseCamera2D>();

            if (not pos or not weaponEnt or not camera)
                return;

            auto window = ecsRef->getEntity("__MainWindow");

            if (not window)
                return;

            // Todo I should only need to get the main camera and use mousePosToWorldPos of the main camera instead !
            // auto windowWidth = window->get<PositionComponent>()->width;
            // auto windowHeight = window->get<PositionComponent>()->height;

            // auto normalizedX = 2 * (event.pos.x / windowWidth) - 1.0;
            // auto normalizedY = 2 * (event.pos.y / windowHeight) - 1.0;

            auto mousePosInGame = camera->screenToWorld(event.pos.x, event.pos.y);

            auto fireDir = constant::Vector2D{mousePosInGame.x - pos->x - pos->width / 2.0f, mousePosInGame.y - pos->y - pos->height / 2.0f};

            // Raycast test
            // auto collisionSys = ecsRef->getSystem<CollisionSystem>();

            // auto ray =  collisionSys->raycast({pos->x + pos->width / 2.0f, pos->y + pos->height / 2.0f}, fireDir.normalized(), 1000, 0);

            // if (ray.hit)
            // {
            //     LOG_INFO("Player", "Ray hit entity: " << ray.entityId << " at position: " << ray.hitPoint.x << " " << ray.hitPoint.y);

            //     auto ent = ecsRef->getEntity(ray.entityId);

            //     if (ent and ent->has<PositionComponent>())
            //     {
            //         auto pos = ent->get<PositionComponent>();

            //         // pos->setVisibility(false);
            //     }
            // }

            LOG_INFO("Player","Mouse pos in game: " << mousePosInGame.x << " " << mousePosInGame.y);

            auto& weapon = weaponEnt->weapon;

            // If no ammo, automatically switch back to base weapon
            if (weapon.ammo == 0)
            {
                weaponEnt->weapon = getBaseWeapon();
            }

            for (const auto& dir : weapon.fireDirections(fireDir))
            {
                if (weapon.ammo != 0)
                {
                    auto bullet = makeSimple2DShape(ecsRef, Shape2D::Square, weapon.projectileSize, weapon.projectileSize, {125.f, 125.f, 0.f, 255.f});

                    bullet.get<Simple2DObject>()->setViewport(1);

                    bullet.get<PositionComponent>()->setX(pos->x + 25.f);
                    bullet.get<PositionComponent>()->setY(pos->y + 25.f);
                    bullet.get<PositionComponent>()->setZ(50);

                    std::vector<size_t> collidableLayer = {0, 4};

                    ecsRef->attach<CollisionComponent>(bullet.entity, 2, 1.0, collidableLayer);
                    ecsRef->attach<AllyBulletFlag>(bullet.entity);
                    ecsRef->attach<MoveDirComponent>(bullet.entity, dir, weapon.projectileSpeed, weapon.projectileLifeTime, true);

                    if (weapon.ammo != -1)
                        weapon.ammo--;
                }
                else
                {
                    LOG_ERROR("Player", "Out of ammo - Todo make a visual about this (a ttf text for exemple)");
                }
            }

            // If no ammo, automatically switch back to base weapon
            if (weapon.ammo == 0)
            {
                weaponEnt->weapon = getBaseWeapon();
            }

            printWeapon(weapon);
        }
    }

    void PlayerSystem::onProcessEvent(const ConfiguredKeyEvent<GameKeyConfig>& event)
    {
        switch (event.value)
        {
        case GameKeyConfig::MoveLeft:
            if (not leftTimer->running)
            {
                lastMoveDir.x = -1.f;
                leftTimer->start();
            }
            break;
        case GameKeyConfig::MoveRight:
            if (not rightTimer->running)
            {
                lastMoveDir.x = 1.f;
                rightTimer->start();
            }
            break;
        case GameKeyConfig::MoveUp:
            if (not upTimer->running)
            {
                lastMoveDir.y = -1.f;
                upTimer->start();
            }
            break;
        case GameKeyConfig::MoveDown:
            if (not bottomTimer->running)
            {
                lastMoveDir.y = 1.f;                
                bottomTimer->start();
            }
            break;

        case GameKeyConfig::Interact:
            tryCollect();
            break;

        case GameKeyConfig::Dodge:
            tryingToDash = true;
            break;

        case GameKeyConfig::Heal:
            tryHeal();
            break;

        default:
            break;
        }
    }

    void PlayerSystem::tryCollect()
    {
        constexpr float collectRadius = 40.f;
        constexpr float collectRadius2 = collectRadius * collectRadius;

        // get player pos
        auto pos = player->get<PositionComponent>();
        constant::Vector2D playerCenter{ pos->x + pos->width / 2.0f, pos->y + pos->height / 2.0f };

        // search all collectibles

        bool found = false;
        CollectibleFlag *collectible = nullptr;
        _unique_id cId;

        float minDist = std::numeric_limits<float>::max();

        // Find nearest collectible
        for (const auto& obj : viewGroup<CollectibleFlag, PositionComponent>())
        {
            auto cf = obj->get<CollectibleFlag>();
            auto cpos = obj->get<PositionComponent>();

            constant::Vector2D colCenter{ cpos->x + 0.5f * 25.f, cpos->y + 0.5f * 25.f };

            float dx = colCenter.x - playerCenter.x;
            float dy = colCenter.y - playerCenter.y;

            float dist2 = dx*dx + dy*dy;

            if (dist2 <= collectRadius2)
            {
                if (dist2 < minDist)
                {
                    minDist = dist2;
                    collectible = cf;
                    found = true;
                    cId = obj->entity.id;
                }
            }
        }

        if (found)
        {
            // it's in range!
            switch (collectible->type)
            {
                case CollectibleType::Gold:
                    LOG_ERROR("Player System", "Todo !");
                    break;
                case CollectibleType::Weapon:
                    ecsRef->attach<WeaponComponent>(player.entity, collectible->weapon);

                    printWeapon(collectible->weapon);
                break;
            }

            ecsRef->removeEntity(cId);  // remove collectible
        }
    }

    void PlayerSystem::tryDodge()
    {
        if (player->get<PlayerFlag>()->inDodge)
            return;

        if (lastMoveDir.x == 0.f and lastMoveDir.y == 0.f)
            return;

        player->get<PlayerFlag>()->inDodge = true;
        
        dodgeTimer->start();

        if (lastMoveDir.x == 1.f)
        {
            auto playingAnim = animFile["Dodge_Profile"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else if (lastMoveDir.x == -1.f)
        {
            auto playingAnim = animFile["Dodgle_Profile_L"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else if (lastMoveDir.y == -1.f)
        {
            auto playingAnim = animFile["Dodge_Back"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else if (lastMoveDir.y == 1.f)
        {
            auto playingAnim = animFile["Dodge_Front"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        // // Todo need mirror
        // 


        dashElapsed = 0.0f;
        dashDir = lastMoveDir.normalized();
    }

    void PlayerSystem::onEvent(const PlayerDodgeEndEvent& event)
    {
        player->get<PlayerFlag>()->inDodge = false;

        auto playingAnim = animFile["Idle_Front"];

        player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
    }

    void PlayerSystem::tryHeal()
    {
        LOG_INFO("Player", "Try healing");
        auto& weapon = player->get<WeaponComponent>()->weapon;

        if (weapon.name == "BaseWeapon")
            return;

        if (weapon.ammo == 0)
            return;

        health += 1;

        weapon = getBaseWeapon();

        updateHealthUi();
        updateWeaponUi();
    }

    void PlayerSystem::updateHealthUi()
    {
        if (uiElements["HealthUI"]->has<TTFText>())
        {
            uiElements["HealthUI"]->get<TTFText>()->setText("Health: " + std::to_string(static_cast<int>(health)));
        }
    }

    void PlayerSystem::updateWeaponUi()
    {
        const auto& weapon = player->get<WeaponComponent>()->weapon;

        printWeapon(weapon);
    }

    void PlayerSystem::printWeapon(const Weapon& weapon)
    {
        if (uiElements["BulletUi"]->has<TTFText>())
        {
            std::string ammoText = weapon.ammo == -1 ? "Infinite" : std::to_string(weapon.ammo);

            uiElements["BulletUi"]->get<TTFText>()->setText("Ammo: " + ammoText);
        }
    }

    void PlayerSystem::onEvent(const PlayerMoveUp&)
    {
        movePlayer(0.f, -1.f);
    }

    void PlayerSystem::onEvent(const PlayerMoveDown&)
    {   
        movePlayer(0.f, 1.f);
    }

    void PlayerSystem::onEvent(const PlayerMoveLeft&)
    {
        movePlayer(-1.f, 0.f);
    }

    void PlayerSystem::onEvent(const PlayerMoveRight&)
    {
        movePlayer(1.f, 0.f);
    }

    void PlayerSystem::execute()
    {
        if (tryingToDash)
        {
            tryingToDash = false;
            tryDodge();
        }

        if (deltaTime == 0.0f)
            return;

        auto delta = deltaTime;
        deltaTime = 0.0f;

        // auto playerFlag = player->get<PlayerFlag>();

        // LOG_INFO("Player", "tick");

        if (not player->get<PlayerFlag>()->inDodge)
            return;

        // float step = ((delta / 1000.0f) * dashDistance) / dashDuration;
        float step = ((delta / 1000.0f) * dashDistance);

        LOG_INFO("Player", "Dashing " << step);
        // float step = ((delta) * dashDistance) / dashDuration;
        // float step = ((delta) * dashDistance);// / dashDuration;
        movePlayer(dashDir.x * step, dashDir.y * step);

        dashElapsed += delta;
        // if (dashElapsed >= dashDuration)
        // {
        //     playerFlag->inDodge = false;
        // }
    }

    void PlayerSystem::movePlayer(float x, float y, bool scaleToMoveSpeed)
    {
        if (scaleToMoveSpeed)
        {
            x *= movespeed;
            y *= movespeed;
        }

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
