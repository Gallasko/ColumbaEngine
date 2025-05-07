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
    // Todo need to test if this suffice or do we need to make some special checks when mods are pressed to know when a key is not pressed anymore for the ConfiguredKeyEventReleased event


    struct DefaultScancode
    {
        DefaultScancode(const std::string& name, const SDL_Scancode& code, const Uint16& mod = KMOD_NONE) : name(name), code(code), mod(mod) {}
        DefaultScancode(const DefaultScancode& other) : name(other.name), code(other.code), mod(other.mod) {}

        DefaultScancode& operator=(const DefaultScancode& other)
        {
            name = other.name;
            code = other.code;
            mod = other.mod;

            return *this;
        }

        std::string name;
        SDL_Scancode code;
        Uint16 mod;
    };

    struct ModdedSDLScancode
    {
        ModdedSDLScancode(const SDL_Scancode& code, const Uint16& mod = KMOD_NONE) : code(code), mod(mod) {}
        ModdedSDLScancode(const ModdedSDLScancode& other) : code(other.code), mod(other.mod) {}

        ModdedSDLScancode& operator=(const ModdedSDLScancode& other)
        {
            code = other.code;
            mod = other.mod;

            return *this;
        }

        SDL_Scancode code;
        Uint16 mod;
    };

    // template <typename Type>
    struct ChangeKeyBind
    {
        ChangeKeyBind(const std::any& value, const ModdedSDLScancode& oldCode, const ModdedSDLScancode& newCode) : value(value), oldCode(oldCode), newCode(newCode) {}
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
        ModdedSDLScancode oldCode;
        ModdedSDLScancode newCode;
    };

    // Todo make this more generic and enable the fact to change the control during runtime

    template <typename Type>
    class ConfiguredKeySystem : public System<Listener<OnSDLScanCode>, Listener<OnSDLScanCodeReleased>, QueuedListener<ChangeKeyBind>, InitSys>
    {
    private:
        struct KeyState
        {
            KeyState(const SDL_Scancode& code, const Uint16& mod, bool pressed) : code(code), mod(mod), pressed(pressed) {}
            KeyState(const KeyState& other) : code(other.code), mod(other.mod), pressed(other.pressed) {}

            KeyState& operator=(const KeyState& other)
            {
                code = other.code;
                pressed = other.pressed;
                mod = other.mod;

                return *this;
            }

            SDL_Scancode code;
            Uint16 mod;
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

        virtual void onProcessEvent(const ChangeKeyBind& event) override
        {
            Type configValue = std::any_cast<Type>(event.value);

            auto it = std::find_if(scancodeToType.begin(), scancodeToType.end(), [&](auto &pair){ return pair.first.code == event.oldCode.code and pair.first.mod  == event.oldCode.mod;});

            if (it == scancodeToType.end())
            {
                LOG_ERROR("Keyconfig", "Wrong old code given !");
                return;
            }

            auto &holders = it->second;

            auto it2 = std::find_if(holders.begin(), holders.end(),[&](auto &h){ return h.code == configValue; });

            std::string name;

            if (it2 != holders.end())
            {
                name = it2->name;
                LOG_INFO("Keyconfig", "Removing name: " << name);
                holders.erase(it2);
            }
            else
            {
                LOG_ERROR("Keyconfig", "Wrong old code given !");
                return;
            }

            auto it3 = std::find_if(scancodeToType.begin(), scancodeToType.end(), [&](auto &pair){ return pair.first.code == event.newCode.code and pair.first.mod  == event.newCode.mod;});

            if (it3 == scancodeToType.end())
            {
                scancodeToType.emplace_back(ModdedSDLScancode{event.newCode.code, event.newCode.mod}, std::vector<TypeHolder>{ TypeHolder{name, configValue} });
            }
            else
                it3->second.emplace_back(name, configValue);

            ecsRef->sendEvent(SaveElementEvent{name + "_key", event.newCode.code});
            ecsRef->sendEvent(SaveElementEvent{name + "_mod", event.newCode.mod});
        }

        virtual void onEvent(const OnSDLScanCode& event) override
        {
            eventQueue.emplace(event.key, event.mod, true);
        }

        virtual void onEvent(const OnSDLScanCodeReleased& event) override
        {
            eventQueue.emplace(event.key, event.mod, false);
        }

        virtual void execute() override
        {
            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();

                // Todo need to handle special cases for special keys (the mod key like ctrl, shift, alt, etc...) because we don't trigger correctly the key event released

                for (auto& pair : scancodeToType)
                {
                    if (pair.first.code == event.code and (pair.first.mod & event.mod))
                    {
                        for (auto& holder : pair.second)
                        {
                            if (event.pressed)
                                ecsRef->sendEvent(ConfiguredKeyEvent<Type>{holder.code});
                            else
                                ecsRef->sendEvent(ConfiguredKeyEventReleased<Type>{holder.code});
                        }
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
                auto savedKey = ecsRef->getSavedData(defaultKey.second.name + "_key");
                auto savedMod = ecsRef->getSavedData(defaultKey.second.name + "_mod");

                SDL_Scancode keyCode = defaultKey.second.code;
                Uint16 mod = defaultKey.second.mod;

                LOG_INFO("Key Config", "Event named " << defaultKey.second.name << ", has key : " << keyCode << ", saved data: " << savedKey.toString() << ", has mod : " << mod << ", saved data: " << savedMod.toString());

                if (not savedKey.isEmpty() and savedKey.isNumber())
                {
                    keyCode = static_cast<SDL_Scancode>(savedKey.template get<int>());
                }

                if (not savedMod.isEmpty() and savedMod.isNumber())
                {
                    mod = static_cast<Uint16>(savedMod.template get<int>());
                }

                ecsRef->sendEvent(SaveElementEvent{defaultKey.second.name + "_key", keyCode});
                ecsRef->sendEvent(SaveElementEvent{defaultKey.second.name + "_mod", mod});

                auto it = std::find_if(scancodeToType.begin(), scancodeToType.end(), [&](auto &pair){ return pair.first.code == keyCode and pair.first.mod == mod;});

                if (it == scancodeToType.end())
                {
                    scancodeToType.emplace_back(ModdedSDLScancode{keyCode, mod}, std::vector<TypeHolder>{ TypeHolder{defaultKey.second.name, defaultKey.first} });
                }
                else
                    it->second.emplace_back(defaultKey.second.name, defaultKey.first);
            }
        }

    private:
        DefaultScancodeMap defaultMap;

        std::vector<std::pair<ModdedSDLScancode, std::vector<TypeHolder>>> scancodeToType;

        std::queue<KeyState> eventQueue;

        std::queue<ChangeKeyBind> changeEventQueue;
    };
}