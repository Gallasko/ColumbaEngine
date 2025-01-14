#pragma once

#include "logger.h"

#include "Systems/coresystems.h"

using namespace pg;

struct OnClickGainGold { };

struct OnGoldGain
{
    OnGoldGain(size_t gold) : gold(gold) {}

    size_t gold;
};

struct GoldSystem : public System<Listener<OnClickGainGold>, Listener<OnGoldGain>, InitSys, StoragePolicy>
{
    virtual std::string getSystemName() const override { return "Gold System"; }

    void init() override
    {
        auto sentence = makeSentence(ecsRef, 150, 20, {"0"});

        sentence.get<UiComponent>()->setZ(1);

        goldTextId = sentence.entity.id;
    }

    virtual void onEvent(const OnClickGainGold&) override
    {
        this->gold += clickPower;

        auto goldStr = Strfy() << this->gold.load();

        // Todo fix this
        // ecsRef->sendEvent(OnTextChanged{goldTextId, goldStr.getData()});
    }

    virtual void onEvent(const OnGoldGain& event) override
    {
        this->gold += event.gold;

        auto goldStr = Strfy() << this->gold.load();

        // Todo fix this
        // ecsRef->sendEvent(OnTextChanged{goldTextId, goldStr.getData()});
    }

    size_t clickPower = 1;

    std::atomic<size_t> gold {0};
    _unique_id goldTextId;
};

struct BuyFactory { };

struct FactorySystem : public System<Listener<BuyFactory>, Listener<TickEvent>, StoragePolicy>
{
    virtual std::string getSystemName() const override { return "Factory System"; }

    virtual void onEvent(const BuyFactory&) override
    {
        LOG_THIS_MEMBER("FactorySystem");

        LOG_INFO("FactorySystem", "Bought a new factory");
        nbFactory += 1;
    }

    virtual void onEvent(const TickEvent& event) override
    {
        LOG_THIS_MEMBER("FactorySystem");

        accumulatedTick += event.tick;

        while (accumulatedTick >= factoryProdDuration)
        {
            LOG_INFO("FactorySystem", "Factory produced gold");

            accumulatedTick -= factoryProdDuration;

            ecsRef->sendEvent(OnGoldGain{static_cast<size_t>(nbFactory * factoryProdValue)});
        }
    }

    size_t accumulatedTick = 0;

    size_t nbFactory = 0;
    size_t factoryProdDuration = 1000;
    size_t factoryProdValue = 1;
};