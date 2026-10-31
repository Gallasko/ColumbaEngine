#include "stdafx.h"

#include "shop.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#include "Core/spells.h"

namespace arena
{
    using namespace ui;

    namespace
    {
        const Colour BOSS_TAG = hex(0x2E1636);
        const Colour LEVEL_STRIP = hex(0x2A2414);
        const Colour LEVEL_PICK = hex(0x3A321C);
        const Colour BOUGHT_FILL = hex(0x16261C);
        const Colour WAITING = hex(0x3A3E4A);

        constexpr int CARDS = 4;
        constexpr float CARD_X[CARDS] = {66, 287, 508, 729};
        constexpr float CARD_Y = 186, CARD_W = 209, CARD_H = 416;

        std::string percent(float v) { return std::to_string(static_cast<int>(std::lround(v * 100.0f))) + "%"; }

        std::string oneDecimal(float v)
        {
            std::ostringstream o;
            o.setf(std::ios::fixed);
            o.precision(1);
            o << v;
            return o.str();
        }

        std::string localName(const std::string& id)
        {
            const size_t colon = id.find(':');
            return colon == std::string::npos ? id : id.substr(colon + 1);
        }

        std::string roman(int n)
        {
            static const char* const numerals[] = {"", "", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
            return n >= 0 and n <= 10 ? numerals[n] : std::to_string(n);
        }

        /** Commonest first: grey, blue, purple, orange (more than four: the last again). */
        Colour rarityColour(int rarity)
        {
            static const Colour colours[] = {hex(0xAEB4C2), hex(0x5EA8E8), hex(0xB27CE8), hex(0xF0A040)};
            return colours[std::clamp(rarity, 0, 3)];
        }

        std::string rarityName(const Content& content, int rarity)
        {
            const auto& r = content.run.rarities;
            return rarity >= 0 and rarity < static_cast<int>(r.size()) ? r[static_cast<size_t>(rarity)].name : "";
        }

        float rarityPower(const Content& content, int rarity)
        {
            const auto& r = content.run.rarities;
            return rarity >= 0 and rarity < static_cast<int>(r.size()) ? r[static_cast<size_t>(rarity)].power : 1.0f;
        }

        std::string upper(std::string t)
        {
            for (auto& c : t)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return t;
        }
    }

    std::string runSpellName(const Content& content, const Fighter::RunSpell& spell)
    {
        auto it = content.spells.find(spell.id);
        const std::string name = it != content.spells.end() ? it->second.name : localName(spell.id);
        return spell.level > 1 ? name + " " + roman(spell.level) : name;
    }

    std::string itemText(const Content& content, const UpgradeDef& u, float k)
    {
        (void)content;
        std::vector<std::string> parts;
        auto pc = [&](float v) { return std::to_string(static_cast<int>(std::lround(v * k * 100.0f))) + "%"; };
        auto num = [&](float v) { return oneDecimal(v * k); };

        if (u.forSpell)
        {
            const SpellTweak& t = u.spell;
            if (t.damage != 0.0f) parts.push_back("+" + pc(t.damage) + " damage");
            if (t.area != 0.0f) parts.push_back("+" + pc(t.area) + " area");
            if (t.cooldown != 0.0f) parts.push_back("recharges " + pc(t.cooldown) + " faster");
            if (t.cost != 0.0f) parts.push_back("costs " + pc(t.cost) + " less mana");
            if (t.duration != 0.0f) parts.push_back("lasts " + pc(t.duration) + " longer");
            if (t.speed != 0.0f) parts.push_back("flies " + pc(t.speed) + " faster");
        }
        else
        {
            if (u.maxHealth != 0.0f) parts.push_back("+" + std::to_string(static_cast<int>(std::lround(u.maxHealth * k))) + " max health");
            if (u.damage != 0.0f) parts.push_back("+" + pc(u.damage) + " damage");
            if (u.dashCharges != 0) parts.push_back("+" + std::to_string(u.dashCharges) + (u.dashCharges == 1 ? " dash charge" : " dash charges"));
            if (u.manaCost != 0.0f) parts.push_back("Spells cost " + pc(u.manaCost) + " less");
            if (u.manaRegen != 0.0f) parts.push_back("+" + pc(u.manaRegen) + " mana regeneration");
            if (u.staminaRegen != 0.0f) parts.push_back("+" + pc(u.staminaRegen) + " stamina regeneration");
            if (u.speed != 0.0f) parts.push_back("+" + pc(u.speed) + " move speed");
            if (u.cooldown != 0.0f) parts.push_back("Skills and spells recharge " + pc(u.cooldown) + " faster");
            if (u.lifeOnHit != 0.0f) parts.push_back("Heal " + num(u.lifeOnHit) + " on every hit");
            if (u.statusStacks != 0) parts.push_back("Your statuses land " + std::to_string(u.statusStacks) + (u.statusStacks == 1 ? " stack deeper" : " stacks deeper"));
            if (u.armor != 0.0f) parts.push_back("Take " + pc(u.armor) + " less damage");
            if (u.crit != 0.0f) parts.push_back("+" + pc(u.crit) + " chance of a critical hit");
            if (u.pickupRange != 0.0f) parts.push_back("Pick up shards " + num(u.pickupRange) + " m further");
            if (u.regen != 0.0f) parts.push_back("Regenerate " + num(u.regen) + " health a second");
        }

        if (parts.empty())
            return u.text;

        std::string out;
        for (const auto& part : parts)
            out += (out.empty() ? "" : ", ") + part;
        return out;
    }

