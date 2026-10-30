#include "stdafx.h"

#include "screens.h"

#include <cmath>

namespace arena::screens
{
    using namespace ui;

    namespace
    {
        const Colour ARENA_LINE = hex(0x5A4718);
        const Colour SLOT_DASH = hex(0x4A4F5C);
        const Colour SUBTITLE = hex(0xDDE0E8);
        const Colour DEFEAT_WASH = hex(0x280A0A, 0.6f);
    }

    std::string grouped(int value)
    {
        std::string digits = std::to_string(std::abs(value));
        std::string out;

        for (size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 and (digits.size() - i) % 3 == 0)
                out += ' ';
            out += digits[i];
        }

        return (value < 0 ? "-" : "") + out;
    }

    void drawTitle(Ui& ui, const TitleView& v)
    {
        ui.section("title");

        // A dark band on the left, ending in two hard steps
        ui.anchor(Anchor::Left);
        ui.rectPx(0, 0, ui.X(620), ui.height(), withAlpha(INK, 0.82f));
        ui.rectPx(ui.X(620), 0, ui.X(628), ui.height(), withAlpha(INK, 0.6f));
        ui.rectPx(ui.X(628), 0, ui.X(636), ui.height(), withAlpha(INK, 0.35f));

        // The logo: a row of blocks in the game's colours, then the name
        const Colour blocks[] = {HEALTH, STAMINA, MANA, GOLD, SHARDS, FIRE, ICE, VOLT};
        for (int k = 0; k < 8; ++k)
            ui.box(72.0f + static_cast<float>(k) * 23.0f, 72, 18, 18, blocks[k]);

        ui.textShadow(72, 96, "VOXEL", Font::Display, 112, TEXT, 5.0f, Align::Left, 2.0f);
        ui.textShadow(72, 186, "ARENA", Font::Display, 112, GOLD, 5.0f, Align::Left, 2.0f);

        // The arena (or waiting for the host)
        if (v.waitingForHost)
        {
            ui.rect(72, 318, 472, 78, CARD);
            ui.dashed(72, 318, 472, 78, 2, 6, SLOT_DASH);
            for (int k = 0; k < 3; ++k)
            {
                const bool lit = static_cast<int>(v.time * 3.0f) % 3 == k;
                ui.rect(92.0f + static_cast<float>(k) * 16.0f, 336, 10, 10, lit ? GOLD : LINE);
            }
            ui.text(146, 326, "Waiting for the host\xE2\x80\xA6", Font::Display, 30, TEXT);
            ui.text(92, 364, std::to_string(v.players) + " of " + std::to_string(v.maxPlayers) + " players in \xC2\xB7 only the host can start", Font::Text500, 13, MUTED);
        }
        else
        {
            const bool over = ui.hover(72, 318, 472, 78);
            ui.raised(72, 318, 472, 78, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
            const float kw = ui.measure("ENTER", Font::Display, 18) + 12.0f;
            ui.box(90, 332, kw, 26, INK, 0.0f);
            ui.text(90 + kw * 0.5f, 335, "ENTER", Font::Display, 18, GOLD, Align::Centre);
            ui.text(90 + kw + 14, 326, "The Arena", Font::Display, 34, GOLD_INK);
            ui.text(90, 368, std::to_string(v.waves) + " timed waves, a shop between them, bosses midway and at the end", Font::Text500, 13, ARENA_LINE);
            ui.hit(72, 318, 472, 78, TITLE_ARENA);
        }

        struct Item { std::string key, name, hint; int action; };
        const Item items[] = {
            {"T", "Training ground", "dummies, enemies that come back", TITLE_TRAINING},
            {"B", "Spellbook", "build your spells", TITLE_SPELLBOOK},
            {"O", "Options", "mouse, view, sound", TITLE_OPTIONS},
        };

        for (int k = 0; k < 3; ++k)
        {
            const float y = 404.0f + static_cast<float>(k) * 56.0f;
            const bool over = ui.hover(72, y, 472, 48);
            ui.box(72, y, 472, 48, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(84, y + 11, 26, items[k].key, GOLD, GOLD_INK, 20, 0.0f);
            ui.text(124, y + 9, items[k].name, Font::Display, 28, TEXT);
            ui.text(530, y + 17, items[k].hint, Font::Text500, 12, MUTED, Align::Right);
            ui.hit(72, y, 472, 48, items[k].action);
        }

        ui.text(72, 596, "BEST SCORE", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        const float bw = ui.text(72, 610, grouped(v.best), Font::Display, 40, TEXT);
        ui.text(72 + bw + 12, 628, v.best > 0 ? "your best run so far" : "no run yet", Font::Text500, 12, MUTED);

        ui.text(72, 668, "F1 overlay \xC2\xB7 ` console \xC2\xB7 Esc frees the mouse", Font::Text500, 11, FAINT);
    }

    void drawEnd(Ui& ui, const EndView& v)
    {
        ui.section("end");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), v.won ? withAlpha(INK, 0.5f) : DEFEAT_WASH);

        const Colour accent = v.won ? GOLD : HEALTH_NUMBER;

        ui.anchor(Anchor::Centre);
        ui.raised(320, 150, 640, 420, PANEL, 4.0f);
        ui.rect(322, 152, 636, 6, accent);

        ui.textShadow(354, 176, v.won ? "VICTORY" : "DEFEATED", Font::Display, 84, accent, 4.0f, Align::Left, 2.0f);
        ui.text(354, 262, v.won ? "All " + std::to_string(v.waves) + " waves, and " + v.boss + " falls."
                                : "At wave " + std::to_string(v.wave) + " of " + std::to_string(v.waves) + ".",
                Font::Text600, 16, SUBTITLE);

        // The waves: reached, the one it ended on, the rest
        for (int k = 0; k < v.waves; ++k)
        {
            const float x = 354.0f + static_cast<float>(k) * (572.0f + 3.0f) / static_cast<float>(v.waves);
            const float w = std::floor(575.0f / static_cast<float>(v.waves)) - 3.0f;
            Colour c = withAlpha(INK, 0.6f);
            if (k < v.wave - 1 or (v.won and k < v.waves))
                c = TEXT;
            else if (k == v.wave - 1)
                c = HEALTH;
            ui.rect(std::round(x), 294, w, 10, c);
        }

        // The score, the best, the run in numbers
        ui.box(354, 322, 572, 96, WELL);
        ui.text(372, 336, "SCORE", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        const float sw = ui.text(372, 350, grouped(v.score), Font::Display, 56, TEXT);

        if (v.score >= v.best and v.score > 0)
        {
            const float tw = ui.measure("BEST!", Font::Display, 18) + 12.0f;
            ui.rect(372 + sw + 12, 368, tw, 22, GOLD);
            ui.text(372 + sw + 18, 369, "BEST!", Font::Display, 18, GOLD_INK);
        }
        else
        {
            ui.text(372 + sw + 12, 372, "(best " + grouped(v.best) + ")", Font::Text600, 13, MUTED);
        }

        const int seconds = static_cast<int>(v.time);
        const std::string time = std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);
        const std::pair<std::string, std::string> stats[] = {{"KILLS", std::to_string(v.kills)}, {"LEVEL", std::to_string(v.level)}, {"TIME", time}};

        for (int k = 0; k < 3; ++k)
        {
            const float x = 926.0f - 18.0f - static_cast<float>(3 - k) * 86.0f;
            ui.text(x, 340, stats[k].first, Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.text(x, 358, stats[k].second, Font::Display, 34, TEXT);
        }

        // Again, or the training ground
        const float aw = 22.0f + ui.measure("ENTER", Font::Display, 16) + 12.0f + ui.measure("Play again", Font::Display, 28) + 24.0f;
        bool over = ui.hover(354, 492, aw, 48);
        ui.raised(354, 492, aw, 48, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
        const float kw = ui.measure("ENTER", Font::Display, 16) + 12.0f;
        ui.box(366, 505, kw, 22, INK, 0.0f);
        ui.text(366 + kw * 0.5f, 507, "ENTER", Font::Display, 16, GOLD, Align::Centre);
        ui.text(366 + kw + 12, 500, "Play again", Font::Display, 28, GOLD_INK);
        ui.hit(354, 492, aw, 48, END_AGAIN);

        if (v.solo)
        {
            const float tx = 354.0f + aw + 12.0f;
            const float tw = 12.0f + 26.0f + 12.0f + ui.measure("Training ground", Font::Display, 28) + 16.0f;
            over = ui.hover(tx, 492, tw, 48);
            ui.box(tx, 492, tw, 48, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(tx + 12, 503, 26, "T", GOLD, GOLD_INK, 20, 0.0f);
            ui.text(tx + 50, 500, "Training ground", Font::Display, 28, TEXT);
            ui.hit(tx, 492, tw, 48, END_TRAINING);
        }
    }
}
