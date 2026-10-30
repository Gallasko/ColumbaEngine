#include "stdafx.h"

#include "options.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Core/json.h"

namespace arena
{
    namespace
    {
        constexpr int ROWS = 9;
        const char* const NAMES[ROWS] = {"Mouse sensitivity", "Invert mouse Y", "Field of view", "Volume", "Camera shake", "Hit flash", "Slash trails", "View", "Debug overlay (F1)"};

        bool isToggle(int row) { return row == 1 or row == 6 or row == 7 or row == 8; }

        std::string percent(float v) { return std::to_string(static_cast<int>(std::lround(v * 100.0f))) + "%"; }

        std::string valueOf(const GameOptions& o, int row)
        {
            std::ostringstream s;
            switch (row)
            {
                case 0: s.setf(std::ios::fixed); s.precision(2); s << o.sensitivity; return s.str();
                case 1: return o.invertY ? "On" : "Off";
                case 2: return (o.fovOffset >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(o.fovOffset)) + "\xC2\xB0";
                case 3: return percent(o.volume);
                case 4: return percent(o.shake);
                case 5: return percent(o.hitFlash);
                case 6: return o.trails ? "On" : "Off";
                case 7: return o.firstPerson ? "First person" : "Third person";
                case 8: return o.overlay ? "On" : "Off";
            }
            return "";
        }

        /** Where a number option is in its range, 0..1. */
        float fraction(const GameOptions& o, int row)
        {
            switch (row)
            {
                case 0: return (o.sensitivity - 0.2f) / 2.8f;
                case 2: return (o.fovOffset + 20.0f) / 50.0f;
                case 3: return o.volume;
                case 4: return o.shake / 1.5f;
                case 5: return o.hitFlash;
            }
            return 0.0f;
        }

        void change(GameOptions& o, int row, int step)
        {
            switch (row)
            {
                case 0: o.sensitivity = std::clamp(o.sensitivity + 0.1f * static_cast<float>(step), 0.2f, 3.0f); break;
                case 1: o.invertY = not o.invertY; break;
                case 2: o.fovOffset = std::clamp(o.fovOffset + 5.0f * static_cast<float>(step), -20.0f, 30.0f); break;
                case 3: o.volume = std::clamp(o.volume + 0.1f * static_cast<float>(step), 0.0f, 1.0f); break;
                case 4: o.shake = std::clamp(o.shake + 0.25f * static_cast<float>(step), 0.0f, 1.5f); break;
                case 5: o.hitFlash = std::clamp(o.hitFlash + 0.25f * static_cast<float>(step), 0.0f, 1.0f); break;
                case 6: o.trails = not o.trails; break;
                case 7: o.firstPerson = not o.firstPerson; break;
                case 8: o.overlay = not o.overlay; break;
            }
        }
    }

    std::string optionsPath()
    {
        std::string base;

        if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg and *xdg)
            base = xdg;
        else if (const char* home = std::getenv("HOME"); home and *home)
            base = std::string(home) + "/.local/share";
        else
            base = ".";

