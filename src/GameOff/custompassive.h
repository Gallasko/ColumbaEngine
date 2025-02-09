#pragma once

#include "passives.h"
#include "character.h"

namespace pg
{
    struct BurnPassive : public Passive
    {
        BurnPassive()
        {
            name = "Burn";

            applyOnCharacter = [](Character& chara, const ElementMap& args, EntitySystem *ecsRef) {
                LOG_INFO("Custom Passive", "Player " << chara.name << " took some burn damage !");

                const auto& it = args.find("burnDmg");

                if (it != args.end())
                {
                    chara.receiveDmg(it->second.get<int>(), ecsRef);
                }
            };
        }
    };
}