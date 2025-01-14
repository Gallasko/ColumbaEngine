#pragma once

#include "logger.h"

#include "coresystems.h"

namespace pg
{
    struct FpsSystem : public System<Listener<TickEvent>, InitSys, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Fps System"; }

        virtual void init() override
        {
            LOG_INFO("FPS System initialized", "FPS System initializing");

            auto sentence = makeSentence(ecsRef, 0, 0, {"0"});
            auto s2 = makeSentence(ecsRef, 0, 0, {"0"});
            auto s3 = makeSentence(ecsRef, 0, 0, {"0"});

            sentence.get<UiComponent>()->setZ(2);
            s2.get<UiComponent>()->setZ(2);
            s3.get<UiComponent>()->setZ(2);

            s2.get<UiComponent>()->setTopAnchor(sentence.get<UiComponent>()->bottom);
            s3.get<UiComponent>()->setTopAnchor(s2.get<UiComponent>()->bottom);

            fpsText = sentence.get<SentenceText>();
            generatedText = s2.get<SentenceText>();
            executionText = s3.get<SentenceText>();
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
            }
        }

        CompRef<SentenceText> fpsText;
        CompRef<SentenceText> generatedText;
        CompRef<SentenceText> executionText;
        
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

    struct MoveToSystem : public System<Own<MoveToComponent>, Listener<TickEvent>, InitSys>
    {
        virtual void init() override
        {
            registerGroup<UiComponent, MoveToComponent>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void execute() override
        {
            if (not deltaTime)
                return;

            for (const auto& elem : viewGroup<UiComponent, MoveToComponent>())
            {
                auto ui = elem->get<UiComponent>();
                auto move = elem->get<MoveToComponent>();

                if (not move->moving)
                    continue;

                auto travelSpeed = move->speed * (deltaTime / 1000.0f);

                float x = ui->pos.x;
                float y = ui->pos.y;

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

}
