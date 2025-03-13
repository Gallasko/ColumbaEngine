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

    struct GameLog : System<Listener<PrintGameLog>, Listener<StandardEvent>, InitSys>
    {
        virtual void init() override
        {
            addListenerToStandardEvent("gamelog");

            auto windowEnt = ecsRef->getEntity("__MainWindow");

            auto windowAnchor = windowEnt->get<UiAnchor>();

            auto listView = makeListView(ecsRef, 10, 10, 225, 0);
            logView = listView.get<ListView>();

            auto listViewAnchor = listView.get<UiAnchor>();

            listViewAnchor->setTopAnchor(windowAnchor->top);
            listViewAnchor->setTopMargin(35);
            listViewAnchor->setRightAnchor(windowAnchor->right);
            listViewAnchor->setBottomAnchor(windowAnchor->bottom);
            listViewAnchor->setBottomMargin(35);

            logView->stickToBottom = true;

            logView->spacing = 5;
        }

        virtual void onEvent(const PrintGameLog& event) override
        {
            eventQueue.push(event.message);
        }

        virtual void onEvent(const StandardEvent& event) override
        {
            auto message = event.values.at("message").get<std::string>();
            LOG_INFO("Gamelog", "Message received! " << message);
            eventQueue.push(message);
        }

        virtual void execute() override
        {
            while (not eventQueue.empty())
            {
                const auto& message = eventQueue.front();

                auto log = makeTTFText(ecsRef, 0, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", message, 0.4);

                auto ui = log.get<PositionComponent>();
                auto text = log.get<TTFText>();

                text->wrap = true;
        
                ui->setVisibility(false);
        
                logView->addEntity(log.entity);

                eventQueue.pop();
            }
        }

        CompRef<ListView> logView;
        std::queue<std::string> eventQueue;
    };
}