    OfferInfo offerInfo(const Content& content, const std::string& id)
    {
        if (auto u = content.upgrades.find(id); u != content.upgrades.end())
            return {u->second.forSpell ? "Spell item" : "Stat", u->second.name, itemText(content, u->second, 1.0f)};

        if (auto sp = content.spells.find(id); sp != content.spells.end())
            return {"Spell", sp->second.name, sp->second.text.empty() ? describe(content, compileSpell(content, sp->second.recipe)) : sp->second.text};

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
                return {"Spell form", k->second.name, "Spells can take the " + k->second.name + " form (the spellbook)."};
            return {"Modifier", k->second.name, "Spells can use " + k->second.name + ": it changes the form before it (the spellbook)."};
        }

        if (id.rfind("element:", 0) == 0)
        {
            Element e = Element::None;
            const std::string name = id.substr(8);
            const std::string shown = elementFromName(name, e) and content.elements.count(e) ? content.elements.at(e).name : name;
            return {"Element", shown, "Spells of " + shown + " (the spellbook)."};
        }

        return {"", id, ""};
    }

    std::string offerIcon(const Content& content, const std::string& id)
    {
        if (id.rfind("element:", 0) == 0)
            return id.substr(8);

        if (auto sp = content.spells.find(id); sp != content.spells.end())
            return sp->second.recipe.element;

        const std::string name = localName(id);

        if (content.upgrades.count(id))
        {
            static const std::map<std::string, std::string> stats = {
                {"tough", "heart"}, {"keen", "keen"}, {"nimble", "dash"}, {"focus", "eye"}, {"wellspring", "drop"},
                {"second_wind", "wind"}, {"swift", "dash"}, {"quick", "clock"}, {"leech", "drop"}, {"potent", "stacks"},
                {"plated", "shield"}, {"precise", "target"}, {"magnet", "magnet"}, {"mending", "plus"},
            };
            auto it = stats.find(name);
            return it != stats.end() ? it->second : "star";
        }

        if (auto w = content.weapons.find(id); w != content.weapons.end())
        {
            const std::string& type = w->second.type;
            if (type == "dagger" or type == "spear" or type == "sword" or type == "bow" or type == "staff")
                return type;
            return "sword";
        }

        if (content.abilities.count(id))
        {
            if (name == "whirlwind") return "whirlwind";
            if (name == "leap_slam" or name == "backflip") return "leap";
            if (name == "shadow_dash") return "dash";
            if (name == "raise_wall") return "wall";
            if (name == "dig_pit") return "pit";
            if (name == "mine") return "mine";
            if (name == "volley") return "bow";
            if (name == "disengage") return "leap";
            if (name == "sanctuary") return "zone";
            if (name == "radiance") return "nova";
            return "star";
        }

        if (content.keywords.count(id))
            return ui::icon(name).empty() ? "star" : name;

        return "star";
    }

