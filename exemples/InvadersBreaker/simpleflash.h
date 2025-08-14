#pragma once

#include "player.h"

class FlashEffectSystem : public System<InitSys, Listener<TickEvent>, Listener<PlayerHitEvent>>
{
public:
    void init() override {
        registerGroup<FlashEffect, Simple2DObject>();
        registerGroup<Paddle, Simple2DObject>();
    }

    void onEvent(const PlayerHitEvent& event) override
    {
        // Flash the paddle white
        for (auto paddle : viewGroup<Paddle, Simple2DObject>())
        {
            if (not paddle->entity->has<FlashEffect>())
            {
                // Todo add a attach the the component set
                // auto flash = paddle.attach<FlashEffect>();

                auto flash = ecsRef->attach<FlashEffect>(paddle->entity);

                auto shape = paddle->get<Simple2DObject>();
                flash->originalColor = shape->colors;
                shape->setColors({255, 0, 0, 255});  // Flash Red
            }
        }
    }

    void onEvent(const TickEvent& event) override {
        std::vector<EntityRef> toRemove;

        for (auto entity : viewGroup<FlashEffect, Simple2DObject>()) {
            auto flash = entity->get<FlashEffect>();
            auto shape = entity->get<Simple2DObject>();

            flash->elapsed += event.tick;

            if (flash->elapsed >= flash->duration)
            {
                // Restore original color
                shape->setColors(flash->originalColor);
                toRemove.push_back(entity->entity);
            }
        }

        // Remove flash components
        for (auto& entity : toRemove)
        {
            // Todo add a detach in entity
            // entity->detach<FlashEffect>();
            ecsRef->detach<FlashEffect>(entity);
        }
    }
};