#pragma once

#include "passives.h"
#include "character.h"

namespace pg
{
    struct BurnPassive : public Passive
    {
        BurnPassive()
        {
            info.name = "Burn";

            info.type = PassiveType::CharacterEffect;

            info.trigger = TriggerType::TurnStart;

            applyOnCharacter = [](Character& chara, const ElementMap& args) {
                const auto& it = std::find(args.begin(), args.end(), "burnDmg");

                if (it != args.end())
                {
                    chara.receiveDmg(it->second.get<int>());
                }
            };
        }
    };
}