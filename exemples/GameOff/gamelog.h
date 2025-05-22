#pragma once

#include "ECS/system.h"

#include "UI/sizer.h"

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

            auto listView = makeVerticalLayout(ecsRef, 10, 10, 255, 0, true);
            ecsRef->attach<EntityName>(listView.entity, "logview");
            auto logView = listView.get<VerticalLayout>();

            auto listViewAnchor = listView.get<UiAnchor>();

            listViewAnchor->setTopAnchor(windowAnchor->top);
            listViewAnchor->setTopMargin(35);
            listViewAnchor->setRightAnchor(windowAnchor->right);
            // listViewAnchor->setBottomAnchor(windowAnchor->bottom);
            // listViewAnchor->setBottomMargin(35);

            listViewAnchor->setHeightConstrain(PosConstrain{windowEnt->id, AnchorType::Height, PosOpType::Mul, 0.4f});

            logView->stickToEnd = true;

            auto test = makeUiSimple2DShape(ecsRef, Shape2D::Square, 70, 70, {0.f, 192.f, 0.f, 255.f});

            logView->addEntity(test.entity);

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

                auto text = log.get<TTFText>();
                auto anchor = log.get<UiAnchor>();

                text->wrap = true;

                auto logView = listViewEnt.get<VerticalLayout>();

                anchor->setWidthConstrain(PosConstrain{listViewEnt->id, AnchorType::Width, PosOpType::Sub, 30.f});

                logView->addEntity(log.entity);

                eventQueue.pop();
            }
        }

        EntityRef listViewEnt;
        std::queue<std::string> eventQueue;
    };
}