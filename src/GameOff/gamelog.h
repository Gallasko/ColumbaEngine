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

            auto listView = makeListView(ecsRef, 10, 10, 255, 0);
            auto logView = listView.get<ListView>();

            auto listViewAnchor = listView.get<UiAnchor>();

            listViewAnchor->setTopAnchor(windowAnchor->top);
            listViewAnchor->setTopMargin(35);
            listViewAnchor->setRightAnchor(windowAnchor->right);
            listViewAnchor->setBottomAnchor(windowAnchor->bottom);
            listViewAnchor->setBottomMargin(35);

            logView->stickToBottom = true;

            logView->spacing = 5;

            listViewEnt = listView.entity;
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
                auto anchor = log.get<UiAnchor>();

                text->wrap = true;
        
                ui->setVisibility(false);

                auto logView = listViewEnt.get<ListView>();
                auto logAnchor = listViewEnt.get<UiAnchor>();

                anchor->setLeftAnchor(logAnchor->left);
                anchor->setLeftMargin(15);
                anchor->setRightAnchor(logAnchor->right);
                anchor->setRightMargin(15);
        
                logView->addEntity(log.entity);

                eventQueue.pop();
            }
        }

        EntityRef listViewEnt;
        std::queue<std::string> eventQueue;
    };
}