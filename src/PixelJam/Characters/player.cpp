#include "player.h"

#include "UI/ttftext.h"
#include "2D/texture.h"
#include "2D/animator2d.h"

#include "Audio/audiosystem.h"

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

    // Todo : move to aseprite loader
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
        auto cursorEnt = makeAnchoredPosition(ecsRef);

        cursorEnt.get<PositionComponent>()->setZ(10);

        auto followCam = ecsRef->attach<FollowCamera2D>(cursorEnt.entity);
        followCam->setSmoothFactor(1.f);
        ecsRef->attach<CameraShakeComponent>(cursorEnt.entity);

        cursor = cursorEnt.entity;

        // auto frame = animFile.frames[7];
        auto idleAnim = animFile["Idle_Front"];

        auto playerEnt = makeUiTexture(ecsRef, 44, 64, idleAnim[0].textureName);
        // auto playerEnt = makeSimple2DShape(ecsRef, Shape2D::Square, 50.f, 50.f, {0.f, 255.f, 0.f, 255.f});

        playerEnt.get<PositionComponent>()->setZ(10);
        playerEnt.get<PositionComponent>()->setVisibility(false);

        ecsRef->attach<EntityName>(playerEnt.entity, "Player");
        ecsRef->attach<PlayerFlag>(playerEnt.entity);

        ecsRef->attach<Texture2DAnimationComponent>(playerEnt.entity, getAnimationKeypoint(idleAnim), true, true);

        playerEnt.get<Texture2DComponent>()->setViewport(1);

        constexpr size_t spikeLayer = 12;

        std::vector<size_t> collidableLayer = {0, 3, 5, 6, spikeLayer};

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
        invicibilityTimer->interval = 350;
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

    void PlayerSystem::onProcessEvent(const PlayerHitEvent& event)
    {
        if (invincibility)
            return;

        ecsRef->sendEvent(PlaySoundEffect{"res/audio/SFX/Player_Hurt.mp3", 0});

        cursor->get<CameraShakeComponent>()->shake(150.f, 25.f);

        player->get<Texture2DAnimationComponent>()->overrideColor({255, 0, 0}, 0.4f);

        health -= event.damage;

        updateHealthUi();

        invincibility = true;
        invicibilityTimer->start();

        if (health <= 0)
        {
            ecsRef->sendEvent(GameEnd{false});
        }
    }

    void PlayerSystem::onEvent(const PlayerInvincibilityEndEvent& event)
    {
        invincibility = false;

        player->get<Texture2DAnimationComponent>()->clearOverrideColor();
    }

    void PlayerSystem::updateCamera()
    {
        auto cursorPos = cursor->get<PositionComponent>();

        if (not cursorPos)
            return;

        auto window = ecsRef->getEntity("__MainWindow");

        if (not window)
            return;

        auto playerPos = player->get<PositionComponent>();

        if (not playerPos)
            return;

        auto windowWidth = window->get<PositionComponent>()->width / 2.0f;
        auto windowHeight = window->get<PositionComponent>()->height / 2.0f;

        auto playerWidth = playerPos->width / 2.0f;
        auto playerHeight = playerPos->height / 2.0f;

        cursorPos->setX(lastCameraPos.x - windowWidth + playerPos->x + playerWidth);
        cursorPos->setY(lastCameraPos.y - windowHeight + playerPos->y + playerHeight);
    }

    void PlayerSystem::onProcessEvent(const OnMouseMove& event)
    {
        // constant::Vector2D{mousePosInGame.x - pos->x - pos->width / 2.0f, mousePosInGame.y - pos->y - pos->height / 2.0f};

        lastCameraPos.x = event.pos.x;
        lastCameraPos.y = event.pos.y;

        updateCamera();
    }

    void PlayerSystem::onProcessEvent(const OnMouseClick& event)
    {
        if (event.button == SDL_BUTTON_LEFT)
        {
            auto pos = player->get<PositionComponent>();
            auto weaponEnt = player->get<WeaponComponent>();
            auto camera = cursor->get<BaseCamera2D>();

            if (not pos or not weaponEnt or not camera)
                return;

            auto window = ecsRef->getEntity("__MainWindow");

            if (not window)
                return;

            ecsRef->sendEvent(PlaySoundEffect{"res/audio/SFX/Shot1.mp3", 0});

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

    void PlayerSystem::selectedRunningAnimation()
    {
        if (lastMoveDir.x == 1.f)
        {
            auto playingAnim = animFile["Run_Profile"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else if (lastMoveDir.x == -1.f)
        {
            auto playingAnim = animFile["Run_Profile_L"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else if (lastMoveDir == constant::Vector2D{0.f, -1.f})
        {
            auto playingAnim = animFile["Run_Back"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else if (lastMoveDir == constant::Vector2D{0.f, 1.f})
        {
            auto playingAnim = animFile["Run_Front"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else
        {
            auto playingAnim = animFile["Idle_Front"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
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

                selectedRunningAnimation();
            }
            break;
        case GameKeyConfig::MoveRight:
            if (not rightTimer->running)
            {
                lastMoveDir.x = 1.f;
                rightTimer->start();

                selectedRunningAnimation();
            }
            break;
        case GameKeyConfig::MoveUp:
            if (not upTimer->running)
            {
                lastMoveDir.y = -1.f;
                upTimer->start();

                selectedRunningAnimation();
            }
            break;
        case GameKeyConfig::MoveDown:
            if (not bottomTimer->running)
            {
                lastMoveDir.y = 1.f;
                bottomTimer->start();

                selectedRunningAnimation();
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

    void PlayerSystem::onProcessEvent(const ConfiguredKeyEventReleased<GameKeyConfig>& event)
    {
        bool movementKeyPress = false;
        switch (event.value)
        {
        case GameKeyConfig::MoveLeft:
            leftTimer->stop();
            if (rightTimer->running)
                lastMoveDir.x = 1.f;
            movementKeyPress = true;
            break;
        case GameKeyConfig::MoveRight:
            rightTimer->stop();
            if (leftTimer->running)
                lastMoveDir.x = -1.f;
            movementKeyPress = true;
            break;
        case GameKeyConfig::MoveUp:
            upTimer->stop();
            if (bottomTimer->running)
                lastMoveDir.y = 1.f;
            movementKeyPress = true;
            break;
        case GameKeyConfig::MoveDown:
            bottomTimer->stop();
            if (upTimer->running)
                lastMoveDir.y = -1.f;
            movementKeyPress = true;
            break;

        default:
            break;
        }

        bool xAxisNotRunning = leftTimer->running == false and rightTimer->running == false;
        bool yAxisNotRunning = upTimer->running == false and bottomTimer->running == false;

        if (xAxisNotRunning)
            lastMoveDir.x = 0.f;

        if (yAxisNotRunning)
            lastMoveDir.y = 0.f;

        if (movementKeyPress)
            selectedRunningAnimation();
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

        LOG_INFO("Player System", "Trying to dodge");

        player->get<PlayerFlag>()->inDodge = true;

        cursor->get<CameraShakeComponent>()->shake(55.f, 5.f);

        dodgeTimer->start();

        ecsRef->sendEvent(PlaySoundEffect{"res/audio/SFX/Dodge.mp3", 0}); // PlaySoundEffect

        // Zoom for dodging

        // auto camera = cursor->get<FollowCamera2D>();

        // camera->setViewportWidth(camera->viewportWidth * 0.95f);
        // camera->setViewportHeight(camera->viewportHeight * 0.95f);

        // Select dodge animation

        if (lastMoveDir.x == 1.f)
        {
            auto playingAnim = animFile["Dodge_Profile"];

            player->get<Texture2DAnimationComponent>()->changeAnimation(getAnimationKeypoint(playingAnim));
        }
        else if (lastMoveDir.x == -1.f)
        {
            auto playingAnim = animFile["Dodge_Profile_L"];

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

        dashElapsed = 0.0f;
        dashDir = lastMoveDir.normalized();
    }

    void PlayerSystem::onProcessEvent(const SpawnPlayerEvent& event)
    {
        health = 5.0f;
        auto weaponEnt = player->get<WeaponComponent>();

        if (weaponEnt) weaponEnt->weapon = getBaseWeapon();

        updateHealthUi();
        updateWeaponUi();

        player->get<PositionComponent>()->setX(event.x);
        player->get<PositionComponent>()->setY(event.y);

        player->get<PositionComponent>()->setVisibility(true);

        auto window = ecsRef->getEntity("__MainWindow");

        if (not window)
            return;

        auto windowWidth = window->get<PositionComponent>()->width / 2.0f;
        auto windowHeight = window->get<PositionComponent>()->height / 2.0f;

        lastCameraPos = {windowWidth, windowHeight};

        updateCamera();

        ecsRef->sendEvent(SnapCamera{0.1f});;
    }

    void PlayerSystem::onProcessEvent(const SnapCamera& event)
    {
        // updateCamera();

        cursor->get<FollowCamera2D>()->setSmoothFactor(event.smoothFactor);
    }

    void PlayerSystem::onEvent(const PlayerDodgeEndEvent& event)
    {
        player->get<PlayerFlag>()->inDodge = false;

        selectedRunningAnimation();

        // Dezoom out of dash

        // auto window = ecsRef->getEntity("__MainWindow");

        // auto windowWidth = window->get<PositionComponent>()->width;
        // auto windowHeight = window->get<PositionComponent>()->height;

        // auto camera = cursor->get<FollowCamera2D>();

        // camera->setViewportWidth(windowWidth);
        // camera->setViewportHeight(windowHeight);
    }

    void PlayerSystem::tryHeal()
    {
        if (health >= maxHealth)
            return;

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

        //std::cout << player.get<PositionComponent>()->x << " " << player.get<PositionComponent>()->y << std::endl;

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

    // Todo test
    constant::Vector2D moveWithSlide(
        CollisionSystem*    cs,
        constant::Vector2D  startPos,
        constant::Vector2D  size,
        constant::Vector2D  delta,
        const std::vector<size_t>& layers)
    {
        constexpr float EPS = 1e-3f;
        constant::Vector2D pos       = startPos;
        constant::Vector2D remaining = delta;

        // We allow up to two iterations (for hitting two walls)
        for (int iter = 0; iter < 2; ++iter)
        {
            // sweepMove must now fill res.hit, res.delta, res.normal, res.t
            auto res = sweepMove(cs, pos, size, remaining, layers);
            if (not res.hit)
            {
                // no collision: move the rest
                pos += remaining;
                break;
            }

            // 2a) move up to just before the hit
            float travelT = std::max(0.0f, res.t - EPS);
            pos += remaining * travelT;

            // 2b) subtract the traveled portion
            constant::Vector2D traveled = remaining * travelT;
            remaining = remaining - traveled;

            // 2c) remove the component into the wall
            //    remaining' = remaining - (remaining·normal) * normal
            float into = remaining.dot(res.normal);
            remaining = remaining - res.normal * into;
            // now `remaining` is tangential to the hit surface
        }

        return pos;
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

        constant::Vector2D delta = { x, y };

        // run the slide solver
        constant::Vector2D endPos = moveWithSlide(collisionSys, posVec, size, delta, {0});

        // finally set your new position
        pos->setX(endPos.x);
        pos->setY(endPos.y);

        updateCamera();
    }
} // namespace pg
