#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Core/content.h"
#include "Core/json.h"
#include "Core/tuning.h"

#include "ui.h"

namespace pg
{
    struct VM;
    struct ObjFunction;
}

namespace arena
{
    /**
     * The lab (--lab, or the console's lab): a tool for making the game. A panel lists everything the
     * mods load (items, weapons, techniques, spells, enemies); picking one shows it on the training
     * ground, at the level, branch or rarity picked.
     *
     * How a pick is shown is a pgscript (tools/lab/lab.pg): `fun stage()` sets the scene, and one
     * function per kind (showItem, showWeapon, showTechnique, showSpell, showEnemy) gets the pick as
     * a table and answers with console commands, in order: run("weapon", w.id), press("light"),
     * wait(20)... They are queued like --exec's, so the scene plays out on the game's own steps and
     * draws with the game's own renderer. The script reloads when its file changes.
     */
    class Lab
    {
    public:
        enum class Kind { Items, Weapons, Techniques, Spells, Enemies };
        static constexpr int KINDS = 5;

        // Click actions on the panel (Ui::hit): out of the screens' range
        static constexpr int ACTION_KIND = 3000;     // + the kind
        static constexpr int ACTION_ENTRY = 3100;    // + the row on screen
        static constexpr int ACTION_LEVEL = 3400;    // + the level (0-based)
        static constexpr int ACTION_BRANCH = 3500;   // + 1 + the branch (-1: none)
        static constexpr int ACTION_REPLAY = 3600, ACTION_LOOP = 3601, ACTION_SLOW = 3602, ACTION_BOXES = 3603, ACTION_VIEW = 3604, ACTION_ORBIT = 3605,
                             ACTION_CLOSE = 3606, ACTION_STAGE = 3607;
        static bool owns(int action) { return action >= ACTION_KIND and action < ACTION_KIND + 1000; }

        Lab();
        ~Lab();

        /** The lists from the content, and the script compiled anew. Keeps the pick (by id) when it is still there. */
        void load(const std::string& scriptPath, const Content& content, const Tuning& tuning);

        /** The script's file changed since it was loaded (checked at most twice a second). */
        bool scriptChanged(float frameSeconds);

        /** The console commands that set the scene / show the pick, as the script answered them. */
        std::vector<std::string> stage();
        std::vector<std::string> show();

        /** A key: true when the lab used it. `replay` is set when the pick changed (show it). */
        bool key(int scancode, bool& replay);
        /** A click on the panel: what it changed, as for key. Toggles the game owns come back in `command`. */
        bool click(int action, bool& replay, std::string& command);
        /** The mouse wheel over the panel scrolls its list. */
        void scroll(float steps);
        /** The panel covers this window point (the mouse there drives the panel, not the camera). */
        bool covers(const Ui& ui, float px, float py) const;

        struct View
        {
            bool loop = false, slow = false, boxes = false, firstPerson = false, orbit = false;
            bool busy = false;          // the scene is still playing
            std::string caption;        // what the scene is showing now (the script's caption)
        };
        void draw(Ui& ui, const View& v);

        bool loop = false;
        std::string scriptPath;
        std::string status;             // the script's state: ready, or its error
        bool scriptOk = false;
        std::vector<std::string> printed;   // the script's print() lines, the latest last

    private:
        struct Entry
        {
            std::string id, name, text;
            std::string tag;                     // a short note on the row (an item's part, a spell's element)
            ui::Colour colour = ui::TEXT;        // the row's note colour
            bool dim = false;                    // nothing to see (an item with no look)
            std::vector<std::string> levels;     // the level row's labels (empty: no level row)
            std::vector<std::string> branches;   // the branch row's choices (empty: no branch row)
            std::string levelLabel = "Level", branchLabel = "Branch", noBranch = "None";
            Json info;                           // handed to the script, with the level and branch picked
        };

        void build(const Content& content, const Tuning& tuning);
        std::vector<std::string> call(const std::string& function, const Json* arg);
        const Entry* current() const;
        void select(int index);
        void clampScroll();

        std::vector<Entry> lists[KINDS];
        int kind = 0;
        int index[KINDS] = {};
        int scrollTop[KINDS] = {};
        int level = 0;        // into the entry's levels
        int branch = -1;

        // The script
        struct Script;
        std::unique_ptr<Script> script;
        std::filesystem::file_time_type scriptTime{};
        float checkTimer = 0.0f;
        std::vector<std::string> out;   // the commands the running call queued
    };
}
