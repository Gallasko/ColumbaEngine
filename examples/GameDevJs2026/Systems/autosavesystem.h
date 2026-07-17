#pragma once

#include "ECS/system.h"
#include "Systems/coresystems.h"

struct AutoSaveSystem : public pg::System<pg::Listener<pg::TickEvent>>
{
    // Default: 3 minutes
    static constexpr size_t DEFAULT_INTERVAL_MS = 3 * 60 * 1000;

    explicit AutoSaveSystem(size_t intervalMs = DEFAULT_INTERVAL_MS)
        : intervalMs(intervalMs) {}

    virtual std::string getSystemName() const override { return "Auto Save"; }

    virtual void onEvent(const pg::TickEvent& event) override
    {
        accumulator += static_cast<size_t>(event.tick);
    }

    virtual void execute() override
    {
        if (accumulator >= intervalMs)
        {
            accumulator -= intervalMs;
            this->world()->forceSaveNow();
        }
    }

private:
    size_t intervalMs;
    size_t accumulator = 0;
};
