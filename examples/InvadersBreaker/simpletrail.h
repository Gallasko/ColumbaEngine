#pragma once

#include "components.h"
#include "events.h"

#include "2D/simple2dobject.h"
#include "Systems/tween.h"

using namespace pg;

class TrailSystem : public System<InitSys, Own<Trail>, QueuedListener<TickEvent>, Listener<RemoveEntityEvent>> 
{
public:
    void init() override {
        registerGroup<Ball, Trail, PositionComponent>();
    }

    void onProcessEvent(const TickEvent& event)
    {
        for (auto entity : viewGroup<Ball, Trail, PositionComponent>())
        {
            auto trail = entity->get<Trail>();
            auto pos = entity->get<PositionComponent>();
            auto ball = entity->get<Ball>();
            
            if (ball->launched)
            {
                if (trail->hasLastPosition)
                {
                    auto segment = makeSimple2DShape(ecsRef, Shape2D::Square, 8, 8);

                    segment.get<PositionComponent>()->setX(trail->lastPosition.first);
                    segment.get<PositionComponent>()->setY(trail->lastPosition.second);
                    
                    segment.get<Simple2DObject>()->setColors({125, 125, 125, 255});

                    segment.attach<TweenComponent>(TweenComponent {
                        255.0f, // Start opacity
                        0.0f, // End opacity
                        200.0f, // Duration in milliseconds
                        [segment](const TweenValue& value) {
                            auto v = std::get<float>(value);
                            segment.get<Simple2DObject>()->setColors({v, v, v, v});
                        },
                        makeCallable<RemoveEntityEvent>(segment.entity.id)
                    });
                }

                // Add current position
                trail->hasLastPosition = true;
                trail->lastPosition = {pos->x + pos->width / 2.0f - 4, pos->y + pos->height / 2.0f - 4};  // Center
            }
        }
    }

    void onEvent(const RemoveEntityEvent& event)
    {
        ecsRef->removeEntity(event.prefabId);
    }
    
    void execute() override {}
};
