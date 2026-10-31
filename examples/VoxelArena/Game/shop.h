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

    /** A stat or tag item at a rarity, in words: "+40 max health", "Fire: +24% damage". */
    std::string itemText(const Content& content, const UpgradeDef& item, float power);

    /** A spell or technique given in a run, with its level ("Firebolt III"); the name of the branch it took. */
    std::string runSpellName(const Content& content, const Fighter::RunSpell& spell);
    std::string branchName(const Content& content, const Fighter::RunSpell& spell);

    /** What a tag item's boost reaches ("Projectiles"), and its boost in words ("+1 projectile, +25% projectile speed"). */
    std::string tagLabel(const std::string& tag);
    std::string boostText(const SpellTweak& boost, float k = 1.0f);

    /** A number that changes from one fighter to another ("Firebolt damage: 12.0 -> 14.4"): what a tooltip lists. */
    struct Change
    {
        std::string what, before, after;
        bool better = true;           // a cost or a cooldown going up is not
    };
    std::vector<Change> whatChanges(const Content& content, const ArenaWorld& world, const Fighter& before, const Fighter& after);

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
        void drawPresent(Ui& ui, const Content& content, const ArenaWorld& world, const Fighter& me);
    };
}
