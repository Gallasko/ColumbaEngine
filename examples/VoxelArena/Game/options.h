#pragma once

#include <string>
#include <vector>

#include "ui.h"

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

    /** The options screen (UI_SPEC.md section 10): a row per option, pips and - + or two choices, Back. */
    class OptionsScreen
    {
    public:
        void setOpen(bool open) { this->open = open; }
        bool isOpen() const { return open; }

        void draw(Ui& ui, const GameOptions& options);

        /** A click's action (Ui::hitAt); returns true when an option changed (`closed` on Back). */
        bool act(int action, GameOptions& options, bool& closed);

        /** The keys: up and down pick a row, left and right change it. Returns true when an option changed. */
        bool key(int dx, int dy, GameOptions& options);

        static constexpr int BACK = 99, ROW = 100;   // ROW + row * 10 + (1 minus, 2 plus, 3 first choice, 4 second)

    private:
        bool open = false;
        int focus = 0;
    };
}
