#pragma once

#include <string>
#include <vector>

#include "Core/world.h"
#include "ui.h"

namespace arena
{
    /** What a shop offer is, for showing it: "Stat", "Weapon", "Skill", "Spell form"..., its name and a line on it. */
    struct OfferInfo
    {
        std::string kind, name, text;
    };
    OfferInfo offerInfo(const Content& content, const std::string& id);

    /** A run's spell with its level: "Fireball", "Fireball II". */
    std::string runSpellName(const Content& content, const Fighter::RunSpell& spell);

    /** A stat or spell item at a rarity, in words: "+40 max health", "+24% damage". */
    std::string itemText(const Content& content, const UpgradeDef& item, float power);

    /** The icon an offer (or anything owned) is drawn with. */
    std::string offerIcon(const Content& content, const std::string& id);

    /**
     * The shop between the waves of a run (UI_SPEC.md section 6): the offers as cards (Buy, Lock),
     * the free stat items of a level up, the build so far, Reroll and Ready. A click is the input's
     * pick (InputFrame::BUY + i...): the server (or the host) does the rest.
     */
    class ShopScreen
    {
    public:
        void draw(Ui& ui, const Content& content, const ArenaWorld& world, const Fighter& me);

        /** The level ups, before the shop: a card each for a new spell, a spell's next level, a weapon skill. */
        void drawLevelUp(Ui& ui, const Content& content, const ArenaWorld& world, const Fighter& me);
    };
}