        return base + "/voxelarena/options.json";
    }

    bool loadOptions(const std::string& path, GameOptions& out)
    {
        std::ifstream file(path);

        if (not file)
            return false;

        std::stringstream text;
        text << file.rdbuf();
        const JsonParseResult parsed = parseJson(text.str());

        if (not parsed.ok or not parsed.value.isObject())
            return false;

        const Json& j = parsed.value;
        GameOptions o;
        o.sensitivity = static_cast<float>(j.num("sensitivity", o.sensitivity));
        o.invertY = j.flag("invertY", o.invertY);
        o.fovOffset = static_cast<float>(j.num("fovOffset", o.fovOffset));
        o.volume = static_cast<float>(j.num("volume", o.volume));
        o.shake = static_cast<float>(j.num("shake", o.shake));
        o.hitFlash = static_cast<float>(j.num("hitFlash", o.hitFlash));
        o.trails = j.flag("trails", o.trails);
        o.firstPerson = j.flag("firstPerson", o.firstPerson);
        o.overlay = j.flag("overlay", o.overlay);
        out = o;
        return true;
    }

    bool saveOptions(const std::string& path, const GameOptions& o)
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        std::ofstream file(path, std::ios::trunc);

        if (not file)
            return false;

        auto flag = [](bool b) { return b ? "true" : "false"; };
        file << "{\n"
             << "    \"sensitivity\": " << o.sensitivity << ",\n"
             << "    \"invertY\": " << flag(o.invertY) << ",\n"
             << "    \"fovOffset\": " << o.fovOffset << ",\n"
             << "    \"volume\": " << o.volume << ",\n"
             << "    \"shake\": " << o.shake << ",\n"
             << "    \"hitFlash\": " << o.hitFlash << ",\n"
             << "    \"trails\": " << flag(o.trails) << ",\n"
             << "    \"firstPerson\": " << flag(o.firstPerson) << ",\n"
             << "    \"overlay\": " << flag(o.overlay) << "\n"
             << "}\n";
        return static_cast<bool>(file);
    }

    void OptionsScreen::draw(Ui& ui, const GameOptions& o, bool paused, bool inGame)
    {
        using namespace ui;

        if (not open)
            return;

        ui.section("options");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.58f));

        ui.anchor(Anchor::Centre);
        ui.raised(320, 44, 640, 632, PANEL, 4.0f);

        float hx = 348.0f + ui.text(348, 64, "Options", Font::Display, 44, TEXT) + 14.0f;
        if (inGame)
        {
            const std::string tag = paused ? "PAUSED" : "THE GAME GOES ON";
            const float tw = ui.measure(tag, Font::Display, 16, 1.0f) + 12.0f;
            ui.rect(hx, 80, tw, 20, LINE);
            ui.text(hx + 6, 81, tag, Font::Display, 16, TEXT, Align::Left, 1.0f);
            hx += tw + 12.0f;
        }
        ui.text(hx, 84, "saved as you change it", Font::Text500, 12, GOOD);

        for (int row = 0; row < ROWS; ++row)
        {
            const auto itemScope = ui.scope(static_cast<uint64_t>(row));
            const float y = 122.0f + 52.0f * static_cast<float>(row);
            if (ui.hover(348, y, 584, 46))
                focus = row;

            if (row == focus)
            {
                ui.rect(348, y, 584, 46, hex(0x252833));
                ui.rect(348, y, 4, 46, GOLD);
            }

            ui.text(366, y + 14, NAMES[row], Font::Text600, 15, row == focus ? TEXT : TEXT_DIM);

            if (isToggle(row))
            {
                // Two choices side by side, the one in effect filled
                const bool second = (row == 1 and o.invertY) or (row == 6 and o.trails) or (row == 7 and not o.firstPerson) or (row == 8 and o.overlay);
                const std::string a = row == 7 ? "First person" : "Off";
                const std::string b = row == 7 ? "Third person" : "On";
                const float aw = ui.measure(a, Font::Display, 20) + 28.0f, bw = ui.measure(b, Font::Display, 20) + 28.0f;
                const float x1 = 918.0f - bw, x0 = x1 - aw;

                ui.box(x0, y + 7, aw, 32, second ? CARD : TEXT, 2.0f, INK);
                ui.text(x0 + aw * 0.5f, y + 12, a, Font::Display, 20, second ? MUTED : INK, Align::Centre);
                ui.box(x1, y + 7, bw, 32, second ? TEXT : CARD, 2.0f, INK);
                ui.text(x1 + bw * 0.5f, y + 12, b, Font::Display, 20, second ? INK : MUTED, Align::Centre);

                ui.hit(x0, y + 7, aw, 32, ROW + row * 10 + 3);
                ui.hit(x1, y + 7, bw, 32, ROW + row * 10 + 4);
            }
            else
            {
                const float fill = fraction(o, row);
                const int pips = static_cast<int>(std::lround(fill * 10.0f));
                for (int k = 0; k < 10; ++k)
                    ui.box(568.0f + static_cast<float>(k) * 14.0f, y + 16, 12, 14, k < pips ? GOLD : WELL, 2.0f, INK);

                ui.text(788, y + 11, valueOf(o, row), Font::Display, 24, TEXT, Align::Right);

                for (int step : {-1, 1})
                {
                    const float bx = step < 0 ? 846.0f : 886.0f;
                    const bool over = ui.hover(bx, y + 7, 32, 32);
                    ui.box(bx, y + 7, 32, 32, CARD, 2.0f, over ? GOLD : LINE);
                    ui.text(bx + 16, y + 9, step < 0 ? "\xE2\x88\x92" : "+", Font::Display, 26, TEXT, Align::Centre);
                    ui.hit(bx, y + 7, 32, 32, ROW + row * 10 + (step < 0 ? 1 : 2));
                }
            }
        }

        const bool over = ui.hover(348, 610, 150, 42);
        ui.raised(348, 610, 150, 42, GOLD, 3.0f, 2.0f, over ? TEXT : INK);
        const float kw = ui.measure("ESC", Font::Display, 15) + 10.0f;
        ui.box(360, 620, kw, 22, INK, 0.0f);
        ui.text(360 + kw * 0.5f, 622, "ESC", Font::Display, 15, GOLD, Align::Centre);
        ui.text(360 + kw + 10, 617, inGame ? "Resume" : "Back", Font::Display, 24, GOLD_INK);
        ui.hit(348, 610, 150, 42, BACK);

        ui.text(512, 624, "Up / Down pick a row \xC2\xB7 Left / Right change it \xC2\xB7 Esc closes", Font::Text500, 11, FAINT);
    }

    bool OptionsScreen::act(int action, GameOptions& o, bool& closed)
    {
        closed = false;

        if (not open)
            return false;

        if (action == BACK)
        {
            closed = true;
            return false;
        }

        if (action < ROW)
            return false;

        const int row = (action - ROW) / 10, kind = (action - ROW) % 10;
        if (row < 0 or row >= ROWS)
            return false;

        focus = row;

        if (kind == 1 or kind == 2)
        {
            change(o, row, kind == 1 ? -1 : 1);
            return true;
        }

        // A choice: only a change when it is not the one in effect
        const bool second = (row == 1 and o.invertY) or (row == 6 and o.trails) or (row == 7 and not o.firstPerson) or (row == 8 and o.overlay);
        if ((kind == 4) != second)
        {
            change(o, row, 0);
            return true;
        }

        return false;
    }

    bool OptionsScreen::key(int dx, int dy, GameOptions& o)
    {
        if (not open)
            return false;

        if (dy != 0)
        {
            focus = (focus + dy + ROWS) % ROWS;
            return false;
        }

        if (dx != 0)
        {
            change(o, focus, dx);
            return true;
        }

        return false;
    }
}
