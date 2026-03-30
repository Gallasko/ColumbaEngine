#pragma once

#include <chrono>

#include "ECS/entitysystem.h"
#include "ECS/callable.h"

#include "oneventcomponent.h"

namespace pg
{
    // Todo make it modifiable with scripts/param file
    constexpr int TickRateMilliseconds = 8;

    // Todo add all the logger thing to all those systems and doc too

    struct EntityName : public Component
    {
        DEFAULT_COMPONENT_MEMBERS(EntityName)

        EntityName(const std::string& name) : name(name) {}

        inline static std::string getType() { return "EntityName"; }

        std::string name;
    };

    template <>
    void serialize(Archive& archive, const EntityName& value);

    template <>
    EntityName deserialize(const UnserializedObject& serializedString);

    struct EntityNameSystem : public System<Own<EntityName>, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Entity Name System"; }

        _unique_id getEntityId(const std::string& name) const
        {
            auto nameList = view<EntityName>();

            for (auto it = nameList.begin(); it != nameList.end(); ++it)
            {
                if ((*it)->name == name)
                {
                    return (*it)->entityId;
                }
            }

            return 0;
        }
    };

    struct TickEvent
    {
        TickEvent(float duration) : tick(duration) {}

        float tick;
    };

    /**
     * @brief Automatic delta time handling trait
     *
     * Systems that inherit from this trait will automatically:
     * - Listen to TickEvent
     * - Accumulate delta time
     * - Have access to getDeltaTime() to get time in seconds
     * - Automatically reset delta time after execute()
     *
     * The system's execute() method will be automatically generated to:
     * 1. Check if delta time > 0
     * 2. Call onExecute(deltaSeconds)
     * 3. Reset delta time to 0
     *
     * Example usage:
     * @code
     * class MySystem : public System<DeltaTime, InitSys> {
     *     void init() override { ... }
     *     void onExecute(float deltaTime) override {
     *         // deltaTime is in seconds, automatically provided
     *     }
     * };
     * @endcode
     */
    struct DeltaTime : public Listener<TickEvent>
    {
        virtual ~DeltaTime() {}

        /**
         * @brief Override this method in your system to receive automatic delta time updates
         * @param deltaTime Time elapsed since last execute, in seconds
         */
        virtual void onExecute(float deltaTime) = 0;

        void onEvent(const TickEvent& event) override
        {
            __accumulatedDeltaTime += event.tick;
        }

        // This will be called by the System's execute() through the template inheritance
        void execute()
        {
            if (__accumulatedDeltaTime > 0.0f)
            {
                float deltaSeconds = __accumulatedDeltaTime / 1000.0f;
                onExecute(deltaSeconds);
                __accumulatedDeltaTime = 0.0f;
            }
        }

        /**
         * @brief Get current accumulated delta time in seconds
         * @return Delta time in seconds
         */
        float getDeltaTime() const
        {
            return __accumulatedDeltaTime / 1000.0f;
        }

    private:
        float __accumulatedDeltaTime = 0.0f;
    };

    // Register DeltaTime trait with the ECS system
    template <typename... Comps, typename Sys>
    void registerComponents(Sys *system, ComponentRegistry *registry, const tag<DeltaTime>&, const Comps&... comps)
    {
        LOG_THIS("System");

        LOG_INFO("System", "Registering DeltaTime trait (auto delta time handling)");

        // Register as a TickEvent listener (DeltaTime inherits from Listener<TickEvent>)
        static_cast<Listener<TickEvent>*>(static_cast<DeltaTime*>(system))->setRegistry(registry);

        // Add execute handler to the execution queue to call DeltaTime::execute()
        system->_executionQueue.emplace_back([system]() {
            static_cast<DeltaTime*>(system)->execute();
        });

        // Continue registering remaining components
        registerComponents(system, registry, comps...);
    }

    struct TickingSystem : public System<>
    {

        TickingSystem(int16_t duration = TickRateMilliseconds) : tickDuration(duration), reminder(0)
        {
            LOG_THIS_MEMBER("Ticking System");

            // firstTickTime = std::chrono::high_resolution_clock::now();
            firstTickTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            secondTickTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        ~TickingSystem() { LOG_THIS_MEMBER("Ticking System"); stop(); }

        virtual std::string getSystemName() const override { return "Ticking System"; }

        inline void stop()
        {
            LOG_THIS_MEMBER("Ticking System");

            LOG_INFO("Ticking System", "Ticking system stopping ...");

            paused = false;
        }

        inline void pause()
        {
            LOG_THIS_MEMBER("Ticking System");

            paused = true;
        }

        inline void resume()
        {
            LOG_THIS_MEMBER("Ticking System");

            firstTickTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            secondTickTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            paused = false;
        }

        // Todo test this step by step to see if the reminder is correctly calculated
        virtual void execute() override
        {
            if (paused)
                return;

            bool triggered = false;

            secondTickTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            // To prevent for an overflow
            if (secondTickTime < firstTickTime)
            {
                LOG_MILE("Ticking System", "Overflow detected, reset counters");

                firstTickTime = secondTickTime;
                reminder = 0;
                return;
            }

            auto delta = secondTickTime - firstTickTime - reminder;

            while (delta >= tickDuration)
            {
                triggered = true;

                delta -= tickDuration;

                ecsRef->sendEvent(TickEvent{static_cast<float>(tickDuration)});
            }

            if (triggered)
            {
                firstTickTime = secondTickTime;

                reminder = delta;

                // This should never happend
                if (reminder < 0)
                {
                    LOG_ERROR("Ticking System", "Anormal reminder of less than 0 (" << reminder << ")");
                    reminder = 0;
                }
            }
        }

        int16_t tickDuration;

        int16_t firstTickTime, secondTickTime, reminder;
        bool paused = false;
    };

    struct Timer: public Component
    {
        DEFAULT_COMPONENT_MEMBERS(Timer)

        void start()
        {
            currentTime = 0;
            running = true;
        }

        void stop()
        {
            running = false;
        }

        size_t interval = 0;

        size_t currentTime = 0;

        bool running = false;

        bool oneShot = false;

        CallablePtr callback = nullptr;
    };

    struct TimerSystem : public System<Own<Timer>, Listener<TickEvent>>
    {
        virtual std::string getSystemName() const override { return "Timer System"; }

        virtual void onEvent(const TickEvent& event) override
        {
            LOG_THIS_MEMBER("Ticking System");

            currentIncrement += event.tick;
        }

        virtual void execute() override
        {
            // Todo here compare and exchange currentIncrement !
            if (currentIncrement == 0) return;

            auto currentIncrementLoaded = currentIncrement.exchange(0);

            const auto ecsRef = this->world();

            for (const auto& timer : view<Timer>())
            {
                if (timer->running)
                {
                    timer->currentTime += currentIncrementLoaded;

                    while (timer->currentTime >= timer->interval)
                    {
                        timer->currentTime -= timer->interval;

                        if (timer->callback)
                            timer->callback->call(ecsRef);

                        if (timer->oneShot)
                        {
                            timer->running = false;
                            break;
                        }
                    }
                }
            }

        }

        std::atomic<size_t> currentIncrement{0};
    };

    struct TextInputTriggeredEvent
    {
        TextInputTriggeredEvent(EntityRef entity) : entity(entity) {}
        TextInputTriggeredEvent(const TextInputTriggeredEvent& other) : entity(other.entity) {}
        ~TextInputTriggeredEvent() {}

        EntityRef entity;
    };

    struct OnEventComponentSystem : public System<Own<OnEventComponent>, Own<OnStandardEventComponent>, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "OnEventComponentSystem"; }
    };
}