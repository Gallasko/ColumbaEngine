#pragma once

#include "ECS/system.h"

#include "UI/listview.h"

#include "UI/ttftext.h"

namespace pg
{
    struct PrintGameLog
    {
        std::string message;
    };

    struct GameLog : System<Listener<PrintGameLog>, InitSys>
    {
        virtual void init() override
        {
            auto listView = makeListView(ecsRef, 10, 10, 100, 400);
            logView = listView.get<ListView>();

            logView->stickToBottom = true;

            logView->spacing = 5;
        }

        virtual void onEvent(const PrintGameLog& event) override
        {
            eventQueue.push(event);
        }

        virtual void execute() override
        {
            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();

                eventQueue.pop();
            }
        }

        CompRef<ListView> logView;
        std::queue<PrintGameLog> eventQueue;
    };
}