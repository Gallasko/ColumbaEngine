#include "stdafx.h"

#include "options.h"

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
        const glm::vec4 PANEL(14, 16, 26, 235);
        const glm::vec4 BUTTON(42, 48, 66, 240);
        const glm::vec4 TEXT(236, 238, 242, 255);

        constexpr int ROWS = 9;
        constexpr float ROW = 32.0f, TOP = 72.0f;
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
                case 2: return (o.fovOffset >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(o.fovOffset)) + " degrees";
                case 3: return percent(o.volume);
                case 4: return percent(o.shake);
                case 5: return percent(o.hitFlash);
                case 6: return o.trails ? "On" : "Off";
                case 7: return o.firstPerson ? "First person" : "Third person";
                case 8: return o.overlay ? "On" : "Off";
            }
            return "";
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

    void OptionsScreen::init(Hud* h)
    {
        hud = h;
        pw = 640.0f;
        ph = 520.0f;
        px = (hud->width() - pw) * 0.5f;
        py = (hud->height() - ph) * 0.5f;

        panel = hud->makeQuad(px, py, 6, pw, ph, PANEL);
        title = hud->makeText(px + 24, py + 16, 8, Hud::FONT, "Options", 1.25f, TEXT);
        hint = hud->makeText(px + 24, py + ph - 34, 8, Hud::MONO, "O / Esc: close    saved as you change it", 0.8f, glm::vec4(150, 154, 166, 255));

        for (int row = 0; row < ROWS; ++row)
        {
            const float y = py + TOP + static_cast<float>(row) * ROW;
            names.push_back(hud->makeText(px + 24, y, 8, Hud::FONT, NAMES[row], 0.95f, glm::vec4(200, 204, 214, 255)));
            values.push_back(hud->makeText(px + 330, y, 8, Hud::FONT, "", 0.95f, TEXT));
        }

        for (auto id : {panel, title, hint})
            hud->show(id, false);
        for (auto id : names)
            hud->show(id, false);
        for (auto id : values)
            hud->show(id, false);

        // Made now, every one (an entity made while the screen is drawn has no components that frame)
        buttons.resize(ROWS * 2 + 1);
        for (auto& b : buttons)
        {
            b.back = hud->makeQuad(0, 0, 7, 10, 10, BUTTON);
            b.label = hud->makeText(0, 0, 8, Hud::FONT, "", 0.95f, TEXT);
            hud->show(b.back, false);
            hud->show(b.label, false);
        }
    }

    void OptionsScreen::setButton(Button& b, float x, float y, float w, float h, const std::string& label, int row, int step)
    {
        b.x = x;
        b.y = y;
        b.w = w;
        b.h = h;
        b.row = row;
        b.step = step;
        b.shown = true;
        hud->place(b.back, x, y, w, h, true);
        hud->place(b.label, x + w * 0.5f - 6.0f * static_cast<float>(label.size()) * 0.5f - 2.0f, y + h * 0.5f - 11.0f, w, h, true);
        hud->text(b.label, label);
    }

    void OptionsScreen::setOpen(bool o)
    {
        open = o;

        for (auto id : {panel, title, hint})
            hud->show(id, open);
        for (auto id : names)
            hud->show(id, open);
        for (auto id : values)
            hud->show(id, open);

        if (not open)
            for (auto& b : buttons)
            {
                b.shown = false;
                hud->show(b.back, false);
                hud->show(b.label, false);
            }
    }

    void OptionsScreen::refresh(const GameOptions& o)
    {
        if (not open)
            return;

        for (int row = 0; row < ROWS; ++row)
            hud->text(values[static_cast<size_t>(row)], valueOf(o, row));

        // Each row's buttons level with its text
        size_t k = 0;
        for (int row = 0; row < ROWS; ++row)
        {
            const float y = py + TOP + static_cast<float>(row) * ROW - 1.0f;

            if (isToggle(row))
            {
                setButton(buttons[k++], px + 520, y, 80, 26, "switch", row, 0);
                buttons[k].shown = false;
                hud->show(buttons[k].back, false);
                hud->show(buttons[k].label, false);
                ++k;
            }
            else
            {
                setButton(buttons[k++], px + 520, y, 36, 26, "-", row, -1);
                setButton(buttons[k++], px + 564, y, 36, 26, "+", row, 1);
            }
        }

        setButton(buttons[k], px + 24, py + ph - 80, 120, 34, "Back", -1, 0);
    }

    bool OptionsScreen::click(float x, float y, GameOptions& o, bool& closed)
    {
        closed = false;

        if (not open)
            return false;

        for (const auto& b : buttons)
        {
            if (not b.shown or x < b.x or x > b.x + b.w or y < b.y or y > b.y + b.h)
                continue;

            if (b.row < 0)
            {
                closed = true;
                return false;
            }

            change(o, b.row, b.step);
            return true;
        }

        return false;
    }
}
