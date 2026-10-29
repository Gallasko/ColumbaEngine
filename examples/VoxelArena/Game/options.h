#pragma once

#include <string>
#include <vector>

#include "hud.h"

namespace arena
{
    /** The player's options (the options screen, O), kept between runs. */
    struct GameOptions
    {
        float sensitivity = 1.0f;    // times the tuning's mouse sensitivity
        bool invertY = false;
        float fovOffset = 0.0f;      // degrees added to the field of view
        float volume = 1.0f;         // the whole game's sound
        float shake = 1.0f;          // camera shake
        float hitFlash = 1.0f;       // the red flash when hit
        bool trails = true;          // slash trails
        bool firstPerson = true;     // the view to play in
        bool overlay = false;        // the debug overlay (F1)
    };

    std::string optionsPath();
    bool loadOptions(const std::string& path, GameOptions& out);
    bool saveOptions(const std::string& path, const GameOptions& options);

    /** The options screen: a row per option, - and + (or a toggle), Back. */
    class OptionsScreen
    {
    public:
        void init(Hud* hud);
        void setOpen(bool open);
        bool isOpen() const { return open; }
        void refresh(const GameOptions& options);

        /** A click at screen pixels; returns true when an option changed (or `closed` on Back). */
        bool click(float x, float y, GameOptions& options, bool& closed);

    private:
        struct Button
        {
            pg::_unique_id back = 0, label = 0;
            float x = 0, y = 0, w = 0, h = 0;
            int row = -1;
            int step = 0;            // -1, +1, or 0 for a toggle / Back
            bool shown = false;
        };

        void setButton(Button& b, float x, float y, float w, float h, const std::string& label, int row, int step);

        Hud* hud = nullptr;
        bool open = false;
        float px = 0, py = 0, pw = 0, ph = 0;
        pg::_unique_id panel = 0, title = 0, hint = 0;
        std::vector<pg::_unique_id> names, values;   // a text each per row, level with its buttons
        std::vector<Button> buttons;
    };
}
