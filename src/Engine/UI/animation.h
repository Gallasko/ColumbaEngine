#pragma once

#include <vector>

#include "ECS/system.h"
#include "2D/position.h"

#include "Systems/coresystems.h"

namespace pg
{
    struct Position
    {
        Position() : x(0.0f), y(0.0f), z(0.0f) {}
        Position(float x, float y, float z) : x(x), y(y), z(z) {}
        Position(const Position& pos) : x(pos.x), y(pos.y), z(pos.z) {}

        Position& operator=(const Position& pos)
        {
            x = pos.x;
            y = pos.y;
            z = pos.z;
            return *this;
        }

        float x, y, z;
    };

    struct PositionKeyPoint
    {
        PositionKeyPoint() : offset(0.0f, 0.0f, 0.0f), time(0) {}
        PositionKeyPoint(const Position& offset, unsigned int time)
            : offset(offset), time(time) {}

        Position offset;
        unsigned int time;
    };

    struct PositionAnimationSequence
    {
        PositionAnimationSequence(const Position& origin = Position(0.0f, 0.0f, 0.0f)) : origin(origin), duration(0) {}

        void addKeyPoint(const PositionKeyPoint& keyPoint)
        {
            keyPoints.push_back(keyPoint);

            if (keyPoint.time > duration)
                duration = keyPoint.time;
        }

        // Returns a linearly interpolated offset.
        Position getInterpolatedOffset(unsigned int elapsedTime) const
        {
            if (keyPoints.empty())
                return Position();

            unsigned int currentIndex = 0;

            while (currentIndex < keyPoints.size() - 1 and keyPoints[currentIndex + 1].time <= elapsedTime)
                currentIndex++;

            if (currentIndex == keyPoints.size() - 1)
                return keyPoints[currentIndex].offset;

            const PositionKeyPoint& currentKey = keyPoints[currentIndex];
            const PositionKeyPoint& nextKey = keyPoints[currentIndex+1];

            float delta = float(elapsedTime - currentKey.time) / float(nextKey.time - currentKey.time);

            Position interp;
            interp.x = currentKey.offset.x + (nextKey.offset.x - currentKey.offset.x) * delta;
            interp.y = currentKey.offset.y + (nextKey.offset.y - currentKey.offset.y) * delta;
            interp.z = currentKey.offset.z + (nextKey.offset.z - currentKey.offset.z) * delta;

            return interp;
        }

        Position origin;
        std::vector<PositionKeyPoint> keyPoints;
        unsigned int duration;
    };

    // A plain data component that stores animation info.
    struct AnimationPositionComponent
    {
        AnimationPositionComponent() : elapsedTime(0), looping(false) {}

        // The animation sequence.
        PositionAnimationSequence sequence;

        // Elapsed time in the animation.
        unsigned int elapsedTime;

        // Flag indicating if we should the loop the animation
        bool looping;

        // Flag indicating if animation is running
        bool running = false;
    };

    struct AnimationPositionSystem : public System<Own<AnimationPositionComponent>, Listener<TickEvent>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Animation Position System"; }

        virtual void init() override
        {
            registerGroup<PositionComponent, AnimationPositionComponent>();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            // For every tick, update all animations.
            for (auto animGroup : viewGroup<PositionComponent, AnimationPositionComponent>())
            {
                auto pos = animGroup->get<PositionComponent>();
                auto anim = animGroup->get<AnimationPositionComponent>();

                if (not anim->running)
                    continue;

                anim->elapsedTime += event.tick;

                if (anim->elapsedTime > anim->sequence.duration)
                {
                    if (anim->looping)
                        anim->elapsedTime %= anim->sequence.duration;
                    else
                    {
                        anim->running = false;
                        continue;
                    }
                }

                Position offset = anim->sequence.getInterpolatedOffset(anim->elapsedTime);
                pos->setX(anim->sequence.origin.x + offset.x);
                pos->setY(anim->sequence.origin.y + offset.y);
                pos->setZ(anim->sequence.origin.z + offset.z);
            }
        }
    };
}
