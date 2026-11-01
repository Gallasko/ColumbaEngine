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

        constexpr float TIP_DELAY = 0.35f, TIP_FADE = 0.12f;   // s hovering before a card's tooltip shows, s it fades in over

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

        std::string upperFirst(std::string t)
        {
            if (not t.empty())
                t[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(t[0])));
            return t;
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
        std::string name = localName(spell.id);
        if (auto it = content.spells.find(spell.id); it != content.spells.end())
            name = it->second.name;
        else if (auto a = content.abilities.find(spell.id); a != content.abilities.end())
            name = a->second.name;
        return spell.level > 1 ? name + " " + roman(spell.level) : name;
    }

    std::string branchName(const Content& content, const Fighter::RunSpell& spell)
    {
        const std::vector<BranchDef>* branches = nullptr;
        if (auto it = content.spells.find(spell.id); it != content.spells.end())
            branches = &it->second.branches;
        else if (auto a = content.abilities.find(spell.id); a != content.abilities.end())
            branches = &a->second.branches;

        return branches and spell.branch >= 0 and spell.branch < static_cast<int>(branches->size()) ? (*branches)[static_cast<size_t>(spell.branch)].name : "";
    }

    std::string tagLabel(const std::string& tag)
    {
        static const std::map<std::string, std::string> labels = {
            {"fire", "Fire"}, {"ice", "Ice"}, {"volt", "Volt"}, {"projectile", "Projectiles"}, {"area", "Area"}, {"zone", "Zones"},
            {"beam", "Beams"}, {"spell", "Spells"}, {"technique", "Techniques"}, {"attack", "Attacks"}, {"melee", "Melee"}, {"dash", "Dashes"}, {"heal", "Healing"},
        };
        auto it = labels.find(tag);
        if (it != labels.end())
            return it->second;
        std::string t = tag;
        if (not t.empty())
            t[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(t[0])));
        return t;
    }

    std::string boostText(const SpellTweak& t, float k)
    {
        std::vector<std::string> parts;
        auto pc = [&](float v) { return std::to_string(static_cast<int>(std::lround(v * k * 100.0f))) + "%"; };

        if (t.count != 0) parts.push_back("+" + std::to_string(t.count) + (t.count == 1 ? " projectile" : " projectiles"));
        if (t.damage != 0.0f) parts.push_back("+" + pc(t.damage) + " damage");
        if (t.area != 0.0f) parts.push_back("+" + pc(t.area) + " area");
        if (t.cooldown != 0.0f) parts.push_back("-" + pc(t.cooldown) + " cooldown");
        if (t.cost != 0.0f) parts.push_back("-" + pc(t.cost) + " cost");
        if (t.duration != 0.0f) parts.push_back("+" + pc(t.duration) + " duration");
        if (t.speed != 0.0f) parts.push_back("+" + pc(t.speed) + " projectile speed");

        std::string out;
        for (const auto& part : parts)
            out += (out.empty() ? "" : ", ") + part;
        return out;
    }

    namespace
    {
        using Stats = std::vector<std::pair<std::string, float>>;

        float stat(const Stats& stats, const std::string& label)
        {
            for (const auto& [l, v] : stats)
                if (l == label)
                    return v;
            return 0.0f;
        }

        /** A spell's numbers, as the tooltips compare them. */
        Stats spellStats(const Spell& s)
        {
            Stats out;
            if (not s.valid() or s.stages.empty())
                return out;

            float damage = 0.0f, radius = 0.0f, lasts = 0.0f, speed = 0.0f;
            int count = 0;
            for (const auto& st : s.stages)
            {
                damage = std::max(damage, st.damage);
                if (st.form.shape == "nova" or st.form.shape == "zone")
                    radius = std::max(radius, st.radius * 2.0f);   // how wide, as the spell's text says
                if (st.form.shape == "zone")
                    lasts = std::max(lasts, st.duration);
                if (st.form.shape == "bolt" and count == 0)
                {
                    count = st.count;
                    speed = st.speed;
                }
            }

            out.push_back({"damage", damage});
            if (count > 0) out.push_back({"projectiles", static_cast<float>(count)});
            if (speed > 0.0f) out.push_back({"projectile speed", speed});
            if (radius > 0.0f) out.push_back({"area", radius});
            if (lasts > 0.0f) out.push_back({"lasts", lasts});
            out.push_back({"mana", s.mana});
            out.push_back({"cooldown", s.cooldown});
            return out;
        }

        /** A technique's numbers for a player as it stands (its levels, branch, tag items). */
        Stats techniqueStats(const ArenaWorld& world, const Fighter& f, const AbilityDef& a)
        {
            const Fighter::RunSpell* rs = world.runSkill(f, a.id);
            const int branch = rs ? rs->branch : -1;
            const SpellTweak boost = world.techniqueBoost(f, a.id);
            const AttackDef at = world.boostedAttack(a, boost, branch >= 0 and branch < static_cast<int>(a.branches.size()) ? &a.branches[static_cast<size_t>(branch)] : nullptr);

            Stats out;
            const bool shoots = at.projectile.enabled;
            out.push_back({"damage", shoots ? at.projectile.damage : at.damage});
            if (shoots) out.push_back({"projectiles", static_cast<float>(at.projectile.count)});
            if (shoots) out.push_back({"projectile speed", at.projectile.speed});
            if (not at.hitboxes.empty()) out.push_back({"area", at.hitboxes.front().size.x});
            if (at.heal.amount > 0.0f) out.push_back({"heal", at.heal.amount});
            if (at.healZone.perSecond > 0.0f) out.push_back({"heal a second", at.healZone.perSecond});
            if (at.healZone.radius > 0.0f) out.push_back({"area", at.healZone.radius});
            if (at.healZone.duration > 0.0f) out.push_back({"lasts", at.healZone.duration});
            out.push_back({"stamina", a.staminaCost * std::max(0.2f, 1.0f - boost.cost)});
            out.push_back({"cooldown", a.cooldown * std::max(0.3f, 1.0f - f.boon.cooldown) * std::max(0.2f, 1.0f - boost.cooldown)});
            return out;
        }

        std::string number(const std::string& label, float v)
        {
            if (label == "projectiles" or label == "mana" or label == "stamina")
                return std::to_string(static_cast<int>(std::lround(v)));
            if (label == "cooldown" or label == "lasts")
                return oneDecimal(v) + " s";
            if (label == "area")
                return oneDecimal(v) + " m";
            if (label == "projectile speed")
                return std::to_string(static_cast<int>(std::lround(v))) + " m/s";
            return oneDecimal(v);
        }

        void diff(std::vector<Change>& out, const std::string& who, const Stats& before, const Stats& after)
        {
            for (const auto& [label, v] : after)
            {
                const float was = stat(before, label);
                if (std::fabs(v - was) > 0.004f * std::max(1.0f, std::fabs(was)))
                {
                    const bool lowerIsBetter = label == "mana" or label == "stamina" or label == "cooldown";
                    out.push_back({who + " " + label, number(label, was), number(label, v), lowerIsBetter ? v < was : v > was});
                }
            }
        }

        /** The player's numbers (stat items), as the tooltips compare them. */
        Stats playerStats(const ArenaWorld& world, const Content& content, const Fighter& f)
        {
            const auto& b = f.boon;
            return {
                {"Health", f.maxHealth}, {"Damage %", b.damage * 100.0f}, {"Armor %", std::min(b.armor, 0.7f) * 100.0f}, {"Crit %", b.crit * 100.0f},
                {"Speed %", b.speed * 100.0f}, {"Regen /s", world.healthRegen(f)}, {"Dashes", static_cast<float>(world.maxDashes(f))},
                {"Pickup m", content.run.pickupRange + b.pickupRange}, {"Spell cost -%", b.manaCost * 100.0f}, {"Mana regen %", b.manaRegen * 100.0f},
                {"Stamina regen %", b.staminaRegen * 100.0f}, {"Cooldowns -%", b.cooldown * 100.0f}, {"Life on hit", b.lifeOnHit},
                {"Status stacks", static_cast<float>(b.statusStacks)}, {"Attack speed %", b.attackSpeed * 100.0f},
            };
        }
    }

    std::vector<Change> whatChanges(const Content& content, const ArenaWorld& world, const Fighter& before, const Fighter& after)
    {
        std::vector<Change> out;

        // The player's own numbers
        for (const auto& [label, v] : playerStats(world, content, after))
        {
            const float was = stat(playerStats(world, content, before), label);
            if (std::fabs(v - was) > 1e-3f)
                out.push_back({label, oneDecimal(was), oneDecimal(v), v > was});
        }

        // Each spell and technique
        for (const auto& rs : after.runSpells)
        {
            const auto had = std::find_if(before.runSpells.begin(), before.runSpells.end(), [&](const Fighter::RunSpell& r) { return r.id == rs.id; });
            const Stats now = spellStats(*world.spellOf(world.spellAt(rs, &after)));
            const Stats was = had == before.runSpells.end() ? Stats{} : spellStats(*world.spellOf(world.spellAt(*had, &before)));
            diff(out, runSpellName(content, {rs.id, 1}), was, now);
        }

        // The weapon's attacks: its first light one and its heavy
        if (const Moveset* m = world.movesetOf(after))
        {
            for (const std::string& name : {m->light, m->heavy})
            {
                const AttackDef* was = world.attackDef(before, name);
                const AttackDef* now = world.attackDef(after, name);
                if (not was or not now)
                    continue;

                auto attackStats = [](const AttackDef& a) {
                    Stats st = {{"damage", a.projectile.enabled ? a.projectile.damage : a.damage}};
                    if (a.projectile.enabled) st.push_back({"projectile speed", a.projectile.speed});
                    if (not a.hitboxes.empty()) st.push_back({"area", a.hitboxes.front().size.x});
                    return st;
                };
                diff(out, std::string(name == m->light ? "Light" : "Heavy") + " attack", attackStats(*was), attackStats(*now));
            }
        }

        for (const auto& rs : after.runSkills)
        {
            auto a = content.abilities.find(rs.id);
            if (a == content.abilities.end())
                continue;
            const bool had = world.runSkill(before, rs.id) != nullptr;
            diff(out, a->second.name, had ? techniqueStats(world, before, a->second) : Stats{}, techniqueStats(world, after, a->second));
        }

        return out;
    }

    std::string itemText(const Content& content, const UpgradeDef& u, float k)
    {
        (void)content;
        std::vector<std::string> parts;
        auto pc = [&](float v) { return std::to_string(static_cast<int>(std::lround(v * k * 100.0f))) + "%"; };
        auto num = [&](float v) { return oneDecimal(v * k); };

        if (u.forTag())
        {
            static const std::map<std::string, std::string> who = {
                {"fire", "fire spells and attacks"}, {"ice", "ice spells and attacks"}, {"volt", "lightning spells and attacks"},
                {"projectile", "everything that shoots"}, {"area", "area spells and techniques"}, {"zone", "spells that leave an area"},
                {"beam", "beams"}, {"spell", "all your spells"}, {"technique", "all your techniques"}, {"attack", "your weapon attacks"},
                {"melee", "melee attacks and techniques"}, {"dash", "dash techniques"}, {"heal", "healing"},
            };
            auto it = who.find(u.tag);
            std::string out = boostText(u.boost, k) + " for " + (it != who.end() ? it->second : "everything tagged " + tagLabel(u.tag)) + ".";
            out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
            return out;
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
            if (u.attackSpeed != 0.0f) parts.push_back("+" + pc(u.attackSpeed) + " attack speed (your weapon's light and heavy attacks)");
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
            return {u->second.forTag() ? tagLabel(u->second.tag) : "Stat", u->second.name, itemText(content, u->second, 1.0f)};

        if (auto sp = content.spells.find(id); sp != content.spells.end())
            return {"Spell", sp->second.name, sp->second.text.empty() ? describe(content, compileSpell(content, sp->second.recipe)) : sp->second.text};

        if (auto w = content.weapons.find(id); w != content.weapons.end())
            return {"Weapon", w->second.name, w->second.text.empty() ? "A " + w->second.type + "." : w->second.text};

        if (auto a = content.abilities.find(id); a != content.abilities.end())
        {
            std::string with;
            for (const auto& t : a->second.weapons)
                with += (with.empty() ? "" : " or ") + t;
            if (not a->second.text.empty())
                return {"Skill", a->second.name, a->second.text};
            return {"Skill", a->second.name, "A weapon skill" + (with.empty() ? std::string() : " for the " + with) + ". It uses stamina, not mana."};
        }

        if (auto k = content.keywords.find(id); k != content.keywords.end())
        {
            if (k->second.form)
                return {"Spell form", k->second.name, "Unlocks the " + k->second.name + " form for your spells."};
            return {"Modifier", k->second.name, "Unlocks the " + k->second.name + " modifier for your spells."};
        }

        if (id.rfind("element:", 0) == 0)
        {
            Element e = Element::None;
            const std::string name = id.substr(8);
            const std::string shown = elementFromName(name, e) and content.elements.count(e) ? content.elements.at(e).name : name;
            return {"Element", shown, "Unlocks " + shown + " spells."};
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
                {"plated", "shield"}, {"precise", "target"}, {"magnet", "magnet"}, {"mending", "plus"}, {"frenzy", "keen"},
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

        // The level ups first, then the presents: the shop after
        if (me.levelUps > 0 and not me.levelOffer.empty())
        {
            drawLevelUp(ui, content, world, me);
            return;
        }

        if (me.presents > 0 and not me.presentOffer.empty())
        {
            drawPresent(ui, content, world, me);
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

            // The interest the kept shards earned
            if (me.interest > 0)
                ui.text(x + shardW, 98, "+" + std::to_string(me.interest) + " interest (on " + std::to_string(me.banked) + " saved shards)", Font::Text600, 11, GOOD, Align::Right);
        }

        // What is sold, by rarity
        {
            float lx = 66.0f + ui.text(66, 124, "Chance of each rarity:", Font::Text500, 13, TEXT_DIM) + 10.0f;
            for (int k = 0; k < static_cast<int>(content.run.rarities.size()); ++k)
            {
                const auto itemScope = ui.scope(static_cast<uint64_t>(100 + k));
                const int chance = static_cast<int>(std::lround(content.run.rarityChance(k, r.wave) * 100.0f));
                ui.rect(lx, 128, 8, 8, rarityColour(k));
                lx += 14.0f + ui.text(lx + 14, 124, content.run.rarities[static_cast<size_t>(k)].name + " " + std::to_string(chance) + "%",
                                      Font::Text700, 12, rarityColour(k)) + 14.0f;
            }
        }

        // The offers
        int hovered = -1;
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
            const int had = static_cast<int>(std::count_if(me.boons.begin(), me.boons.end(), [&](const Fighter::Boon& b) { return b.id == o.id; }));
            if (not o.bought and ui.hover(x, y, CARD_W, CARD_H))
                hovered = i;
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
            ui.icon(offerIcon(content, o.id), x + 26, cy + 12, 4.0f, o.bought ? MUTED : kind);
            cy += 52.0f + 10.0f;

            ui.text(x + 14, cy, about.name, Font::Display, 30, main);
            cy += 30.0f + 10.0f;

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
                {"Attack speed", "+" + percent(b.attackSpeed), ""},
                {"Regen", oneDecimal(world.healthRegen(me)) + "/s", ""},
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
                const std::string branch = branchName(content, rs);
                ui.text(x + 34, ry, runSpellName(content, rs) + (branch.empty() ? "" : " \xC2\xB7 " + branch), Font::Text600, 13, TEXT);
                ry += 20.0f;
            }

            if (not me.runSkills.empty())
            {
                ry += 6.0f;
                ui.text(x + 14, ry, "TECHNIQUES", Font::Text700, 11, MUTED, Align::Left, 1.5f);
                ry += 20.0f;

                for (const auto& rs : me.runSkills)
                {
                    const auto itemScope = ui.scope(Ui::idOf("skill " + rs.id));
                    const std::string branch = branchName(content, rs);
                    ui.icon(offerIcon(content, rs.id), x + 14, ry + 2, 2.0f, KIND_SKILL);
                    ui.text(x + 34, ry, runSpellName(content, rs) + (branch.empty() ? "" : " \xC2\xB7 " + branch), Font::Text600, 13, TEXT);
                    ry += 20.0f;
                }
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

        // The card under the mouse: what buying it would change, on top of everything. After a moment
        // of hovering (TIP_DELAY s), fading in (TIP_FADE s)
        const auto now = std::chrono::steady_clock::now();
        if (hovered != hoverCard)
        {
            hoverCard = hovered;
            hoverSince = now;
        }

        const float hoveredFor = std::chrono::duration<float>(now - hoverSince).count();
        const float alpha = std::clamp((hoveredFor - TIP_DELAY) / TIP_FADE, 0.0f, 1.0f);

        if (hovered >= 0 and alpha > 0.0f and content.upgrades.count(me.shop[static_cast<size_t>(hovered)].id))
        {
            const auto& o = me.shop[static_cast<size_t>(hovered)];
            Fighter after = me;
            after.boons.push_back({o.id, o.rarity});
            world.sumBoons(after);
            after.maxHealth = me.maxHealth + (after.boon.maxHealth - me.boon.maxHealth);
            const std::vector<Change> changes = whatChanges(content, world, me, after);
            auto faded = [&](const Colour& c) { return withAlpha(c, alpha * c.a / 255.0f); };

            const auto tipScope = ui.scope(900);
            const float w = 300.0f, line = 20.0f;
            const size_t lines = std::max<size_t>(changes.size(), 1);
            const float h = 44.0f + line * static_cast<float>(std::min<size_t>(lines, 14));
            const float cardX = CARD_X[hovered];
            const float x = hovered < CARDS - 1 ? cardX + CARD_W - 20.0f : cardX - w + 20.0f;
            const float y = CARD_Y + 90.0f;

            ui.rect(x + 3, y + 3, w, h, faded(INK));
            ui.box(x, y, w, h, faded(PANEL), 2.0f, faded(GOLD));
            ui.text(x + 14, y + 12, "IF YOU BUY IT", Font::Text700, 11, faded(GOLD), Align::Left, 1.5f);

            if (changes.empty())
                ui.text(x + 14, y + 36, "No effect on your build yet", Font::Text500, 13, faded(MUTED));

            float ly = y + 36.0f;
            for (size_t k = 0; k < changes.size() and k < 14; ++k)
            {
                const auto lineScope = ui.scope(1000 + k);
                const Change& c = changes[k];
                ui.text(x + 14, ly, ui.fit(c.what, Font::Text500, 13, 150.0f), Font::Text500, 13, faded(TEXT_DIM));
                ui.text(x + w - 14, ly - 2, c.after, Font::Display, 18, faded(c.better ? GOOD : WARNING), Align::Right);
                const float aw = ui.measure(c.after, Font::Display, 18);
                ui.text(x + w - 14 - aw - 8, ly, c.before + "  ->", Font::Text600, 12, faded(MUTED), Align::Right);
                ly += line;
            }
        }
    }

    namespace
    {
        /** What a level up's pick is, for its card: a spell or technique, new, its next level, a branch, or mastery. */
        struct PickCard
        {
            std::string kind, name, branch, text, icon;
            Colour colour = KIND_SKILL;
            std::vector<Change> changes;
        };

        PickCard levelCard(const Content& content, const ArenaWorld& world, const Fighter& me, const std::string& offer)
        {
            PickCard card;
            int branch = -1;
            const std::string id = ArenaWorld::offerTarget(offer, branch);
            card.icon = offerIcon(content, id);

            // The player as they would be with it
            Fighter after = me;
            auto grow = [&](std::vector<Fighter::RunSpell>& list) -> Fighter::RunSpell& {
                for (auto& rs : list)
                    if (rs.id == id)
                    {
                        ++rs.level;
                        if (branch >= 0)
                            rs.branch = branch;
                        return rs;
                    }
                list.push_back({id, 1});
                return list.back();
            };

            if (auto sp = content.spells.find(id); sp != content.spells.end())
            {
                const bool isNew = std::none_of(me.runSpells.begin(), me.runSpells.end(), [&](const Fighter::RunSpell& r) { return r.id == id; });
                const Fighter::RunSpell next = grow(after.runSpells);
                const SpellDef& def = sp->second;
                card.colour = elementColour(def.recipe.element);
                card.name = runSpellName(content, next);

                if (isNew)
                {
                    card.kind = "NEW SPELL";
                    card.text = (def.text.empty() ? "" : def.text + " ") + describe(content, *world.spellOf(world.spellAt(next, &after)));
                    return card;
                }

                if (branch >= 0 and branch < static_cast<int>(def.branches.size()))
                {
                    card.kind = "BRANCH \xC2\xB7 LEVEL " + std::to_string(next.level);
                    card.branch = def.branches[static_cast<size_t>(branch)].name;
                    card.text = def.branches[static_cast<size_t>(branch)].text;
                }
                else if (next.level <= def.maxLevel())
                {
                    card.kind = "SPELL LEVEL " + std::to_string(next.level);
                    card.text = describe(content, *world.spellOf(world.spellAt(next, &after)));
                }
                else
                {
                    card.kind = "MASTERY \xC2\xB7 LEVEL " + std::to_string(next.level);
                    card.text = upperFirst(boostText(content.run.mastery)) + ".";
                }
            }
            else if (auto a = content.abilities.find(id); a != content.abilities.end())
            {
                const bool isNew = world.runSkill(me, id) == nullptr;
                const Fighter::RunSpell next = grow(after.runSkills);
                const AbilityDef& def = a->second;
                card.colour = KIND_SKILL;
                card.name = runSpellName(content, next);

                if (isNew)
                {
                    card.kind = "NEW TECHNIQUE";
                    card.text = offerInfo(content, id).text;
                    return card;
                }

                if (branch >= 0 and branch < static_cast<int>(def.branches.size()))
                {
                    card.kind = "BRANCH \xC2\xB7 LEVEL " + std::to_string(next.level);
                    card.branch = def.branches[static_cast<size_t>(branch)].name;
                    card.text = def.branches[static_cast<size_t>(branch)].text;
                }
                else if (next.level - 2 < static_cast<int>(def.levels.size()))
                {
                    card.kind = "TECHNIQUE LEVEL " + std::to_string(next.level);
                    card.text = upperFirst(boostText(def.levels[static_cast<size_t>(next.level - 2)])) + ".";
                }
                else
                {
                    card.kind = "MASTERY \xC2\xB7 LEVEL " + std::to_string(next.level);
                    card.text = upperFirst(boostText(content.run.mastery)) + ".";
                }
            }

            card.changes = whatChanges(content, world, me, after);
            return card;
        }

        /** A pick's card (a level up's, a present's): its kind, icon, name, branch, text, what changes, Take. */
        void pickCard(Ui& ui, const PickCard& card, float x, float y, float w, float h, int index, int action)
        {
            const bool over = ui.hover(x, y, w, h);
            ui.raised(x, y, w, h, CARD, 3.0f, 2.0f, over ? GOLD : card.colour);
            float cy = y + 18.0f;
            ui.rect(x + 18, cy + 2, 8, 8, card.colour);
            ui.text(x + 32, cy, card.kind, Font::Text700, 12, card.colour, Align::Left, 1.5f);
            cy += 28.0f;

            ui.box(x + 18, cy, 56, 56, WELL, 2.0f, card.colour);
            ui.icon(card.icon, x + 31, cy + 13, 4.4f, card.colour);
            cy += 56.0f + 12.0f;

            ui.text(x + 18, cy, ui.fit(card.name, Font::Display, 32, w - 36.0f), Font::Display, 32, TEXT);
            cy += 32.0f + 8.0f;

            if (not card.branch.empty())
            {
                ui.text(x + 18, cy, card.branch, Font::Display, 22, GOLD);
                cy += 22.0f + 8.0f;
            }

            cy += ui.paragraph(x + 18, cy, w - 36.0f, card.text, Font::Text500, 14, TEXT_DIM, 19.6f, 5) + 10.0f;

            // What it changes
            const float bottom = y + h - 18.0f - 44.0f - 10.0f;
            for (size_t k = 0; k < card.changes.size() and cy + 18.0f <= bottom; ++k)
            {
                const auto lineScope = ui.scope(500 + k);
                const Change& c = card.changes[k];
                ui.text(x + 18, cy, ui.fit(c.what, Font::Text500, 12, w - 160.0f), Font::Text500, 12, TEXT_DIM);
                const float aw = ui.text(x + w - 18, cy - 1, c.after, Font::Text700, 13, c.better ? GOOD : WARNING, Align::Right);
                ui.text(x + w - 18 - aw - 8, cy, c.before + "  ->", Font::Text600, 12, MUTED, Align::Right);
                cy += 18.0f;
            }

            // Take it
            const float by = y + h - 18.0f - 44.0f;
            ui.raised(x + 18, by, w - 36, 44, over ? GOLD : CARD_DIM, 3.0f, 2.0f, over ? TEXT : LINE);
            ui.keycap(x + 30, by + 11, 22, std::to_string(index + 1), over ? INK : GOLD, over ? GOLD : GOLD_INK, 18, 0.0f);
            ui.text(x + 64, by + 11, "Take", Font::Text700, 16, over ? GOLD_INK : TEXT);

            ui.hit(x, y, w, h, action);
        }
    }

    void ShopScreen::drawLevelUp(Ui& ui, const Content& content, const ArenaWorld& world, const Fighter& me)
    {
        ui.section("levelup");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.62f));

        ui.anchor(Anchor::Centre);
        ui.text(640, 72, "LEVEL UP", Font::Display, 64, GOLD, Align::Centre, 2.0f);
        ui.text(640, 148, "Lv " + std::to_string(me.level - me.levelUps + 1) + (me.levelUps > 1 ? "  \xC2\xB7  " + std::to_string(me.levelUps) + " picks to make" : "  \xC2\xB7  pick one"),
                Font::Text700, 14, TEXT_DIM, Align::Centre, 1.5f);

        const size_t n = me.levelOffer.size();
        const float w = n > 3 ? 280.0f : 330.0f, h = 440.0f, gap = n > 3 ? 16.0f : 24.0f, y = 180.0f;
        const float startX = 640.0f - (static_cast<float>(n) * w + static_cast<float>(n - 1) * gap) * 0.5f;

        for (size_t i = 0; i < n; ++i)
        {
            const auto itemScope = ui.scope(i);
            const float x = std::round(startX + static_cast<float>(i) * (w + gap));
            pickCard(ui, levelCard(content, world, me, me.levelOffer[i]), x, y, w, h, static_cast<int>(i), InputFrame::LEVEL + static_cast<int>(i));
        }

        ui.text(640, 640, "Press 1-" + std::to_string(n) + " or click to choose. " + (me.presents > 0 ? "Your presents" : "The shop") + " is next.", Font::Text500, 12, FAINT,
                Align::Centre);
    }

    void ShopScreen::drawPresent(Ui& ui, const Content& content, const ArenaWorld& world, const Fighter& me)
    {
        ui.section("present");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.62f));

        ui.anchor(Anchor::Centre);
        ui.text(640, 72, "A PRESENT", Font::Display, 64, GOLD, Align::Centre, 2.0f);
        ui.text(640, 148, std::string("From an elite. Pick one item for free.") + (me.presents > 1 ? "  \xC2\xB7  " + std::to_string(me.presents) + " to open" : ""),
                Font::Text700, 14, TEXT_DIM, Align::Centre, 1.5f);

        const size_t n = me.presentOffer.size();
        const float w = n > 3 ? 280.0f : 330.0f, h = 440.0f, gap = n > 3 ? 16.0f : 24.0f, y = 180.0f;
        const float startX = 640.0f - (static_cast<float>(n) * w + static_cast<float>(n - 1) * gap) * 0.5f;

        for (size_t i = 0; i < n; ++i)
        {
            const auto itemScope = ui.scope(i);
            const auto& o = me.presentOffer[i];
            const float x = std::round(startX + static_cast<float>(i) * (w + gap));

            PickCard card;
            const OfferInfo about = offerInfo(content, o.id);
            const auto item = content.upgrades.find(o.id);
            card.colour = rarityColour(o.rarity);
            card.kind = upper(rarityName(content, o.rarity)) + " " + upper(about.kind);
            card.name = about.name;
            card.icon = offerIcon(content, o.id);
            card.text = item != content.upgrades.end() ? itemText(content, item->second, rarityPower(content, o.rarity)) : about.text;

            Fighter after = me;
            after.boons.push_back({o.id, o.rarity});
            world.sumBoons(after);
            after.maxHealth = me.maxHealth + (after.boon.maxHealth - me.boon.maxHealth);
            card.changes = whatChanges(content, world, me, after);

            pickCard(ui, card, x, y, w, h, static_cast<int>(i), InputFrame::PRESENT + static_cast<int>(i));
        }

        ui.text(640, 640, "Press 1-" + std::to_string(n) + " or click to choose. The shop is next.", Font::Text500, 12, FAINT, Align::Centre);
    }
}
