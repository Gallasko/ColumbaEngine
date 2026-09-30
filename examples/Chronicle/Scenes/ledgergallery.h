#pragma once

#include <string>
#include <vector>

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: three resource ledgers from res/chronicle/ui/ledgergallery.yaml. The Life
    // screen's (in its panel) and the 480 wide one follow WorldFacts: resources.coin.value,
    // resources.rations.value, resources.timber.value (the first one adds the Timber row to
    // STORES) and resources.relic.sunstone.held (false removes the relic's row). The values
    // are the scene's strings; the ledger only draws them. "Earn 100 coin", "Eat (−6
    // rations)", "Find timber", "Lose the relic" and "Reset" write those facts. T toggles theme.
    struct LedgerGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        void writeAll();

        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        std::vector<pg::_unique_id> ledgers;     // The two fed ledgers

        int coin = 412;
        int rations = 18;
        int timber = 0;                // 0 = never found: no row
        bool relicHeld = true;

        Label echo;
    };
}
