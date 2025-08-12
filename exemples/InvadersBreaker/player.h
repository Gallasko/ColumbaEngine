// BoxBouncerSystem.h
#pragma once

#include <random>

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

struct Paddle
{

};

struct PlayerSystem : public System<InitSys, Listener<TickEvent>>
{
    virtual std::string getSystemName() const override { return "Player System"; }

    virtual void init() override
    {

    }

    virtual void onEvent(const TickEvent& event)
    {

    }

    float deltaTime = 0.0f;

};