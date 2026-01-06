#include "fsm.h"

#include "../ECS/entitysystem.h"

namespace pg
{
    void FSMState::setEnterScript(const std::string&)
    {
        // enterScript = script;
    }

    void FSMState::setExitScript(const std::string&)
    {
        // exitScript = script;
    }

    void FSMState::setEventScript(const std::string&)
    {
        // eventScript = script;
    }

    void FiniteStateMachine::onDeletion(EntityRef)
    {
        for (const auto& trigger : triggers)
        {
            ecsRef->getComponentRegistry()->removeStandardEventListener(trigger, this);
        }
    }

    void FiniteStateMachine::registerState(const FSMState& state)
    {
        auto& newState = staticStates[state.name] = state;

        newState._FSM = this;

        for (const auto& [trigger, _] : state.eventCallbacks)
        {
            auto inserted = triggers.insert(trigger);

            if (inserted.second)
            {
                ecsRef->sendEvent(AddListenerToEvent{entityId, trigger});
            }
        }
    }

    void FiniteStateMachine::setState(const std::string& fsm, const FSMState& state)
    {
        LOG_INFO("FiniteStateMachine", "Setting state '" << state.name << "' for FSM '" << fsm << "' on entity " << entityId);

        auto& oldState = currentStates[fsm];

        oldState.onExit();

        // for (const auto& trigger : oldState.triggers)
        // {
        //     auto& vec = triggeredStates[trigger];
        //     vec.erase(std::remove(vec.begin(), vec.end(), oldState), vec.end());

        //     if (vec.empty())
        //     {
        //         triggeredStates.erase(trigger);
        //         ecsRef->sendEvent(RemoveListenerFromEvent{entityId, trigger});
        //     }
        // }

        auto& newState = currentStates[fsm] = state;

        newState.currentFSM = fsm;

        for (const auto& [trigger, _] : state.eventCallbacks)
        {
            auto inserted = triggers.insert(trigger);

            if (inserted.second)
            {
                ecsRef->sendEvent(AddListenerToEvent{entityId, trigger});
            }
        }

        newState.onEnter();
    }

    void FSMSystem::onEvent(const AddListenerToEvent& event)
    {
        LOG_THIS_MEMBER("FSMSystem");

        auto entity = world()->getEntity(event.id);

        if (entity && entity->has<FiniteStateMachine>())
        {
            auto fsm = entity->get<FiniteStateMachine>();

            world()->getComponentRegistry()->addStandardEventListener(event.eventName, fsm.component);
        }
    }
}