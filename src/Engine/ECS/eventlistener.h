#pragma once

#include "componentregistry.h"

#include "logger.h"

#include <queue>

namespace pg
{
    template<typename Event>
    struct Listener
    {
        virtual ~Listener() {}

        virtual void onEvent(const Event& event) = 0;

        void setRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            registry->addEventListener<Event>(this);
        }

        void unsetRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            registry->removeEventListener<Event>(this);
        }
    };

    template<>
    struct Listener<StandardEvent>
    {
        virtual ~Listener() {}

        virtual void onEvent(const StandardEvent& event) = 0;

        void addListenerToStandardEvent(const std::string& name)
        {
            LOG_THIS_MEMBER("Listener");

            _stardardEventListenerList.push_back(name);

            __stardardEventRegistry->addStandardEventListener(name, this);
        }

        void setRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            LOG_INFO("Listener", "Add standard listener");

            __stardardEventRegistry = registry;
        }

        void unsetRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            for (auto eventName : _stardardEventListenerList)
            {
                registry->removeStandardEventListener(eventName, this);
            }
        }

        std::vector<std::string> _stardardEventListenerList;

        ComponentRegistry* __stardardEventRegistry;
    };

    // Todo : Right now you cannot make QueuedListener and Listener of the same event cohabit, maybe fix this in the future
    template<typename Event>
    struct QueuedListener
    {
        virtual ~QueuedListener() {}

        virtual void onProcessEvent(const Event& event) = 0;

        void onEvent(const Event& event)
        {
            LOG_THIS_MEMBER("QueuedListener");

            _eventQueue.push(event);
        }

        void setRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            registry->addEventListener<Event>(this);
        }

        void unsetRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            registry->removeEventListener<Event>(this);
        }

        std::queue<Event> _eventQueue;
    };

    template<>
    struct QueuedListener<StandardEvent>
    {
        virtual ~QueuedListener() {}

        virtual void onProcessEvent(const StandardEvent& event) = 0;

        void onEvent(const StandardEvent& event)
        {
            LOG_THIS_MEMBER("QueuedListener");

            _eventQueue.push(event);
        }

        void addListenerToStandardEvent(const std::string& name)
        {
            LOG_THIS_MEMBER("Listener");

            _stardardEventListenerList.push_back(name);

            __stardardEventRegistry->addStandardEventListener(name, this);
        }

        void setRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            LOG_INFO("Listener", "Add standard listener");

            __stardardEventRegistry = registry;
        }

        void unsetRegistry(ComponentRegistry* registry)
        {
            LOG_THIS_MEMBER("Listener");

            for (auto eventName : _stardardEventListenerList)
            {
                registry->removeStandardEventListener(eventName, this);
            }
        }

        std::vector<std::string> _stardardEventListenerList;

        ComponentRegistry* __stardardEventRegistry;

        std::queue<StandardEvent> _eventQueue;
    };
}