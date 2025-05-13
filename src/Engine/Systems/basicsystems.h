#pragma once

#include "logger.h"

#include "coresystems.h"

#include "UI/sentencesystem.h"

#include "2D/position.h"

namespace pg
{
    struct FpsSystem : public System<Listener<TickEvent>, InitSys, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Fps System"; }

        virtual void init() override
        {
            LOG_INFO("FPS System initialized", "FPS System initializing");

            auto mainWindowEnt = ecsRef->getEntity(ecsRef->getSystem<EntityNameSystem>()->getEntityId("__MainWindow"));

            auto ui = mainWindowEnt->get<UiComponent>();

            auto sentence = makeSentence(ecsRef, 0, 0, {"0"});
            auto s2 = makeSentence(ecsRef, 0, 0, {"0"});
            auto s3 = makeSentence(ecsRef, 0, 0, {"0"});
            auto s4 = makeSentence(ecsRef, 0, 0, {"0"});

            sentence.get<UiComponent>()->setZ(10);
            s2.get<UiComponent>()->setZ(10);
            s3.get<UiComponent>()->setZ(10);
            s4.get<UiComponent>()->setZ(10);

            sentence.get<UiComponent>()->setRightAnchor(ui->right);
            s2.get<UiComponent>()->setRightAnchor(sentence.get<UiComponent>()->right);
            s3.get<UiComponent>()->setRightAnchor(s2.get<UiComponent>()->right);
            s4.get<UiComponent>()->setRightAnchor(s3.get<UiComponent>()->right);

            s2.get<UiComponent>()->setTopAnchor(sentence.get<UiComponent>()->bottom);
            s3.get<UiComponent>()->setTopAnchor(s2.get<UiComponent>()->bottom);
            s4.get<UiComponent>()->setTopAnchor(s3.get<UiComponent>()->bottom);

            fpsText = sentence.get<SentenceText>();
            generatedText = s2.get<SentenceText>();
            executionText = s3.get<SentenceText>();
            drawCallText = s4.get<SentenceText>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            LOG_THIS_MEMBER("FactorySystem");

            accumulatedTick += event.tick;

            if (accumulatedTick >= 1000)
            {
                accumulatedTick %= 1000;

                auto rendererSys = ecsRef->getSystem<MasterRenderer>();

                if (not rendererSys)
                    return;

                auto currentNbOfFrames = rendererSys->getNbRenderedFrames();
                auto currentNbGOfFrames = rendererSys->getNbGeneratedFrames();

                // In case of overflow of size_t
                if (currentNbOfFrames < lastNbOfFrames)
                {
                    lastNbOfFrames = currentNbOfFrames;
                    return;
                }

                if (currentNbGOfFrames < lastNbOfGeneratedFrames)
                {
                    lastNbOfGeneratedFrames = currentNbGOfFrames;
                    return;
                }

                auto res = currentNbOfFrames - lastNbOfFrames;
                auto res2 = currentNbGOfFrames - lastNbOfGeneratedFrames;

                auto fpsStr = Strfy() << res;
                auto fpsStr2 = Strfy() << res2;

                lastNbOfFrames = currentNbOfFrames;
                lastNbOfGeneratedFrames = currentNbGOfFrames;

                // Print FPS
                fpsText->setText(fpsStr.getData());
                generatedText->setText(fpsStr2.getData());
                executionText->setText(std::to_string(ecsRef->getCurrentNbOfExecution()));
                drawCallText->setText(std::to_string(rendererSys->getNbRenderCall()));

                ecsRef->reportSystemProfiles();

                // rendererSys->printAllDrawCalls();
            }
        }

        CompRef<SentenceText> fpsText;
        CompRef<SentenceText> generatedText;
        CompRef<SentenceText> executionText;
        CompRef<SentenceText> drawCallText;

