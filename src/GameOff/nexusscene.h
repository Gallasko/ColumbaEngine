#pragma once

#include "Scene/scenemanager.h"

namespace pg
{
    struct NexusScene : public Scene
    {
        virtual void init() override;

        virtual void startUp() override;

        virtual void execute() override;
    };
}