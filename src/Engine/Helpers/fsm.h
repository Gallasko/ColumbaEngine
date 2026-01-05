#pragma once

#include <unordered_map>
#include <vector>

#include "../Memory/elementtype.h"

#include "../ECS/system.h"

namespace pg
{
    struct FSMState
    {
        void onEnter() {}
        void onExit() {}

        void onEvent(const StandardEvent& event) {}

        void setEnterScript(const std::string& script);
        void setExitScript(const std::string& script);
        void setEventScript(const std::string& script);

        std::string name;

        std::string currentFSM;
        ElementMap data;

        std::vector<std::string> triggers;
    };

    struct FiniteStateMachine
    {
        void setState(const std::string& fsm, const FSMState& state)
        {
            auto& oldState = currentStates[fsm];
            oldState.onExit();

            for (const auto& trigger : oldState.triggers)
            {
                auto& vec = triggeredStates[fsm];
                vec.erase(std::remove(vec.begin(), vec.end(), oldState), vec.end());
            }

            currentStates[fsm] = state;

            for (const auto& trigger : state.triggers)
            {
                triggeredStates[fsm].push_back(state);
            }

            currentStates[fsm].onEnter();
        }

        void setState(const FSMState& state)
        {
            setState(state.currentFSM, state);
        }

        void onEvent(const StandardEvent& event)
        {
            for (auto& state : triggeredStates[event.name])
            {
                state.onEvent(event);
            }
        }

        std::unordered_map<std::string, FSMState> currentStates;

        std::unordered_map<std::string, std::vector<FSMState>> triggeredStates;
    };

    struct FSMSystem : public System<>
    {

    };
}