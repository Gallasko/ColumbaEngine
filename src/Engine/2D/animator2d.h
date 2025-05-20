#pragma once

#include "texture.h"

#include "Systems/coresystems.h"

namespace pg
{
    struct Animation2DKeyPoint
    {
        Animation2DKeyPoint(size_t timestamp, const Texture2DComponent& comp, CallablePtr callback = nullptr) : timestamp(timestamp), component(comp), callback(callback) { }
        Animation2DKeyPoint(const Animation2DKeyPoint& other) : timestamp(other.timestamp), component(other.component), callback(other.callback) { }

        Animation2DKeyPoint& operator=(const Animation2DKeyPoint& other)
        {
            timestamp = other.timestamp;
            component = other.component;

            return *this;
        }

        size_t timestamp;

        Texture2DComponent component;

        CallablePtr callback = nullptr;
    };

    struct ChangeTexture2DAnimationEvent
    {
        _unique_id id = 0;
        std::vector<Animation2DKeyPoint> keypoints;
    };

    struct OverrideTexture2DAnimationEvent
    {
        _unique_id id = 0;
    };

    struct Texture2DAnimationComponent : public Ctor
    {
        Texture2DAnimationComponent(const std::vector<Animation2DKeyPoint>& keypoints, bool runningOnStartup = true, bool loop = false) : running(runningOnStartup), keypoints(keypoints), looping(loop) {}
        Texture2DAnimationComponent(const Texture2DAnimationComponent& other) : id(other.id), ecsRef(other.ecsRef), running(other.running), elapsedTime(other.elapsedTime), startId(other.startId), keypoints(other.keypoints), looping(other.looping) {}

        void start() { startId = -1; elapsedTime = 0; running = true; }
        void stop() { running = false; }

        virtual void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void changeAnimation(const std::vector<Animation2DKeyPoint>& keypoints)
        {
            ecsRef->sendEvent(ChangeTexture2DAnimationEvent{id, keypoints});
        }

        _unique_id id = 0;
        EntitySystem *ecsRef = nullptr;

        bool running = true;

        size_t elapsedTime = 0;
        int startId = -1;

        std::vector<Animation2DKeyPoint> keypoints;

        // Todo need to make some guard to avoid sending the event if the comp value didn't change
        void overrideViewport(size_t index)
        { 
            overrideViewportFlag = true;
            overrideViewportIndex = index;

            ecsRef->sendEvent(OverrideTexture2DAnimationEvent{id});
        }

        void clearOverrideViewport()
        { 
            overrideViewportFlag = false;

            ecsRef->sendEvent(OverrideTexture2DAnimationEvent{id});
        }

        void overrideColor(const constant::Vector3D& color, float ratio = 1.0f)
        {
            overrideColorFlag = true;
            overrideColorValue = color;
            colorRatio = ratio;

            ecsRef->sendEvent(OverrideTexture2DAnimationEvent{id});
        }

        void clearOverrideColor()
        {
            overrideColorFlag = false;

            ecsRef->sendEvent(OverrideTexture2DAnimationEvent{id});
        }

        void overrideOpacity(float opacity)
        {
            overrideOpacityFlag = true;
            overrideOpacityValue = opacity;

            ecsRef->sendEvent(OverrideTexture2DAnimationEvent{id});
        }

        void clearOverrideOpacity()
        {
            overrideOpacityFlag = false;

            ecsRef->sendEvent(OverrideTexture2DAnimationEvent{id});
        }

        bool overrideViewportFlag = false;
        size_t overrideViewportIndex = 0;

        bool overrideColorFlag = false;
        constant::Vector3D overrideColorValue;
        float colorRatio = 1.0f;

        bool overrideOpacityFlag = false;
        float overrideOpacityValue = 1.0f;

        bool looping = false;
    };

    struct Texture2DAnimatorSystem : public System<Own<Texture2DAnimationComponent>, Listener<TickEvent>, QueuedListener<ChangeTexture2DAnimationEvent>, QueuedListener<OverrideTexture2DAnimationEvent>, InitSys>
    {
        virtual void init() override
        {
            registerGroup<Texture2DComponent, Texture2DAnimationComponent>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void onProcessEvent(const ChangeTexture2DAnimationEvent& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent or not ent->has<Texture2DAnimationComponent>())
                return;

            auto anim = ent->get<Texture2DAnimationComponent>();
            
            anim->keypoints = event.keypoints;
            anim->startId = -1;
            anim->elapsedTime = 0;
        }

        virtual void onProcessEvent(const OverrideTexture2DAnimationEvent& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent or not ent->has<Texture2DAnimationComponent>() or not ent->has<Texture2DComponent>())
                return;

            auto anim = ent->get<Texture2DAnimationComponent>();
            auto tex = ent->get<Texture2DComponent>();

            if (anim->overrideViewportFlag)
                tex->setViewport(anim->overrideViewportIndex);

            if (anim->overrideColorFlag)
                tex->setOverlappingColor(anim->overrideColorValue, anim->colorRatio);

            if (anim->overrideOpacityFlag)
                tex->setOpacity(anim->overrideOpacityValue);
        }

        virtual void execute() override
        {
            if (not deltaTime)
                return;

            for (const auto& elem : viewGroup<Texture2DComponent, Texture2DAnimationComponent>())
            {
                auto anim = elem->get<Texture2DAnimationComponent>();

                if (not anim->running)
                    continue;

                if (static_cast<size_t>(anim->startId + 1) >= anim->keypoints.size())
                {
                    if (anim->looping and anim->keypoints.size() > 0)
                    {
                        anim->startId = -1;
                        anim->elapsedTime = 0;
                    }
                    else
                    {
                        anim->running = false;
                        continue;
                    }
                }

                anim->elapsedTime += deltaTime;

                if (anim->elapsedTime >= anim->keypoints.at(anim->startId + 1).timestamp)
                {
                    anim->startId++;
                    auto tex = elem->get<Texture2DComponent>();

                    // *tex = anim->keypoints.at(anim->startId).component;
                    *tex.component = anim->keypoints.at(anim->startId).component;
                    // tex->setTexture(anim->keypoints.at(anim->startId).component.textureName);

                    if (anim->overrideViewportFlag)
                        tex.component->setViewport(anim->overrideViewportIndex);

                    if (anim->overrideColorFlag)
                        tex.component->setOverlappingColor(anim->overrideColorValue, anim->colorRatio);

                    if (anim->overrideOpacityFlag)
                        tex.component->setOpacity(anim->overrideOpacityValue);

                    if (anim->keypoints.at(anim->startId).callback)
                        anim->keypoints.at(anim->startId).callback->call(ecsRef);
                }
            }

            deltaTime = 0;
        }

        size_t deltaTime = 0;
    };

}