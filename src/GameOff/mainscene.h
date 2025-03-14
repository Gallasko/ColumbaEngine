#pragma once

#include "Scene/scenemanager.h"

namespace pg
{
    struct MainScene : public Scene
    {
        virtual void init() override;
    };
}