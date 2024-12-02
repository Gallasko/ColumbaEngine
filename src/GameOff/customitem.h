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

};