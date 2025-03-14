#pragma once

#include "ECS/system.h"

#include "Input/inputcomponent.h"

namespace pg
{
    template <typename Type>
    struct ConfiguredKeyEvent
    {
        ConfiguredKeyEvent(const Type& value) : value(value) {}
        ConfiguredKeyEvent(const ConfiguredKeyEvent& other) : value(other.value) {}

        ConfiguredKeyEvent& operator=(const ConfiguredKeyEvent& other)
        {
            value = other.value;

            return *this;
        }

        Type value;
    };

    template <typename Type>
    struct ConfiguredKeyEventReleased
    {
        ConfiguredKeyEventReleased(const Type& value) : value(value) {}
        ConfiguredKeyEventReleased(const ConfiguredKeyEventReleased& other) : value(other.value) {}

        ConfiguredKeyEventReleased& operator=(const ConfiguredKeyEventReleased& other)
        {
            value = other.value;

            return *this;
        }

        Type value;
    };

    // Todo enable multi scan code input (shift + or ctrl + or even ctrl + shift + )

    struct DefaultScancode
    {
        DefaultScancode(const std::string& name, const SDL_Scancode& code) : name(name), code(code) {}
        DefaultScancode(const DefaultScancode& other) : name(other.name), code(other.code) {}

        DefaultScancode& operator=(const DefaultScancode& other)
        {
            name = other.name;
            code = other.code;

            return *this;
        }

        std::string name;
        SDL_Scancode code;
    };

    // template <typename Type>
    struct ChangeKeyBind
    {
        ChangeKeyBind(const std::any& value, const SDL_Scancode& oldCode, const SDL_Scancode& newCode) : value(value), oldCode(oldCode), newCode(newCode) {}
        ChangeKeyBind(const ChangeKeyBind& other) : value(other.value), oldCode(other.oldCode), newCode(other.newCode) {}

        ChangeKeyBind& operator=(const ChangeKeyBind& other)
        {
            value = other.value;
            oldCode = other.oldCode;
            newCode = other.newCode;

            return *this;
        }

        // Todo make it a template instead of any here
        std::any value;
        SDL_Scancode oldCode;
        SDL_Scancode newCode;
    };

    // Todo make this more generic and enable the fact to change the control during runtime

    template <typename Type>
    class ConfiguredKeySystem : public System<Listener<OnSDLScanCode>, Listener<OnSDLScanCodeReleased>, Listener<ChangeKeyBind>, InitSys>
    {
    private:
        struct KeyState
        {
            KeyState(const SDL_Scancode& code, bool pressed) : code(code), pressed(pressed) {}
            KeyState(const KeyState& other) : code(other.code), pressed(other.pressed) {}

            KeyState& operator=(const KeyState& other)
            {
                code = other.code;
                pressed = other.pressed;

                return *this;
            }

            SDL_Scancode code;
            bool pressed;
        };

        struct TypeHolder
        {
            TypeHolder(const std::string& name, const Type& code) : name(name), code(code) {}
            TypeHolder(const TypeHolder& other) : name(other.name), code(other.code) {}

            TypeHolder& operator=(const TypeHolder& other)
            {
                name = other.name;
                code = other.code;

                return *this;
            }

            std::string name;
            Type code;
        };

    public:
        typedef std::map<Type, DefaultScancode> DefaultScancodeMap;

    public:
        /**
         * @brief Construct a new Configured Key System object
         * 
         * @param controlMap A map containg all the different control that need to be handled and their default scancode
         */
        ConfiguredKeySystem(const DefaultScancodeMap& controlMap) : defaultMap(controlMap)
        {

        }

        virtual void onEvent(const ChangeKeyBind& event) override
        {
            changeEventQueue.emplace(event);
        }

        virtual void onEvent(const OnSDLScanCode& event) override
        {
            eventQueue.emplace(event.key, true);
        }

        virtual void onEvent(const OnSDLScanCodeReleased& event) override
        {
            eventQueue.emplace(event.key, false);
        }

        virtual void execute() override
        {
            while (not changeEventQueue.empty())
            {
                const auto& event = changeEventQueue.front();

                Type configValue = std::any_cast<Type>(event.value);

                auto& list = scancodeToType[event.oldCode];

                auto it = std::find_if(list.begin(), list.end(), [&configValue](const TypeHolder& holder) { return holder.code == configValue; } );

                std::string name;

                if (it != list.end())
                {
                    name = it->name;
                    LOG_INFO("Keyconfig", "Removing name: " << name);
                    list.erase(it);
                }
                else
                {
                    LOG_ERROR("Keyconfig", "Wrong old code given !");
                }

                scancodeToType[event.newCode].emplace_back(name, configValue);

                ecsRef->sendEvent(SaveElementEvent{name, event.newCode});

                changeEventQueue.pop();
            }

            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();

                const auto& it = scancodeToType.find(event.code);

                if (it != scancodeToType.end())
                {
                    for (const auto& eventValue : it->second)
                    {
                        if (event.pressed)
                            ecsRef->sendEvent(ConfiguredKeyEvent<Type>{eventValue.code});
                        else
                            ecsRef->sendEvent(ConfiguredKeyEventReleased<Type>{eventValue.code});
                    }
                }

                eventQueue.pop();
            }
        }

        virtual std::string getSystemName() const override { return "Configured Key System"; }

        virtual void init() override
        {
            for (const auto& defaultKey : defaultMap)
            {
                auto savedKey = ecsRef->getSavedData(defaultKey.second.name);

                SDL_Scancode keyCode = defaultKey.second.code;

                LOG_INFO("Key Config", "Event named " << defaultKey.second.name << ", has key : " << keyCode << ", saved data: " << savedKey.toString());

                if (not savedKey.isEmpty() and savedKey.isNumber())
                {
                    keyCode = static_cast<SDL_Scancode>(savedKey.template get<int>());
                }

                ecsRef->sendEvent(SaveElementEvent{defaultKey.second.name, keyCode});

                scancodeToType[keyCode].emplace_back(defaultKey.second.name, defaultKey.first);
            }
        }

    private:
        DefaultScancodeMap defaultMap;

        std::unordered_map<SDL_Scancode, std::vector<TypeHolder>> scancodeToType;

        std::queue<KeyState> eventQueue;

        std::queue<ChangeKeyBind> changeEventQueue;
    };
}