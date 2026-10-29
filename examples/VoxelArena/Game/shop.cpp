#include "stdafx.h"

#include "shop.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace arena
{
    namespace
    {
        const glm::vec4 PANEL(14, 16, 26, 238);
        const glm::vec4 CARD(30, 34, 48, 245);
        const glm::vec4 CARD_LOCKED(52, 46, 26, 245);
        const glm::vec4 CARD_BOUGHT(22, 24, 30, 230);
        const glm::vec4 BUTTON(42, 48, 66, 240);
        const glm::vec4 SELECTED(92, 112, 168, 250);
        const glm::vec4 DISABLED(26, 28, 36, 210);
        const glm::vec4 READY(60, 120, 80, 250);
        const glm::vec4 TEXT(236, 238, 242, 255);

        constexpr int CARDS = 4, LEVELS = 4;
        constexpr float CARD_W = 226.0f, CARD_H = 210.0f, GAP = 12.0f;
        constexpr float PANEL_Z = 6.0f, CARD_Z = 7.0f, BUTTON_Z = 8.0f, LABEL_Z = 9.0f;

        const char* const GOLD = "\\c{255,214,120,255}";
        const char* const SHARD = "\\c{190,160,255,255}";
        const char* const GREY = "\\c{150,154,166,255}";
        const char* const WHITE_TEXT = "\\c{236,238,242,255}";

        /** Words onto lines of at most `width` letters. */
        std::string wrap(const std::string& text, size_t width)
        {
            std::istringstream words(text);
            std::string word, line, out;

            while (words >> word)
            {
                if (not line.empty() and line.size() + 1 + word.size() > width)
                {
                    out += line + "\n";
                    line.clear();
                }
                line += (line.empty() ? "" : " ") + word;
            }

            return out + line;
        }

        std::string percent(float v) { return std::to_string(static_cast<int>(std::lround(v * 100.0f))) + "%"; }

        std::string kindColour(const std::string& kind)
        {
            if (kind == "Stat") return "\\c{150,200,255,255}";
            if (kind == "Weapon") return "\\c{255,190,120,255}";
            if (kind == "Skill") return "\\c{140,230,170,255}";
            if (kind == "Element") return "\\c{255,150,150,255}";
            return "\\c{210,170,255,255}";   // spell keywords
        }
    }

    OfferInfo offerInfo(const Content& content, const std::string& id)
    {
        if (auto u = content.upgrades.find(id); u != content.upgrades.end())
            return {"Stat", u->second.name, u->second.text};

        if (auto w = content.weapons.find(id); w != content.weapons.end())
            return {"Weapon", w->second.name, "A " + w->second.type + ", with its own combos and skills. Tab switches weapons."};

        if (auto a = content.abilities.find(id); a != content.abilities.end())
        {
            std::string with;
            for (const auto& t : a->second.weapons)
                with += (with.empty() ? "" : " or ") + t;
            return {"Skill", a->second.name, "A weapon skill" + (with.empty() ? std::string() : ", with a " + with) + ". Stamina and a cooldown, no mana."};
        }

        if (auto k = content.keywords.find(id); k != content.keywords.end())
        {
            if (k->second.form)
                return {"Spell form", k->second.name, "Spells can take the " + k->second.name + " form (the spellbook, B)."};
            return {"Modifier", k->second.name, "Spells can use " + k->second.name + ": it changes the form before it (the spellbook, B)."};
        }

        if (id.rfind("element:", 0) == 0)
        {
            Element e = Element::None;
            const std::string name = id.substr(8);
            const std::string shown = elementFromName(name, e) and content.elements.count(e) ? content.elements.at(e).name : name;
            return {"Element", shown, "Spells of " + shown + " (the spellbook, B)."};
        }

        return {"", id, ""};
    }

    void ShopScreen::init(Hud* h)
    {
        hud = h;
        pw = 4.0f * CARD_W + 3.0f * GAP + 48.0f;
        ph = 600.0f;
        px = std::floor((hud->width() - pw) * 0.5f);
        py = std::floor((hud->height() - ph) * 0.5f);

        panel = hud->makeQuad(px, py, PANEL_Z, pw, ph, PANEL);
        title = hud->makeText(px + 24, py + 14, LABEL_Z, Hud::FONT, "", 1.25f, TEXT);
        info = hud->makeText(px + 24, py + 54, LABEL_Z, Hud::FONT, "", 1.0f, TEXT);
        levelTitle = hud->makeText(px + 24, py + 334, LABEL_Z, Hud::FONT, "", 1.0f, TEXT);
        build = hud->makeText(px + 24, py + 462, LABEL_Z, Hud::MONO, "", 0.8f, glm::vec4(200, 204, 214, 255));
        hint = hud->makeText(px + 260, py + ph - 42, LABEL_Z, Hud::MONO, "1-4 buy    R reroll    Enter ready    click Lock to keep an offer", 0.8f, glm::vec4(150, 154, 166, 255));

        auto makeButton = [&](Button& b) {
            b.back = hud->makeQuad(0, 0, BUTTON_Z, 10, 10, BUTTON);
            b.label = hud->makeText(0, 0, LABEL_Z, Hud::FONT, "", 0.85f, TEXT);
            hud->show(b.back, false);
            hud->show(b.label, false);
        };

        // Made now, every one (an entity made while the screen is drawn has no components that frame)
        cards.resize(CARDS);
        for (int i = 0; i < CARDS; ++i)
        {
            Card& c = cards[static_cast<size_t>(i)];
            const float x = px + 24.0f + static_cast<float>(i) * (CARD_W + GAP), y = py + 92.0f;
            c.back = hud->makeQuad(x, y, CARD_Z, CARD_W, CARD_H, CARD);
            c.kind = hud->makeText(x + 12, y + 8, LABEL_Z, Hud::MONO, "", 0.8f, TEXT);
            c.name = hud->makeText(x + 12, y + 30, LABEL_Z, Hud::FONT, "", 1.05f, TEXT);
            c.text = hud->makeText(x + 12, y + 64, LABEL_Z, Hud::FONT, "", 0.8f, glm::vec4(200, 204, 214, 255));
            makeButton(c.buy);
            makeButton(c.lock);
        }

        level.resize(LEVELS);
        for (auto& b : level)
            makeButton(b);
        makeButton(reroll);
        makeButton(ready);

        setOpen(false);
    }

    void ShopScreen::setButton(Button& b, float x, float y, float w, float h, const std::string& label, int pick, bool enabled, bool selected)
    {
        b.x = x;
        b.y = y;
        b.w = w;
        b.h = h;
        b.pick = enabled ? pick : 0;
        b.shown = true;
        hud->place(b.back, x, y, w, h, true);
        hud->color(b.back, selected ? SELECTED : (enabled ? BUTTON : DISABLED));
        hud->place(b.label, x + 10.0f, y + h * 0.5f - 11.0f, w, h, true);
        hud->text(b.label, (enabled ? "" : std::string("\\c{110,114,124,255}")) + label);
    }

    void ShopScreen::hideButton(Button& b)
    {
        b.shown = false;
        b.pick = 0;
        hud->show(b.back, false);
        hud->show(b.label, false);
    }

    void ShopScreen::setOpen(bool o)
    {
        open = o;
        lastKey.clear();

        for (auto id : {panel, title, info, levelTitle, build, hint})
            hud->show(id, open);

        for (auto& c : cards)
        {
            for (auto id : {c.back, c.kind, c.name, c.text})
                hud->show(id, open);
            hideButton(c.buy);
            hideButton(c.lock);
        }

        for (auto& b : level)
            hideButton(b);
        hideButton(reroll);
        hideButton(ready);
    }

    void ShopScreen::refresh(const Content& content, const ArenaWorld& world, const Fighter& me)
    {
        if (not open)
            return;

        // Only when something changed: laying out text each frame costs
        std::ostringstream key;
        key << world.run.wave << "|" << me.shards << "|" << me.xp << "|" << me.level << "|" << me.levelUps << "|" << me.ready << "|" << me.rerolls << "|" << me.boons.size() << "|" << me.owned.size();
        for (const auto& o : me.shop)
            key << "|" << o.id << o.price << o.locked << o.bought;
        for (const auto& l : me.levelOffer)
            key << "|" << l;
        int waiting = 0;
        for (const auto& f : world.fighters)
            waiting += f.team == Team::Player and not f.ready ? 1 : 0;
        key << "|" << waiting;

        if (key.str() == lastKey)
            return;
        lastKey = key.str();

        const RunState& r = world.run;
        hud->text(title, "Wave " + std::to_string(r.wave) + " of " + std::to_string(r.waves) + " survived" +
                         (content.run.bosses.count(r.wave + 1) ? std::string("     ") + "\\c{230,120,255,255}next: a boss" : std::string()));
        hud->text(info, std::string(SHARD) + std::to_string(me.shards) + " shards" + WHITE_TEXT + "      Level " + std::to_string(me.level) + GREY + "   (" +
                        std::to_string(me.xp) + " / " + std::to_string(world.xpFor(me.level)) + " to the next)" + WHITE_TEXT + "      Score " + std::to_string(r.score));

        // The offers
        for (int i = 0; i < CARDS; ++i)
        {
            Card& c = cards[static_cast<size_t>(i)];
            const float x = px + 24.0f + static_cast<float>(i) * (CARD_W + GAP), y = py + 92.0f;

            if (i >= static_cast<int>(me.shop.size()))
            {
                for (auto id : {c.kind, c.name, c.text})
                    hud->text(id, "");
                hud->color(c.back, CARD_BOUGHT);
                hideButton(c.buy);
                hideButton(c.lock);
                continue;
            }

            const auto& o = me.shop[static_cast<size_t>(i)];
            const OfferInfo about = offerInfo(content, o.id);
            const int had = static_cast<int>(std::count(me.boons.begin(), me.boons.end(), o.id));
            hud->color(c.back, o.bought ? CARD_BOUGHT : (o.locked ? CARD_LOCKED : CARD));
            hud->text(c.kind, kindColour(about.kind) + about.kind + (had > 0 ? std::string(GREY) + "  (have " + std::to_string(had) + ")" : ""));
            hud->text(c.name, (o.bought ? std::string(GREY) : std::string()) + about.name);
            hud->text(c.text, wrap(about.text, 28));

            const float by = y + CARD_H - 44.0f;

            if (o.bought)
            {
                setButton(c.buy, x + 12, by, CARD_W - 24, 34, "Bought", 0, false);
                hideButton(c.lock);
            }
            else
            {
                const bool afford = me.shards >= o.price;
                setButton(c.buy, x + 12, by, 128, 34, "[" + std::to_string(i + 1) + "] " + (afford ? SHARD : "\\c{255,120,110,255}") + std::to_string(o.price), InputFrame::BUY + i, true);
                setButton(c.lock, x + 148, by, CARD_W - 160, 34, o.locked ? "Locked" : "Lock", InputFrame::LOCK + i, true, o.locked);
            }
        }

        // A level up: a free stat item
        if (me.levelUps > 0 and not me.levelOffer.empty())
        {
            hud->text(levelTitle, std::string(GOLD) + "Level up!" + WHITE_TEXT + "  A stat item, free" + (me.levelUps > 1 ? " (" + std::to_string(me.levelUps) + " to pick)" : std::string()) + ":");

            for (int i = 0; i < LEVELS; ++i)
            {
                if (i >= static_cast<int>(me.levelOffer.size()))
                {
                    hideButton(level[static_cast<size_t>(i)]);
                    continue;
                }

                // Its name, and what it does on two lines at most
                const OfferInfo about = offerInfo(content, me.levelOffer[static_cast<size_t>(i)]);
                std::string text = wrap(about.text, 23);
                if (const size_t second = text.find('\n'); second != std::string::npos)
                    if (const size_t third = text.find('\n', second + 1); third != std::string::npos)
                        text = text.substr(0, third) + "...";

                setButton(level[static_cast<size_t>(i)], px + 24.0f + static_cast<float>(i) * (CARD_W + GAP), py + 364, CARD_W, 78,
                          about.name + "\n" + GREY + text, InputFrame::LEVEL + i, true);
                hud->place(level[static_cast<size_t>(i)].label, px + 34.0f + static_cast<float>(i) * (CARD_W + GAP), py + 370, CARD_W, 78, true);
            }
        }
        else
        {
            hud->text(levelTitle, "");
            for (auto& b : level)
                hideButton(b);
        }

        // What the player has
        const auto& b = me.boon;
        std::string stats = "Health " + std::to_string(static_cast<int>(me.maxHealth)) + "   Damage +" + percent(b.damage) + "   Armor " + percent(std::min(b.armor, 0.7f)) +
                            "   Crit " + percent(b.crit) + "   Speed +" + percent(b.speed) + "   Regen " + std::to_string(b.regen).substr(0, 3) + "/s   Dashes " +
                            std::to_string(world.maxDashes(me)) + "   Pickup " + std::to_string(content.run.pickupRange + b.pickupRange).substr(0, 3) + " m";
        std::string owned;
        for (const auto& id : me.owned)
            owned += (owned.empty() ? "" : ", ") + offerInfo(content, id).name;
        hud->text(build, stats + "\nOwned: " + wrap(owned, 120));

        // Reroll, Ready
        const int price = world.rerollPrice(me);
        setButton(reroll, px + 24, py + ph - 52, 200, 38, "[R] Reroll  " + std::string(me.shards >= price ? SHARD : "\\c{255,120,110,255}") + std::to_string(price), InputFrame::REROLL, true);
        setButton(ready, px + pw - 254, py + ph - 52, 230, 38,
                  me.ready ? (waiting > 0 ? "Waiting for " + std::to_string(waiting) + "..." : "Ready") : "[Enter] Ready", InputFrame::START, true, me.ready);
        hud->color(ready.back, me.ready ? READY : BUTTON);
    }

    int ShopScreen::click(float x, float y) const
    {
        if (not open)
            return 0;

        auto inside = [&](const Button& b) { return b.shown and b.pick != 0 and x >= b.x and x <= b.x + b.w and y >= b.y and y <= b.y + b.h; };

        for (const auto& c : cards)
        {
            if (inside(c.buy))
                return c.buy.pick;
            if (inside(c.lock))
                return c.lock.pick;
        }

        for (const auto& b : level)
            if (inside(b))
                return b.pick;

        if (inside(reroll))
            return reroll.pick;
        if (inside(ready))
            return ready.pick;

        return 0;
    }
}
