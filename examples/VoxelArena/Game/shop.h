#pragma once

#include <string>
#include <vector>

#include "Core/world.h"
#include "hud.h"

namespace arena
{
    /** What a shop offer is, for showing it: "Stat", "Weapon", "Skill", "Spell form"..., its name and a line on it. */
    struct OfferInfo
    {
        std::string kind, name, text;
    };
    OfferInfo offerInfo(const Content& content, const std::string& id);

    /**
     * The shop between the waves of a run: the offers as cards (Buy, Lock), the free stat items of a
     * level up, Reroll and Ready. Clicks become the input's pick: the server (or the host) does the rest.
     */
    class ShopScreen
    {
    public:
        void init(Hud* hud);
        void setOpen(bool open);
        bool isOpen() const { return open; }

        /** Draws it for this player as the run has it now. */
        void refresh(const Content& content, const ArenaWorld& world, const Fighter& me);

        /** A click at screen pixels: the pick it makes (InputFrame::BUY + i...), or 0. */
        int click(float x, float y) const;

    private:
        struct Button
        {
            pg::_unique_id back = 0, label = 0;
            float x = 0, y = 0, w = 0, h = 0;
            int pick = 0;
            bool shown = false;
        };

        struct Card
        {
            pg::_unique_id back = 0, kind = 0, name = 0, text = 0;
            Button buy, lock;
        };

        void setButton(Button& b, float x, float y, float w, float h, const std::string& label, int pick, bool enabled, bool selected = false);
        void hideButton(Button& b);

        Hud* hud = nullptr;
        bool open = false;
        float px = 0, py = 0, pw = 0, ph = 0;
        pg::_unique_id panel = 0, title = 0, info = 0, levelTitle = 0, build = 0, hint = 0;
        std::vector<Card> cards;
        std::vector<Button> level;
        Button reroll, ready;
        std::string lastKey;
    };
}
