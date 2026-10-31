#pragma once

#include <map>
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
        float volume = 0.5f;         // the whole game's sound
        float shake = 1.0f;          // camera shake
        float hitFlash = 1.0f;       // the red flash when hit
        bool trails = true;          // slash trails
        bool firstPerson = true;     // the view to play in
        bool overlay = false;        // the debug overlay (F1)

        // Multiplayer: the name the others see, and the last address joined (the join panel's start)
        std::string name;
        std::string lastJoin;

        // The keys the player chose (the controls screen): action -> its first key, SDL scancode names
        // ("W", "Space") or Mouse1-Mouse5; what is not here keeps the tuning's
        std::map<std::string, std::string> keys;
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

        /**
         * paused: the game waits while it is open (alone); inGame: opened from playing (Esc), its button
         * resumes; quitArmed: Quit was clicked once, the next click quits.
         */
        void draw(Ui& ui, const GameOptions& options, bool paused = false, bool inGame = false, bool quitArmed = false, bool titleArmed = false);

        /** A click's action (Ui::hitAt); returns true when an option changed (`closed` on Back). */
        bool act(int action, GameOptions& options, bool& closed);

        /** The keys: up and down pick a row, left and right change it. Returns true when an option changed. */
        bool key(int dx, int dy, GameOptions& options);

        static constexpr int TO_TITLE = 95, CONTROLS = 96, QUIT = 97, NAME = 98, BACK = 99, ROW = 100;   // ROW + row * 10 + (1 minus, 2 plus, 3 first choice, 4 second)

        // The name the others see, edited in place: typed into while `naming`
        bool naming = false;
        void startNaming(const std::string& current) { naming = true; nameEdit = current; }
        void type(const std::string& text);
        void backspace() { if (not nameEdit.empty()) nameEdit.pop_back(); }
        /** Done: the name typed (trimmed; "" when none). */
        std::string commitName();

    private:
        bool open = false;
        int focus = 0;
        std::string nameEdit;
        float blink = 0.0f;
    };
}