        size_t accumulatedTick = 0;
        size_t lastNbOfFrames = 0;
        size_t lastNbOfGeneratedFrames = 0;
    };

    struct MoveToComponent
    {
        MoveToComponent(constant::Vector2D endPos, float speed, float distanceToTravel = 0.0f, bool destroyUponMoved = true, CallablePtr callback = nullptr) : endPos(endPos), speed(speed), distanceToTravel(distanceToTravel), destroyUponMoved(destroyUponMoved), callback(callback) {}
        MoveToComponent(const MoveToComponent& other) : endPos(other.endPos), speed(other.speed), changed(other.changed), deltaPos(other.deltaPos),
            distanceTravelled(other.distanceTravelled), distanceToTravel(other.distanceToTravel), destroyUponMoved(other.destroyUponMoved),
            callback(other.callback), callbackCalled(other.callbackCalled), moving(other.moving) {}

        void setSpeed(float speed) { this->speed = speed; }
        void setEndPos(const constant::Vector2D& endPos) { moving = true; distanceTravelled = 0.0f; this->endPos = endPos; changed = true; }

        constant::Vector2D endPos;
        float speed = 1;

        bool changed = true;
        constant::Vector2D deltaPos = { 0.0f, 0.0f };
        float distanceTravelled = 0.0f;
        float distanceToTravel = 0.0f;

        bool destroyUponMoved = true;
        CallablePtr callback = nullptr;
        bool callbackCalled = false;

        bool moving = true;
    };

    struct MoveToSystem : public System<Own<MoveToComponent>, Ref<PositionComponent>, Listener<TickEvent>, InitSys>
    {
        virtual void init() override
        {
            registerGroup<PositionComponent, MoveToComponent>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void execute() override
        {
            if (not deltaTime)
                return;

            for (const auto& elem : viewGroup<PositionComponent, MoveToComponent>())
            {
                auto ui = elem->get<PositionComponent>();
                auto move = elem->get<MoveToComponent>();

                if (not move->moving)
                    continue;

                auto travelSpeed = move->speed * (deltaTime / 1000.0f);

                float x = ui->x;
                float y = ui->y;

                if (move->changed)
                {
                    // Normalize the go to vector !
                    auto diffx = move->endPos.x - x;
                    auto diffy = move->endPos.y - y;

                    auto distance = std::sqrt(diffx * diffx + diffy * diffy);

                    move->deltaPos.x = diffx / distance;
                    move->deltaPos.y = diffy / distance;

                    if (move->distanceToTravel == 0.0f)
                        move->distanceToTravel = distance;

                    move->changed = false;
                }

                move->distanceTravelled += travelSpeed;

                if (move->distanceTravelled > move->distanceToTravel)
                {
                    ui->setX(move->endPos.x);
                    ui->setY(move->endPos.y);

                    if (move->callback != nullptr and not move->callbackCalled)
                    {
                        move->callbackCalled = true;
                        move->callback->call(ecsRef);

                        continue;
                    }

                    move->moving = false;

                    if (move->destroyUponMoved)
                    {
                        ecsRef->removeEntity(elem->entity);
                    }
                }
                else
                {
                    auto newX = x + move->deltaPos.x * travelSpeed;
                    auto newY = y + move->deltaPos.y * travelSpeed;

                    ui->setX(newX);
                    ui->setY(newY);
                }
            }

            deltaTime = 0;
        }

        size_t deltaTime = 0;
    };

    struct MoveDirComponent
    {
        MoveDirComponent(constant::Vector2D dir, float speed, float maxDistance = -1.0f, bool destroyAfter = false, CallablePtr callback = nullptr)
            : direction(dir), speed(speed), maxDistance(maxDistance), destroyAfter(destroyAfter), callback(callback)
        {
            normalizeDirection();
        }

        MoveDirComponent(const MoveDirComponent& other)
            : direction(other.direction), speed(other.speed), distanceTraveled(other.distanceTraveled),
            maxDistance(other.maxDistance), destroyAfter(other.destroyAfter), callback(other.callback),
            callbackCalled(other.callbackCalled)
        {}

        void setDirection(const constant::Vector2D& dir)
        {
            direction = dir;
            normalizeDirection();
        }

        void normalizeDirection()
        {
            float mag = std::sqrt(direction.x * direction.x + direction.y * direction.y);
            if (mag > 0.0f)
            {
                direction.x /= mag;
                direction.y /= mag;
            }
        }

        constant::Vector2D direction;
        float speed = 1.0f;
        float distanceTraveled = 0.0f;
        float maxDistance = -1.0f; // if <= 0, move forever
        bool destroyAfter = false;

        CallablePtr callback = nullptr;
        bool callbackCalled = false;
    };

    struct MoveDirSystem : public System<Own<MoveDirComponent>, Ref<PositionComponent>, Listener<TickEvent>, InitSys>
    {
        virtual void init() override
        {
            registerGroup<PositionComponent, MoveDirComponent>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void execute() override
        {
            if (not deltaTime)
                return;

            float deltaSeconds = deltaTime / 1000.0f;

            for (const auto& elem : viewGroup<PositionComponent, MoveDirComponent>())
            {
                auto pos = elem->get<PositionComponent>();
                auto move = elem->get<MoveDirComponent>();

                float moveAmount = move->speed * deltaSeconds;

                float dx = move->direction.x * moveAmount;
                float dy = move->direction.y * moveAmount;

                pos->setX(pos->x + dx);
                pos->setY(pos->y + dy);

                move->distanceTraveled += std::sqrt(dx * dx + dy * dy);

                bool shouldDestroy = move->maxDistance > 0.0f && move->distanceTraveled >= move->maxDistance;

                if (shouldDestroy)
                {
                    if (move->callback && !move->callbackCalled)
                    {
                        move->callbackCalled = true;
                        move->callback->call(ecsRef);
                    }

                    if (move->destroyAfter)
                    {
                        ecsRef->removeEntity(elem->entity);
                    }
                }
            }

            deltaTime = 0;
        }

        size_t deltaTime = 0;
    };

}
