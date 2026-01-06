#pragma once

#include <unordered_map>
#include <vector>

#include "../Memory/elementtype.h"

#include "../ECS/system.h"
#include "../ECS/uniqueid.h"

namespace pg
{
    struct FSMState;
    struct FiniteStateMachine;

    typedef std::function<void(const StandardEvent&, FSMState&)> FSMEventCallback;
    typedef std::function<void(FSMState&)> FSMEntryExitCallback;

    struct FSMState
    {
        FSMState() = default;

        FSMState(const std::string& stateName): name(stateName) {}

        void onEnter() { if (enterCallback) enterCallback(*this); }
        void onExit() { if (exitCallback) exitCallback(*this); }

        void onEvent(const StandardEvent& event) { eventCallbacks[event.name](event, *this); }

        void setEnterScript(const std::string& script);
        void setExitScript(const std::string& script);
        void setEventScript(const std::string& script);

        void setEnterCallback(FSMEntryExitCallback callback)
        {
            enterCallback = callback;
        }

        void setExitCallback(FSMEntryExitCallback callback)
        {
            exitCallback = callback;
        }

        void setEventCallback(const std::string& eventName, FSMEventCallback callback)
        {
            eventCallbacks[eventName] = callback;
        }

        inline FiniteStateMachine * getFSM() const { return _FSM; }

        std::string name;

        std::string currentFSM;
        ElementMap data;

        FiniteStateMachine* _FSM = nullptr;

        FSMEntryExitCallback enterCallback = nullptr;
        FSMEntryExitCallback exitCallback = nullptr;

        std::unordered_map<std::string, FSMEventCallback> eventCallbacks;
    };

    struct AddListenerToEvent
    {
        _unique_id id;
        std::string eventName;
    };

    struct FiniteStateMachine : public Component, public Dtor
    {
        DEFAULT_COMPONENT_MEMBERS(FiniteStateMachine)

        virtual void onDeletion(EntityRef entity) override;

        void registerState(const FSMState& state);

        void setState(const std::string& fsm, const std::string& stateName)
        {
            auto it = staticStates.find(stateName);
            if (it != staticStates.end())
            {
                setState(fsm, it->second);
            }
            else
            {
                LOG_ERROR("FiniteStateMachine", "State '" << stateName << "' not found in FSM '" << fsm << "'");
            }
        }

        void setState(const std::string& fsm, const FSMState& state);

        void setState(const FSMState& state)
        {
            setState(state.currentFSM, state);
        }

        void onEvent(const StandardEvent& event)
        {
            for (auto& [fsm, state] : currentStates)
            {
                if (state.eventCallbacks.count(event.name) > 0)
                    state.onEvent(event);
            }
        }

        std::unordered_map<std::string, FSMState> staticStates;

        std::unordered_map<std::string, FSMState> currentStates;

        std::set<std::string> triggers;
    };

    struct FSMSystem : public System<Own<FiniteStateMachine>, Listener<AddListenerToEvent>, StoragePolicy>
    {
        virtual void onEvent(const AddListenerToEvent& event) override;
    };
}