    void ShopScreen::draw(Ui& ui, const Content& content, const ArenaWorld& world, const Fighter& me)
    {
        const RunState& r = world.run;

        // The level ups first: the shop after
        if (me.levelUps > 0 and not me.levelOffer.empty())
        {
            drawLevelUp(ui, content, world, me);
            return;
        }

        ui.section("shop");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.58f));

        ui.anchor(Anchor::Centre);
        ui.raised(40, 28, 1200, 664, PANEL, 4.0f);

        // The title, and a boss next
        const float titleW = ui.text(66, 52, "WAVE " + std::to_string(r.wave) + " OF " + std::to_string(r.waves) + " SURVIVED", Font::Display, 40, TEXT, Align::Left, 1.0f);

        if (content.run.bosses.count(r.wave + 1))
        {
            const float tx = 66.0f + titleW + 14.0f;
            const float tw = 14.0f + 6.0f + ui.measure("next: a boss", Font::Text700, 12) + 16.0f;
            ui.rect(tx, 62, tw, 24, BOSS_TAG);
            ui.icon("skull", tx + 8, 67, 2.0f, BOSS);
            ui.text(tx + 28, 67, "next: a boss", Font::Text700, 12, BOSS);
        }

        // The chips, right to left: the score, the level, the shards
        {
            float right = 1214.0f;
            auto chip = [&](float w) { right -= w; ui.box(right, 52, w, 42, CARD); const float x = right; right -= 14.0f; return x; };

            const std::string score = std::to_string(r.score);
            const float scoreW = 14.0f + ui.measure("SCORE", Font::Text700, 11, 1.5f) + 8.0f + ui.measure(score, Font::Display, 28) + 14.0f;
            float x = chip(scoreW);
            ui.text(x + 14, 66, "SCORE", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.text(x + scoreW - 14, 58, score, Font::Display, 28, TEXT, Align::Right);

            const std::string lv = "Lv " + std::to_string(me.level);
            const std::string next = std::to_string(me.xp) + "/" + std::to_string(world.xpFor(me.level)) + " to Lv " + std::to_string(me.level + 1);
            const float lvW = ui.measure(lv, Font::Display, 28);
            const float levelW = 14.0f + lvW + 10.0f + std::max(12.0f * 8.0f - 2.0f, ui.measure(next, Font::Text600, 10)) + 14.0f;
            x = chip(levelW);
            ui.text(x + 14, 58, lv, Font::Display, 28, TEXT);
            const int pips = static_cast<int>(std::floor(std::clamp(static_cast<float>(me.xp) / static_cast<float>(std::max(1, world.xpFor(me.level))), 0.0f, 1.0f) * 12.0f));
            for (int k = 0; k < 12; ++k)
                ui.rect(x + 14 + lvW + 10 + static_cast<float>(k) * 8.0f, 62, 6, 6, k < pips ? GOLD : WELL);
            ui.text(x + 14 + lvW + 10, 72, next, Font::Text600, 10, MUTED);

            const std::string shards = std::to_string(me.shards);
            const float shardW = 14.0f + 21.0f + 8.0f + ui.measure(shards, Font::Display, 30) + 14.0f;
            x = chip(shardW);
            ui.icon("shard", x + 14, 62, 3.0f, SHARDS);
            ui.text(x + 14 + 21 + 8, 57, shards, Font::Display, 30, SHARDS);
        }

        // What is sold, by rarity
        {
            float lx = 66.0f + ui.text(66, 124, "Stat items, and items for one of your spells. Rarer ones do more:", Font::Text500, 13, TEXT_DIM) + 10.0f;
            for (int k = 0; k < static_cast<int>(content.run.rarities.size()); ++k)
            {
                const auto itemScope = ui.scope(static_cast<uint64_t>(100 + k));
                ui.rect(lx, 128, 8, 8, rarityColour(k));
                lx += 14.0f + ui.text(lx + 14, 124, content.run.rarities[static_cast<size_t>(k)].name + " x" + oneDecimal(content.run.rarities[static_cast<size_t>(k)].power),
                                      Font::Text700, 12, rarityColour(k)) + 14.0f;
            }
        }

        // The offers
        for (int i = 0; i < CARDS; ++i)
        {
            const auto itemScope = ui.scope(static_cast<uint64_t>(i));
            const float x = CARD_X[i], y = CARD_Y;

            if (i >= static_cast<int>(me.shop.size()))
            {
                ui.raised(x, y, CARD_W, CARD_H, CARD_DIM, 0.0f, 2.0f, LINE);
                continue;
            }

            const auto& o = me.shop[static_cast<size_t>(i)];
            OfferInfo about = offerInfo(content, o.id);
            const auto item = content.upgrades.find(o.id);
            if (item != content.upgrades.end())
                about.text = itemText(content, item->second, rarityPower(content, o.rarity));

            // An item has a rarity: its colour on the card's edge and its kind row
            const bool rated = item != content.upgrades.end() and not content.run.rarities.empty();
            const Colour kind = rated ? rarityColour(o.rarity) : kindColour(about.kind);
            const int had = static_cast<int>(std::count_if(me.boons.begin(), me.boons.end(), [&](const Fighter::Boon& b) { return b.id == o.id and b.spell == o.spell; }));
            const bool afford = me.shards >= o.price;

            if (o.bought)
                ui.raised(x, y, CARD_W, CARD_H, CARD_DIM, 0.0f, 2.0f, INK);
            else
                ui.raised(x, y, CARD_W, CARD_H, CARD, 3.0f, 2.0f, o.locked ? GOLD : (rated ? kind : INK));

            const Colour main = o.bought ? MUTED : TEXT;
            float cy = y + 14.0f;

            // The kind row: "RARE STAT"
            const std::string kindText = (rated ? upper(rarityName(content, o.rarity)) + " " : std::string()) + upper(about.kind.empty() ? "?" : about.kind);
            ui.rect(x + 14, cy + 2, 8, 8, o.bought ? MUTED : kind);
            float kx = x + 28.0f + ui.text(x + 28, cy, kindText, Font::Text700, 11, o.bought ? MUTED : kind, Align::Left, 1.5f);
            if (had > 0)
                ui.text(kx + 8, cy, "(have " + std::to_string(had) + ")", Font::Text600, 11, MUTED);
            cy += 14.0f + 10.0f;

            // The icon (a spell item: its spell's)
            ui.box(x + 14, cy, 52, 52, WELL, 2.0f, o.bought ? LINE : kind);
            ui.icon(offerIcon(content, o.spell.empty() ? o.id : o.spell), x + 26, cy + 12, 4.0f, o.bought ? MUTED : kind);
            cy += 52.0f + 10.0f;

            ui.text(x + 14, cy, about.name, Font::Display, 30, main);
            cy += 30.0f + 10.0f;

            // A spell item: which spell
            if (not o.spell.empty())
            {
                Fighter::RunSpell target{o.spell, 1};
                for (const auto& rs : me.runSpells)
                    if (rs.id == o.spell)
                        target = rs;
                ui.text(x + 14, cy - 4, "for " + runSpellName(content, target), Font::Text700, 13, o.bought ? MUTED : GOLD);
                cy += 20.0f;
            }

            ui.paragraph(x + 14, cy, CARD_W - 28.0f, about.text, Font::Text500, 13, o.bought ? MUTED : TEXT_DIM, 18.2f, 9);

            // Buy
            const float buyY = y + CARD_H - 14 - 30 - 10 - 40;
            const float bx = x + 14, bw = CARD_W - 28;

            if (o.bought)
            {
                ui.box(bx, buyY, bw, 40, BOUGHT_FILL, 2.0f, INK);
                const float lw = 14.0f + 8.0f + ui.measure("BOUGHT", Font::Display, 22);
                ui.icon("check", bx + (bw - lw) * 0.5f, buyY + 13, 2.0f, GOOD);
                ui.text(bx + (bw - lw) * 0.5f + 22, buyY + 8, "BOUGHT", Font::Display, 22, GOOD);
            }
            else
            {
                const bool over = ui.hover(bx, buyY, bw, 40);
                if (afford)
                    ui.raised(bx, buyY, bw, 40, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
                else
                    ui.box(bx, buyY, bw, 40, CARD, 2.0f, over ? GOLD : LINE);

                ui.keycap(bx + 10, buyY + 10, 20, std::to_string(i + 1), INK, GOLD, 16, 0.0f);
                ui.text(bx + 38, buyY + 11, "Buy", Font::Text700, 14, afford ? GOLD_INK : TEXT);
                ui.text(bx + bw - 12, buyY + 8, std::to_string(o.price), Font::Display, 22, afford ? SHARD_ON_GOLD : WARNING, Align::Right);
                ui.hit(bx, buyY, bw, 40, InputFrame::BUY + i);
            }

            // Lock
            if (not o.bought)
            {
                const float ly = y + CARD_H - 14 - 30;
                const bool over = ui.hover(bx, ly, bw, 30);

                if (o.locked)
                {
                    ui.box(bx, ly, bw, 30, GOLD, 2.0f, over ? TEXT : INK);
                    ui.icon("lock", bx + 10, ly + 8, 2.0f, GOLD_INK);
                    ui.text(bx + 30, ly + 8, "Kept for next shop", Font::Text700, 12, GOLD_INK);
                }
                else
                {
                    ui.box(bx, ly, bw, 30, CARD_DIM, 2.0f, over ? GOLD : LINE);
                    ui.icon("lock", bx + 10, ly + 8, 2.0f, MUTED);
                    ui.text(bx + 30, ly + 8, "Lock", Font::Text700, 12, TEXT_DIM);
                    ui.text(bx + bw - 10, ly + 8, "Shift+" + std::to_string(i + 1), Font::Text600, 11, FAINT, Align::Right);
                }

                ui.hit(bx, ly, bw, 30, InputFrame::LOCK + i);
            }
        }

        // The build so far
        {
            const float x = 954, y = 108, w = 260;
            ui.raised(x, y, w, 494, CARD, 3.0f);
            ui.text(x + 14, y + 12, "YOUR BUILD", Font::Text700, 11, MUTED, Align::Left, 1.5f);

            const auto& b = me.boon;
            struct Row { std::string label, value, faint; };
            const std::vector<Row> rows = {
                {"Health", std::to_string(static_cast<int>(me.maxHealth)), ""},
                {"Damage", "+" + percent(b.damage), ""},
                {"Armor", percent(std::min(b.armor, 0.7f)), " / 70"},
                {"Crit", percent(b.crit), ""},
                {"Speed", "+" + percent(b.speed), ""},
                {"Regen", oneDecimal(b.regen) + "/s", ""},
                {"Dashes", std::to_string(world.maxDashes(me)), ""},
                {"Pickup", oneDecimal(content.run.pickupRange + b.pickupRange) + " m", ""},
            };

            float ry = y + 34.0f;
            for (const auto& row : rows)
            {
                const auto itemScope = ui.scope(Ui::idOf(row.label));
                ui.text(x + 14, ry + 3, row.label, Font::Text500, 13, TEXT_DIM);
                float vx = x + w - 14.0f;
                if (not row.faint.empty())
                    vx -= ui.text(vx, ry, row.faint, Font::Display, 20, FAINT, Align::Right);
                ui.text(vx, ry, row.value, Font::Display, 20, TEXT, Align::Right);
                ry += 22.0f;
            }

            ry += 8.0f;
            ui.rect(x + 14, ry, w - 28, 2, LINE);
            ry += 12.0f;
            ui.text(x + 14, ry, "SPELLS", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ry += 20.0f;

            for (const auto& rs : me.runSpells)
            {
                const auto itemScope = ui.scope(Ui::idOf("spell " + rs.id));
                const std::string element = content.spells.count(rs.id) ? content.spells.at(rs.id).recipe.element : "";
                ui.icon(element, x + 14, ry + 2, 2.0f, element == "ice" ? hex(0x7FC8F0) : element == "volt" ? hex(0xE8D060) : hex(0xE87850));
                ui.text(x + 34, ry, runSpellName(content, rs), Font::Text600, 13, TEXT);
                ry += 20.0f;
            }

            ry += 6.0f;
            ui.text(x + 14, ry, "OWNED", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ry += 20.0f;

            // The owned tags, wrapping
            float tx = x + 14.0f;
            for (const auto& id : me.owned)
            {
                const auto itemScope = ui.scope(Ui::idOf(id));
                const OfferInfo about = offerInfo(content, id);
                const float tw = 6.0f + 6.0f + 6.0f + ui.measure(about.name, Font::Text600, 11) + 8.0f;

                if (tx + tw > x + w - 14.0f)
                {
                    tx = x + 14.0f;
                    ry += 24.0f;
                }

                if (ry > y + 494 - 28)
                    break;

                ui.rect(tx, ry, tw, 20, WELL);
                ui.rect(tx + 6, ry + 7, 6, 6, kindColour(about.kind));
                ui.text(tx + 18, ry + 3, about.name, Font::Text600, 11, TEXT_DIM);
                tx += tw + 4.0f;
            }
        }

        // Reroll
        {
            const int price = world.rerollPrice(me);
            Fighter after = me;
            ++after.rerolls;
            const int next = world.rerollPrice(after);
            const bool afford = me.shards >= price;

            const float x = 66, y = 621, w = 250, h = 42;
            const bool over = ui.hover(x, y, w, h);
            ui.box(x, y, w, h, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(x + 10, y + 10, 22, "R", GOLD, GOLD_INK, 18, 0.0f);
            ui.text(x + 42, y + 12, "Reroll", Font::Text700, 14, TEXT);
            const float px = x + 42 + ui.measure("Reroll", Font::Text700, 14) + 10.0f;
            const float pw = ui.text(px, y + 9, std::to_string(price), Font::Display, 22, afford ? SHARDS : WARNING);
            ui.text(px + pw + 10, y + 14, "next " + std::to_string(next), Font::Text600, 11, FAINT);
            ui.hit(x, y, w, h, InputFrame::REROLL);
        }

        ui.text(340, 636, "1-4 buy \xC2\xB7 Shift+n lock \xC2\xB7 R reroll \xC2\xB7 Enter ready", Font::Text500, 11, FAINT);

        // Ready
        {
            int waiting = 0, players = 0;
            for (const auto& f : world.fighters)
            {
                if (f.team != Team::Player)
                    continue;
                ++players;
                waiting += f.ready ? 0 : 1;
            }

            const float x = 954, y = 616, w = 260, h = 52;
            const bool over = ui.hover(x, y, w, h);

            if (me.ready)
            {
                ui.box(x, y, w, h, WAITING, 2.0f, INK);
                ui.text(x + 18, y + 11, waiting > 0 ? "WAITING\xE2\x80\xA6" : "READY", Font::Display, 30, TEXT, Align::Left, 2.0f);
                ui.text(x + w - 18, y + 15, std::to_string(players - waiting) + " / " + std::to_string(players), Font::Display, 24, TEXT_DIM, Align::Right);
            }
            else
            {
                ui.raised(x, y, w, h, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
                ui.text(x + 18, y + 11, "READY", Font::Display, 30, GOLD_INK, Align::Left, 2.0f);
                const float kw = ui.measure("ENTER", Font::Display, 16) + 12.0f;
                ui.box(x + w - 18 - kw, y + 15, kw, 22, INK);
                ui.text(x + w - 18 - kw * 0.5f, y + 17, "ENTER", Font::Display, 16, GOLD, Align::Centre);
            }

            ui.hit(x, y, w, h, InputFrame::START);
        }
    }

    void ShopScreen::drawLevelUp(Ui& ui, const Content& content, const ArenaWorld& world, const Fighter& me)
    {
        (void)world;
        ui.section("levelup");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.62f));

        ui.anchor(Anchor::Centre);
        ui.text(640, 92, "LEVEL UP", Font::Display, 64, GOLD, Align::Centre, 2.0f);
        ui.text(640, 168, "Lv " + std::to_string(me.level - me.levelUps + 1) + (me.levelUps > 1 ? "  \xC2\xB7  " + std::to_string(me.levelUps) + " picks to make" : "  \xC2\xB7  pick one"),
                Font::Text700, 14, TEXT_DIM, Align::Centre, 1.5f);

        const size_t n = me.levelOffer.size();
        const float w = 330.0f, h = 380.0f, gap = 24.0f, y = 210.0f;
        const float startX = 640.0f - (static_cast<float>(n) * w + static_cast<float>(n - 1) * gap) * 0.5f;

        for (size_t i = 0; i < n; ++i)
        {
            const auto itemScope = ui.scope(i);
            const std::string& id = me.levelOffer[i];
            const float x = std::round(startX + static_cast<float>(i) * (w + gap));
            const bool over = ui.hover(x, y, w, h);

            // What it is: a spell new or levelled, or a skill
            std::string kind, name, text;
            Colour colour = KIND_SKILL;

            if (auto sp = content.spells.find(id); sp != content.spells.end())
            {
                const auto has = std::find_if(me.runSpells.begin(), me.runSpells.end(), [&](const Fighter::RunSpell& r) { return r.id == id; });
                const int level = has == me.runSpells.end() ? 1 : has->level + 1;
                SpellRecipe recipe = sp->second.recipe;
                for (int l = 2; l <= level and l - 2 < static_cast<int>(sp->second.levels.size()); ++l)
                    for (const auto& k : sp->second.levels[static_cast<size_t>(l - 2)])
                        recipe.keywords.push_back(k);

                const std::string element = recipe.element;
                colour = element == "ice" ? hex(0x7FC8F0) : element == "volt" ? hex(0xE8D060) : hex(0xE87850);
                name = runSpellName(content, {id, level});

                if (has == me.runSpells.end())
                {
                    kind = "NEW SPELL";
                    text = (sp->second.text.empty() ? "" : sp->second.text + " ") + describe(content, compileSpell(content, recipe));
                }
                else
                {
                    kind = "SPELL LEVEL " + std::to_string(level);
                    std::string adds;
                    for (const auto& k : sp->second.levels[static_cast<size_t>(level - 2)])
                        adds += (adds.empty() ? "" : " and ") + (content.keywords.count(k) ? content.keywords.at(k).name : localName(k));
                    text = "Adds " + adds + ". " + describe(content, compileSpell(content, recipe));
                }
            }
            else if (auto a = content.abilities.find(id); a != content.abilities.end())
            {
                kind = "NEW SKILL";
                name = a->second.name;
                text = offerInfo(content, id).text;
            }

            ui.raised(x, y, w, h, CARD, 3.0f, 2.0f, over ? GOLD : colour);
            float cy = y + 18.0f;
            ui.rect(x + 18, cy + 2, 8, 8, colour);
            ui.text(x + 32, cy, kind, Font::Text700, 12, colour, Align::Left, 1.5f);
            cy += 30.0f;

            ui.box(x + 18, cy, 64, 64, WELL, 2.0f, colour);
            ui.icon(offerIcon(content, id), x + 32, cy + 14, 5.0f, colour);
            cy += 64.0f + 14.0f;

            ui.text(x + 18, cy, name, Font::Display, 34, TEXT);
            cy += 34.0f + 14.0f;

            ui.paragraph(x + 18, cy, w - 36.0f, text, Font::Text500, 14, TEXT_DIM, 19.6f, 8);

            // Take it
            const float by = y + h - 18.0f - 44.0f;
            ui.raised(x + 18, by, w - 36, 44, over ? GOLD : CARD_DIM, 3.0f, 2.0f, over ? TEXT : LINE);
            ui.keycap(x + 30, by + 11, 22, std::to_string(i + 1), over ? INK : GOLD, over ? GOLD : GOLD_INK, 18, 0.0f);
            ui.text(x + 64, by + 11, "Take", Font::Text700, 16, over ? GOLD_INK : TEXT);

            ui.hit(x, y, w, h, InputFrame::LEVEL + static_cast<int>(i));
        }

        ui.text(640, 620, "1-" + std::to_string(n) + " or click to take one \xC2\xB7 the shop comes next", Font::Text500, 12, FAINT, Align::Centre);
    }
}
