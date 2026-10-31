#include "stdafx.h"

#include "hud.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace arena
{
    using namespace ui;

    namespace
    {
        const Colour WAVE_TOTAL = hex(0xAEB4C2);
        const Colour SCORE_TEXT = hex(0xDDE0E8);

        std::string oneDecimal(float v)
        {
            std::ostringstream o;
            o.setf(std::ios::fixed);
            o.precision(1);
            o << std::max(0.0f, v);
            return o.str();
        }

        std::string clock(float seconds)
        {
            const int s = static_cast<int>(std::ceil(std::max(0.0f, seconds)));
            return std::to_string(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + std::to_string(s % 60);
        }

        /** A status's squares: one per stack, in its colour (frozen: white, ice, white). */
        std::vector<Colour> statusSquares(const Hud::Status& st)
        {
            if (st.id == "frozen")
                return {TEXT, ICE, TEXT};

            Colour c = TEXT_DIM;
            if (st.id == "burn") c = FIRE;
            if (st.id == "chill") c = ICE;
            if (st.id == "shock") c = VOLT;
            return std::vector<Colour>(static_cast<size_t>(std::clamp(st.stacks, 1, 4)), c);
        }

        Colour statusText(const Hud::Status& st)
        {
            if (st.id == "burn") return FIRE_TEXT;
            if (st.id == "chill" or st.id == "frozen") return ICE;
            if (st.id == "shock") return VOLT;
            return TEXT_DIM;
        }

        /** An element's colour, dimmed for a key cooling down (fire: #C9876F). */
        Colour dimmed(const Colour& c)
        {
            return Colour(c.r * 0.6f + 0x9A * 0.4f, c.g * 0.6f + 0x9A * 0.4f, c.b * 0.6f + 0x9A * 0.4f, c.a);
        }
    }

    // ---- The frame ------------------------------------------------------------------------------

    void Hud::draw(Ui& ui, const View& v, float dt)
    {
        // Moments that need the last frame: a level up, the badge, a spell coming off cooldown
        if (v.player and v.run and lastLevel > 0 and v.level > lastLevel)
            levelUpAge = 0.0f;
        lastLevel = v.run ? v.level : 0;

        if (v.levelUps != lastLevelUps)
        {
            if (v.levelUps > lastLevelUps)
                badgeAge = 0.0f;
            lastLevelUps = v.levelUps;
        }

        levelUpAge += dt;
        badgeAge += dt;
        for (auto& f : falling)
            f.age += dt;

        lastSpellState.resize(v.spells.size(), 0);
        spellFlash.resize(v.spells.size(), 0);
        for (size_t i = 0; i < v.spells.size(); ++i)
        {
            const int state = static_cast<int>(v.spells[i].state);
            if (lastSpellState[i] == static_cast<int>(SpellKey::State::Cooldown) and state == static_cast<int>(SpellKey::State::Ready))
                spellFlash[i] = 2;
            else if (spellFlash[i] > 0)
                --spellFlash[i];
            lastSpellState[i] = state;
        }

        ui.section("hud-wash");
        ui.anchor(Anchor::TopLeft);

        if (v.hurt > 0.0f)
            ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(HEALTH, 0.35f * std::clamp(v.hurt, 0.0f, 1.0f)));

        if (v.player and v.alive and v.health <= v.maxHealth * 0.3f)
        {
            const float a = 0.75f + 0.25f * std::sin(v.time * 2.0f * 3.14159265f * 2.0f);
            const float t = std::round(8.0f * ui.scale());
            const Colour c = withAlpha(HEALTH, a);
            ui.rectPx(0, 0, ui.width(), t, c);
            ui.rectPx(0, ui.height() - t, ui.width(), ui.height(), c);
            ui.rectPx(0, t, t, ui.height() - t, c);
            ui.rectPx(ui.width() - t, t, ui.width(), ui.height() - t, c);
        }

        if (v.boss and v.enraged)
        {
            const float t = std::round(6.0f * ui.scale());
            ui.rectPx(0, 0, ui.width(), t, BOSS);
            ui.rectPx(0, ui.height() - t, ui.width(), ui.height(), BOSS);
        }

        ui.section("hud-enemies");
        drawEnemies(ui, v);
        ui.section("hud-markers");
        drawMarkers(ui, v);

        ui.section("hud");
        if (v.player and v.alive and v.crosshair)
        {
            drawCrosshair(ui);

            // A draw: five pips under the crosshair, filling, gold when full
            if (v.draw >= 0.0f)
            {
                ui.anchor(Anchor::Centre);
                const int full = static_cast<int>(std::floor(std::clamp(v.draw, 0.0f, 1.0f) * 5.0f + 0.001f));
                for (int k = 0; k < 5; ++k)
                    ui.box(624.0f + static_cast<float>(k) * 7.0f, 378, 6, 5, k < full ? (full == 5 ? GOLD : TEXT) : EMPTY_PIP, 1.0f);
            }
        }

        if (v.run)
            drawRun(ui, v);
        if (v.boss)
            drawBoss(ui, v);

        if (v.player)
        {
            drawBody(ui, v);
            drawAbilities(ui, v);
        }

        ui.section("hud-moments");
        drawMoments(ui, v);
    }

    // ---- 4.1, 4.2, 4.3: the run -------------------------------------------------------------------

    void Hud::drawRun(Ui& ui, const View& v)
    {
        // Top left: the wave, its time, its pips
        ui.anchor(Anchor::TopLeft);
        const float base = 14.0f + ui.ascent(Font::Display, 46);

        float x = 70.0f;
        if (v.duel)
        {
            ui.textOn(24, base - 3, "FINAL", Font::Text700, 12, TEXT, Align::Left, 2.0f, 2.0f);
            x = 80.0f + ui.textOn(80, base, "DUEL", Font::Display, 46, BOSS, Align::Left, 1.0f, 3.0f) + 14.0f;
        }
        else
        {
            ui.textOn(24, base - 3, "WAVE", Font::Text700, 12, TEXT, Align::Left, 2.0f, 2.0f);
            x += ui.textOn(x, base, std::to_string(v.wave), Font::Display, 46, TEXT, Align::Left, 0.0f, 3.0f);
            x += 4.0f;
            x += ui.textOn(x, base, "/" + std::to_string(v.waves), Font::Display, 24, WAVE_TOTAL, Align::Left, 0.0f, 2.0f);
            x += 14.0f;
        }

        if (v.bossWave and not v.duel)
            ui.textOn(x, base, "BOSS", Font::Display, 40, BOSS, Align::Left, 1.0f, 2.0f);
        else
            ui.textOn(x, base, clock(v.timer), Font::Display, 40, v.timer <= 5.0f ? GOLD : TEXT, Align::Left, 0.0f, 2.0f);

        for (int i = 0; i < v.waves; ++i)
        {
            const float px = 24.0f + static_cast<float>(i) * 11.0f;
            Colour fill = withAlpha(INK, 0.45f);
            if (v.duel)
                fill = TEXT;
            else if (i < v.wave - 1)
                fill = TEXT;
            else if (i == v.wave - 1)
                fill = v.bossWave ? BOSS : GOLD;
            ui.box(px, 62, 8, 8, fill, 2.0f);
        }

        // The team, in co-op
        float ty = 92.0f;
        for (const auto& m : v.team)
        {
            const auto itemScope = ui.scope(Ui::idOf(m.name));
            ui.textShadow(24, ty, m.name, Font::Text700, 12, m.down ? MUTED : TEXT, 2.0f);
            const int pips = m.down ? 0 : static_cast<int>(std::ceil(std::clamp(m.health, 0.0f, 1.0f) * 10.0f - 0.001f));
            for (int k = 0; k < 10; ++k)
                ui.rect(82.0f + static_cast<float>(k) * 7.0f, ty + 3, 6, 6, k < pips ? HEALTH : EMPTY_PIP);
            if (m.down)
                ui.textShadow(156, ty - 1, "DOWN \xC2\xB7 back next wave", Font::Display, 15, WARNING, 2.0f);
            ty += 18.0f;
        }

        // Top right: shards, the level and its +N, then the score and the XP
        ui.anchor(Anchor::TopRight);
        const float b1 = 14.0f + ui.ascent(Font::Display, 38);
        float right = 1256.0f;

        if (v.levelUps > 0)
        {
            const float pop = badgeAge < 0.2f ? 1.4f - 0.4f * (badgeAge / 0.2f) : 1.0f;
            const float size = 22.0f * pop;
            const std::string label = "+" + std::to_string(v.levelUps);
            const float bw = std::ceil(ui.measure(label, Font::Display, size)) + 10.0f;
            const float bh = std::round(size + 2.0f);
            ui.rect(right - bw + 2, b1 - bh + 4, bw, bh, INK);
            ui.rect(right - bw, b1 - bh + 2, bw, bh, GOLD);
            ui.text(right - bw * 0.5f, b1 - bh + 2 + (bh - size) * 0.5f - 1.0f, label, Font::Display, size, GOLD_INK, Align::Centre);
            right -= bw + 6.0f;
        }

        right -= ui.textOn(right, b1, "LV " + std::to_string(v.level), Font::Display, 34, TEXT, Align::Right, 0.0f, 2.0f);
        right -= 18.0f;
        right -= ui.textOn(right, b1, std::to_string(v.shards), Font::Display, 38, SHARDS, Align::Right, 0.0f, 2.0f);
        right -= 7.0f + 28.0f;
        ui.icon("shard", right, b1 - 27.0f, 4.0f, SHARDS, 2.0f);

        const float b2 = 58.0f + ui.ascent(Font::Text600, 12);
        right = 1256.0f;
        right -= ui.textOn(right, b2, std::to_string(v.xp) + "/" + std::to_string(v.xpNext), Font::Text600, 12, SCORE_TEXT, Align::Right, 0.0f, 2.0f);
        right -= 10.0f;

        const int xpPips = static_cast<int>(std::floor(std::clamp(static_cast<float>(v.xp) / static_cast<float>(std::max(1, v.xpNext)), 0.0f, 1.0f) * 12.0f));
        right -= 11.0f * 11.0f + 9.0f;
        for (int k = 0; k < 12; ++k)
            ui.box(right + static_cast<float>(k) * 11.0f, b2 - 7.0f, 9, 6, k < xpPips ? GOLD : EMPTY_PIP, 1.0f);
        right -= 10.0f;
        ui.textOn(right, b2, "SCORE " + std::to_string(v.score), Font::Text600, 12, SCORE_TEXT, Align::Right, 0.0f, 2.0f);
    }

    // ---- 4.4: the boss --------------------------------------------------------------------------

    void Hud::drawBoss(Ui& ui, const View& v)
    {
        ui.anchor(Anchor::Top);

        const float nameW = ui.textShadow(640, 18, v.bossName, Font::Display, 34, v.enraged ? BOSS_NAME_ENRAGED : TEXT, 3.0f, Align::Centre, 2.0f);

        if (v.enraged)
        {
            const float tx = 640.0f + nameW * 0.5f + 10.0f;
            const float tw = std::ceil(ui.measure("ENRAGED", Font::Display, 18)) + 12.0f;
            ui.rect(tx, 25, tw, 20, BOSS);
            ui.text(tx + 6, 26, "ENRAGED", Font::Display, 18, BOSS_TAG_INK);
        }

        ui.rect(339, 53, 602, 18, INK);
        const Colour full = v.enraged ? BOSS_ENRAGED : BOSS;
        const float filled = std::clamp(v.bossHealth, 0.0f, 1.0f) * 20.0f;

        for (int i = 0; i < 20; ++i)
        {
            const float x = 341.0f + static_cast<float>(i) * 30.0f;
            const float part = std::clamp(filled - static_cast<float>(i), 0.0f, 1.0f);
            ui.rect(x, 55, 28, 14, BOSS_EMPTY);
            if (part > 0.0f)
                ui.rect(x, 55, std::max(2.0f, std::round(28.0f * part)), 14, full);
        }

        ui.rect(639, 49, 2, 26, TEXT);
        ui.rect(339, 75, 602, 3, withAlpha(INK, 0.7f));
        ui.rect(339, 75, std::round(602.0f * std::clamp(v.bossPoise, 0.0f, 1.0f)), 3, POISE);
    }

    // ---- 4.5: the body --------------------------------------------------------------------------

    void Hud::drawBody(Ui& ui, const View& v)
    {
        ui.anchor(Anchor::BottomLeft);

        // Weapons, the one in hand underlined, then the key that changes it
        float x = 24.0f;
        for (const auto& [name, current] : v.weapons)
        {
            const auto itemScope = ui.scope(Ui::idOf(name));
            const float tw = ui.textShadow(x, 600, name, Font::Text600, 14, current ? GOLD : TEXT_DIM, 2.0f);
            if (current)
            {
                ui.rect(x + 2, 620, tw, 3, INK);
                ui.rect(x, 618, tw, 3, GOLD);
            }
            x += tw + 16.0f;
        }

        if (v.weapons.size() > 1)
        {
            const float kw = std::ceil(ui.measure(v.nextWeaponKey, Font::Display, 15)) + 12.0f;
            ui.frame(x + 2, 601, kw, 18, 2, INK);
            ui.frame(x, 599, kw, 18, 2, TEXT);
            ui.text(x + kw * 0.5f, 599, v.nextWeaponKey, Font::Display, 15, TEXT, Align::Centre);
        }

        // Health: the number, then a pip per tenth
        const float healthBase = 628.0f + ui.ascent(Font::Display, 50);
        ui.textOn(86, healthBase - 4.0f, std::to_string(static_cast<int>(std::ceil(std::max(0.0f, v.health)))), Font::Display, 50, HEALTH_NUMBER, Align::Right, 0.0f, 3.0f);

        const float health = std::clamp(v.health / std::max(1.0f, v.maxHealth), 0.0f, 1.0f);
        const int pips = static_cast<int>(std::ceil(health * 10.0f - 0.001f));

        if (lastPips >= 0 and pips < lastPips)
            for (int k = pips; k < lastPips; ++k)
                falling.push_back({k, 0.0f});
        lastPips = pips;

        for (int k = 0; k < 10; ++k)
            ui.raised(96.0f + static_cast<float>(k) * 25.0f, 640, 22, 16, k < pips ? HEALTH : EMPTY_PIP, 2.0f);

        // Stamina and mana: the pip being refilled partly drawn
        auto row = [&](float y, float h, float value, const Colour& c) {
            const float units = std::clamp(value, 0.0f, 1.0f) * 10.0f;
            for (int k = 0; k < 10; ++k)
            {
                const float px = 96.0f + static_cast<float>(k) * 25.0f;
                const float part = std::clamp(units - static_cast<float>(k), 0.0f, 1.0f);
                ui.raised(px, y, 22, h, EMPTY_PIP, 2.0f);
                if (part > 0.0f)
                    ui.rect(px + 2, y + 2, std::max(1.0f, std::round(18.0f * part)), h - 4.0f, c);
            }
        };

        row(662, 9, v.stamina, v.exhausted ? EXHAUSTED : STAMINA);
        row(675, 9, v.mana, v.manaLow ? MANA_LOW : MANA);

        // Dash charges: full, refilling (rising from the bottom), empty
        for (int k = 0; k < v.maxDashes; ++k)
        {
            const float px = 352.0f + static_cast<float>(k) * 17.0f;
            const float part = std::clamp(v.dashes - static_cast<float>(k), 0.0f, 1.0f);
            ui.raised(px, 662, 14, 22, part >= 1.0f ? TEXT : EMPTY_PIP, 2.0f);
            if (part > 0.0f and part < 1.0f)
            {
                const float fh = std::round(18.0f * part);
                ui.rect(px + 2, 664 + 18 - fh, 10, fh, COOLING);
            }
        }

        // Under the bars: exhausted, or what the player has on them
        if (v.exhausted)
        {
            ui.textShadow(96, 689, "EXHAUSTED", Font::Display, 17, EXHAUSTED_TEXT, 2.0f, Align::Left, 1.0f);
        }
        else
        {
            float sx = 96.0f;
            for (const auto& st : v.statuses)
            {
                const auto itemScope = ui.scope(Ui::idOf(st.id));
                for (const auto& c : statusSquares(st))
                {
                    ui.rect(sx + 1, 694, 8, 8, INK);
                    ui.rect(sx, 693, 8, 8, c);
                    sx += 10.0f;
                }
                sx += 2.0f;
                sx += ui.textShadow(sx, 691, st.name, Font::Text700, 11, statusText(st), 2.0f) + 12.0f;
            }
        }
    }

    // ---- 4.6: abilities -------------------------------------------------------------------------

    void Hud::drawAbilities(Ui& ui, const View& v)
    {
        ui.anchor(Anchor::BottomRight);

        // The skills row: terrain, then the weapon's
        {
            struct Item { float w, keyW, nameW, rightW; std::string right; };
            std::vector<Item> items;
            float total = 0.0f;

            for (const auto& s : v.skills)
            {
                Item it;
                it.nameW = ui.measure(s.name, Font::Text600, 14);
                it.keyW = std::max(24.0f, std::ceil(ui.measure(s.key, Font::Display, 20)) + 10.0f);   // a long key (LMB, Space) widens its cap
                if (s.charging)
                    it.right = "hold";
                else if (s.progress < 1.0f)
                    it.right = oneDecimal(s.seconds);
                else if (s.noStamina)
                    it.right = std::to_string(s.cost);
                // Room for whatever it may show there (seconds, "hold", its cost): the row does not move when it does
                it.rightW = 6.0f + std::max({ui.measure("88.8", Font::Display, 20), ui.measure("hold", Font::Display, 20),
                                             ui.measure(std::to_string(s.cost), Font::Display, 20)});
                it.w = it.keyW + 6.0f + it.nameW + it.rightW;
                total += it.w;
                items.push_back(it);
            }
            total += 14.0f * static_cast<float>(std::max<size_t>(1, items.size()) - 1);

            float x = 1256.0f - total;
            for (size_t i = 0; i < v.skills.size(); ++i)
            {
                const auto itemScope = ui.scope(i);
                const Skill& s = v.skills[i];
                const Item& it = items[i];
                const bool cooling = s.charging or s.progress < 1.0f;
                const bool ready = not cooling and not s.noStamina;

                const float kw = it.keyW;
                ui.rect(x + 2, 632, kw, 24, INK);
                if (cooling)
                {
                    ui.box(x, 630, kw, 24, INK);
                    const float fh = std::round(20.0f * std::clamp(s.progress, 0.0f, 1.0f));
                    if (fh > 0.0f)
                        ui.rect(x + 2, 632 + 20 - fh, kw - 4.0f, fh, TEXT);
                    ui.text(x + kw * 0.5f, 631, s.key, Font::Display, 20, fh > 10.0f ? INK : TEXT, Align::Centre);
                }
                else
                {
                    ui.box(x, 630, kw, 24, TEXT, 2.0f, s.noStamina ? EXHAUSTED : INK);
                    ui.text(x + kw * 0.5f, 631, s.key, Font::Display, 20, INK, Align::Centre);
                }

                const float base = 630.0f + 17.0f;
                ui.textOn(x + kw + 6, base, s.name, Font::Text600, 14, ready ? TEXT : COOLING, Align::Left, 0.0f, 2.0f);
                if (not it.right.empty())
                    ui.textOn(x + kw + 6 + it.nameW + 6, base + 1, it.right, Font::Display, 20, s.noStamina and not cooling ? EXHAUSTED_TEXT : TEXT, Align::Left, 0.0f, 2.0f);

                x += it.w + 14.0f;
            }
        }

        // The spells row
        {
            struct Item { float w, keyW, nameW; std::string right; };
            std::vector<Item> items;
            float total = 0.0f;

            for (const auto& s : v.spells)
            {
                Item it;
                it.nameW = ui.measure(s.name, Font::Text700, 15);
                it.keyW = std::max(28.0f, std::ceil(ui.measure(s.key, Font::Display, 22)) + 10.0f);
                switch (s.state)
                {
                    case SpellKey::State::Ready: case SpellKey::State::NoMana: it.right = std::to_string(s.mana); break;
                    case SpellKey::State::Cooldown: it.right = oneDecimal(s.seconds); break;
                    case SpellKey::State::Locked: it.right = "locked"; break;
                }
                // Room for its cost or its seconds (and "locked" when it is): the row does not move as they come and go
                const float room = std::max({ui.measure(std::to_string(s.mana), Font::Display, 20), ui.measure("88.8", Font::Display, 20),
                                             s.state == SpellKey::State::Locked ? ui.measure("locked", Font::Display, 20) : 0.0f});
                it.w = it.keyW + 6.0f + it.nameW + 6.0f + room;
                total += it.w;
                items.push_back(it);
            }
            total += 16.0f * static_cast<float>(std::max<size_t>(1, items.size()) - 1);

            float x = 1256.0f - total;
            for (size_t i = 0; i < v.spells.size(); ++i)
            {
                const auto itemScope = ui.scope(i);
                const SpellKey& s = v.spells[i];
                const Item& it = items[i];
                const Colour el = elementColour(s.element);
                const Colour elText = elementText(s.element);
                const bool flash = i < spellFlash.size() and spellFlash[i] > 0;

                Colour nameColour = elText, rightColour = MANA_TEXT, keyColour = INK;
                const float kw = it.keyW;
                ui.rect(x + 2, 666, kw, 28, INK);

                switch (s.state)
                {
                    case SpellKey::State::Ready:
                        ui.box(x, 664, kw, 28, flash ? TEXT : el);
                        break;

                    case SpellKey::State::Cooldown:
                    {
                        ui.box(x, 664, kw, 28, INK);
                        const float fh = std::round(24.0f * std::clamp(s.progress, 0.0f, 1.0f));
                        if (fh > 0.0f)
                            ui.rect(x + 2, 666 + 24 - fh, kw - 4.0f, fh, el);
                        keyColour = fh > 12.0f ? INK : TEXT;
                        nameColour = dimmed(elText);
                        rightColour = TEXT;
                        break;
                    }

                    case SpellKey::State::NoMana:
                        ui.box(x, 664, kw, 28, MANA_LOW, 2.0f, WARNING);
                        keyColour = TEXT;
                        rightColour = WARNING;
                        break;

                    case SpellKey::State::Locked:
                        ui.box(x, 664, kw, 28, withAlpha(INK, 0.5f), 2.0f, FAINT);
                        keyColour = MUTED;
                        nameColour = MUTED;
                        rightColour = MUTED;
                        break;
                }

                ui.text(x + kw * 0.5f, 666, s.key, Font::Display, 22, keyColour, Align::Centre);

                const float base = 664.0f + 20.0f;
                ui.textOn(x + kw + 6, base, s.name, Font::Text700, 15, nameColour, Align::Left, 0.0f, 2.0f);
                ui.textOn(x + kw + 6 + it.nameW + 6, base + 1, it.right, Font::Display, 20, rightColour, Align::Left, 0.0f, 2.0f);

                x += it.w + 16.0f;
            }
        }
    }

    // ---- 4.7: the crosshair ---------------------------------------------------------------------

    void Hud::drawCrosshair(Ui& ui)
    {
        ui.anchor(Anchor::Centre);

        struct R { float x, y, w, h; };
        const R parts[] = {
            {630, 350, 6, 2}, {630, 350, 2, 6},
            {644, 350, 6, 2}, {648, 350, 2, 6},
            {630, 368, 6, 2}, {630, 364, 2, 6},
            {644, 368, 6, 2}, {648, 364, 2, 6},
            {639, 359, 2, 2},
        };

        for (const R& r : parts)
            ui.rect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, INK);
        for (const R& r : parts)
            ui.rect(r.x, r.y, r.w, r.h, TEXT);
    }

    // ---- 4.8: the enemies' bars -----------------------------------------------------------------

    void Hud::drawEnemies(Ui& ui, const View& v)
    {
        ui.anchor(Anchor::TopLeft);

        for (const auto& e : v.enemies)
        {
            const auto itemScope = ui.scope(static_cast<uint64_t>(&e - v.enemies.data()));
            const float cx = std::round(ui.baseX(e.head.x));
            const float top = std::round(ui.baseY(e.head.y)) - 6.0f - 11.0f;
            const float x = cx - 33.0f;

            // A player: the name over them; an ally's health under it, yours not at all (it is at the bottom)
            if (e.self or e.ally)
            {
                ui.textShadow(cx, top - (e.self ? -2.0f : 15.0f), e.name, Font::Text700, 12, e.self ? TEXT_DIM : TEXT, 2.0f, Align::Centre);
                if (e.ally)
                {
                    const int cubes = static_cast<int>(std::ceil(std::clamp(e.health, 0.0f, 1.0f) * 8.0f - 0.001f));
                    ui.rect(x, top, 66, 8, INK);
                    for (int k = 0; k < 8; ++k)
                        ui.rect(x + 1 + static_cast<float>(k) * 8.0f, top + 1, 7, 6, k < cubes ? STAMINA : withAlpha(INK, 0.65f));
                }
                continue;
            }

            ui.rect(x, top, 66, 8, e.elite ? GOLD : (e.boss ? BOSS : INK));

            const int cubes = static_cast<int>(std::ceil(std::clamp(e.health, 0.0f, 1.0f) * 8.0f - 0.001f));
            for (int k = 0; k < 8; ++k)
                ui.rect(x + 1 + static_cast<float>(k) * 8.0f, top + 1, 7, 6, k < cubes ? (e.boss ? BOSS : HEALTH) : withAlpha(INK, 0.65f));

            ui.rect(x, top + 9, 66, 2, withAlpha(INK, 0.7f));
            const float pw = std::round(66.0f * std::clamp(e.poise, 0.0f, 1.0f));
            if (pw > 0.0f)
                ui.rect(x, top + 9, pw, 2, POISE);

            if (e.staggered)
                ui.textShadow(cx, top - 20, "STAGGERED", Font::Display, 16, POISE, 2.0f, Align::Centre, 1.0f);
            else if (not e.name.empty())
                ui.textShadow(cx, top - 17, e.name, Font::Text700, 11, TEXT, 2.0f, Align::Centre);

            float sx = x + 70.0f;
            for (const auto& st : e.statuses)
            {
                const auto itemScope = ui.scope(Ui::idOf(st.id));
                for (const auto& c : statusSquares(st))
                {
                    ui.rect(sx - 1, top - 1, 10, 10, INK);
                    ui.rect(sx, top, 8, 8, c);
                    sx += 10.0f;
                }
            }
        }
    }

    // ---- 4.9: off-screen markers ----------------------------------------------------------------

    void Hud::drawMarkers(Ui& ui, const View& v)
    {
        for (const auto& m : v.markers)
        {
            const auto itemScope = ui.scope(static_cast<uint64_t>(&m - v.markers.data()));
            const Colour c = m.kind == Marker::Kind::Boss ? BOSS : (m.kind == Marker::Kind::Elite ? GOLD : WARNING);
            const float y = std::clamp(m.y, 80.0f, 560.0f) - 17.0f;

            ui.anchor(m.right ? Anchor::Right : Anchor::Left);
            const float x = m.right ? 1280.0f - 12.0f - 35.0f : 12.0f;
            ui.icon(m.right ? "arrow_right" : "arrow_left", x, y, 5.0f, c, 2.0f);

            if (m.kind == Marker::Kind::Boss and not m.name.empty())
            {
                const std::string dist = std::to_string(static_cast<int>(std::round(m.distance))) + " m";
                if (m.right)
                {
                    ui.textShadow(x - 8, y + 3, m.name, Font::Display, 18, c, 2.0f, Align::Right);
                    ui.textShadow(x - 8, y + 21, dist, Font::Text700, 11, TEXT_DIM, 2.0f, Align::Right);
                }
                else
                {
                    ui.textShadow(x + 43, y + 3, m.name, Font::Display, 18, c, 2.0f);
                    ui.textShadow(x + 43, y + 21, dist, Font::Text700, 11, TEXT_DIM, 2.0f);
                }
            }
        }
    }

    // ---- 4.10: moments --------------------------------------------------------------------------

    void Hud::drawMoments(Ui& ui, const View& v)
    {
        // The pips knocked off the health row fall and fade
        ui.anchor(Anchor::BottomLeft);
        for (const auto& f : falling)
        {
            const float t = std::clamp(f.age / 0.4f, 0.0f, 1.0f);
            ui.rect(96.0f + static_cast<float>(f.pip) * 25.0f, 640.0f + 20.0f * t, 22, 16, withAlpha(HEALTH, 0.6f * (1.0f - t)));
        }
        falling.erase(std::remove_if(falling.begin(), falling.end(), [](const Falling& f) { return f.age >= 0.4f; }), falling.end());

        ui.anchor(Anchor::Centre);

        // A name with a count down under it (the duel's): the name goes up out of its way
        if (not v.headline.empty())
            ui.textShadow(640, (v.countdown > 0 ? 112.0f : 200.0f) - 28.0f, v.headline, Font::Display, 56, TEXT, 3.0f, Align::Centre, 2.0f);

        if (levelUpAge < 1.0f and v.run)
            ui.textShadow(640, (v.headline.empty() ? 200.0f : 262.0f) - 28.0f, "LEVEL UP", Font::Display, 56, GOLD, 3.0f, Align::Centre, 2.0f);

        if (v.countdown > 0)
            ui.textShadow(640, 190 - 48, std::to_string(v.countdown), Font::Display, 96, GOLD, 4.0f, Align::Centre);

        if (not v.message.empty())
        {
            if (v.messageDim)
                ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.55f));
            ui.textShadow(640, 300 - 28, v.message, Font::Display, v.messageDim ? 56.0f : 44.0f, TEXT, 3.0f, Align::Centre, 1.0f);
        }
    }

    // ---- 11: developer layers ---------------------------------------------------------------------

    void Hud::devLayers(Ui& ui, const std::string& overlay, bool console, const std::vector<std::string>& lines, const std::string& input, const std::string& banner)
    {
        ui.section("dev");
        ui.anchor(Anchor::TopLeft);

        if (not overlay.empty())
        {
            const int count = 1 + static_cast<int>(std::count(overlay.begin(), overlay.end(), '\n'));
            const float tw = ui.measure(overlay, Font::Mono, 12);
            ui.rect(8, 8, tw + 16.0f, static_cast<float>(count) * 14.6f + 12.0f, withAlpha(INK, 0.7f));
            ui.text(16, 14, overlay, Font::Mono, 12, TEXT);
        }

        if (console)
        {
            ui.anchor(Anchor::BottomLeft);
            const float top = ui.Y(490);
            ui.rectPx(0, top, ui.width(), ui.height(), withAlpha(INK, 0.92f));
            ui.rectPx(0, top, ui.width(), top + std::round(2.0f * ui.scale()), GOLD);

            constexpr size_t SHOWN = 11;
            const size_t from = lines.size() > SHOWN ? lines.size() - SHOWN : 0;
            std::string text;
            for (size_t i = from; i < lines.size(); ++i)
                text += lines[i] + (i + 1 < lines.size() ? "\n" : "");

            ui.text(12, 500, text, Font::Mono, 13, TEXT);
            ui.text(12, 694, ">", Font::Mono, 13, GOLD);
            ui.text(26, 694, input + "_", Font::Mono, 13, TEXT);
        }

        if (not banner.empty())
        {
            ui.anchor(Anchor::TopLeft);
            ui.rectPx(0, 0, ui.width(), std::round(28.0f * ui.scale()), HEALTH);
            ui.text(12, 7, banner, Font::Text700, 13, TEXT);
        }
    }
}
