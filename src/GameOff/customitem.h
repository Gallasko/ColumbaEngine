#pragma once

#include "item.h"

namespace pg
{
    struct XpStone : public Item
    {
        XpStone(size_t nbStones = 0)
        {
            name = "XpStone";

            type = ItemType::Material;

            stacksize = -1;

            nbItems = nbStones;
        }
    };

    struct SlimeBall : public Item
    {
        SlimeBall()
        {
            name = "Slime Ball";

            type = ItemType::Material;

            stacksize = 4;
        }
    };

    struct WarriorScroll : public Item
    {
        WarriorScroll()
        {
            name = "Warrior Scroll";

            type = ItemType::SkillBook;

            stacksize = 1;

            attributes["UsableOnCharacter"] = true;
            attributes["ConsumedUponUse"] = true;
            attributes["SkillTree"] = "Warrior";
        }
    };

    struct WarriorGrimoire : public Item
    {
        WarriorGrimoire()
        {
            name = "Warrior Grimoire";

            type = ItemType::SkillBook;

            stacksize = 1;

            attributes["UsableOnCharacter"] = true;
            attributes["ConsumedUponUse"] = false;
            attributes["SkillTree"] = "Warrior";
        }
    };

};