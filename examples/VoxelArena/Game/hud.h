#pragma once

#include <string>
#include <vector>

#include "ui.h"

namespace arena
{
    /**
     * The combat HUD (UI_SPEC.md section 4) and the developer layers (section 11), drawn each frame
     * from a View the game fills in. It keeps what its moments need between frames: the health pips
     * falling off, the level up badge popping, a spell's key flashing as it comes off cooldown.
     */
    class Hud
    {
    public:
        struct Status
        {
            std::string id;        // "burn", "chill", "frozen", "shock"
            std::string name;
            int stacks = 1;
        };

        struct Skill
        {
            std::string key, name;
            float progress = 1.0f;   // 1 ready, else how far its cooldown (or charge) is
            float seconds = 0.0f;    // left on the cooldown
            bool charging = false;   // held to charge: shows "hold"
            bool noStamina = false;
            int cost = 0;            // stamina
        };

        struct SpellKey
        {
            enum class State { Ready, Cooldown, NoMana, Locked };
            std::string key, name, element;
            State state = State::Ready;
            float progress = 1.0f, seconds = 0.0f;
            int mana = 0;
        };

        struct Mate
        {
            std::string name;
            float health = 1.0f;
            bool down = false;
        };

        struct EnemyBar
        {
            glm::vec2 head;          // window pixels: the top of its head
            float health = 1.0f, poise = 1.0f;
            bool staggered = false, elite = false, boss = false;
            std::vector<Status> statuses;
            std::string name;        // a player's, in the final duel
        };

        struct Marker
        {
            enum class Kind { Spawn, Elite, Boss };
            Kind kind = Kind::Spawn;
            bool right = true;       // the screen side it is on
            float y = 360.0f;        // base layout
            std::string name;        // a boss's
            float distance = 0.0f;   // m
        };

        struct View
        {
            bool player = false, alive = true;

            // The body
            float health = 100.0f, maxHealth = 100.0f;
            float stamina = 1.0f, mana = 1.0f;       // 0..1
            bool exhausted = false, manaLow = false;
            float dashes = 2.0f;
            int maxDashes = 2;
            std::vector<std::pair<std::string, bool>> weapons;   // owned, and whether in hand
            std::string nextWeaponKey = "TAB";
            std::vector<Status> statuses;

            std::vector<Skill> skills;
            std::vector<SpellKey> spells;
            bool crosshair = true;
            float draw = -1.0f;      // a charge being held (a bow's draw), 0..1; < 0: none

            // The run
            bool run = false;
            int wave = 0, waves = 20;
            float timer = 0.0f;
            bool bossWave = false;
            bool duel = false;      // the final duel: its time instead of the wave's
            int shards = 0, level = 1, xp = 0, xpNext = 1, levelUps = 0, score = 0;
            std::vector<Mate> team;   // co-op: the others

            bool boss = false, enraged = false;
            std::string bossName;
            float bossHealth = 1.0f, bossPoise = 1.0f;

            std::vector<EnemyBar> enemies;
            std::vector<Marker> markers;

            // Moments
            float hurt = 0.0f;              // the red wash, 0..1 (already times the option)
            std::string headline;           // a wave's or a boss's name as it starts
            int countdown = 0;              // 3, 2, 1 at a wave's end (0: none)
            std::string message;            // "Paused", "You fell", "Connecting..."
            bool messageDim = false;        // over an ink wash (Paused)
            float time = 0.0f;              // s, for pulses
        };

        void draw(Ui& ui, const View& v, float dt);

        /** The F1 overlay, the console and the content error banner. */
        void devLayers(Ui& ui, const std::string& overlay, bool console, const std::vector<std::string>& lines, const std::string& input, const std::string& banner);

    private:
        void drawRun(Ui& ui, const View& v);
        void drawBoss(Ui& ui, const View& v);
        void drawBody(Ui& ui, const View& v);
        void drawAbilities(Ui& ui, const View& v);
        void drawCrosshair(Ui& ui);
        void drawEnemies(Ui& ui, const View& v);
        void drawMarkers(Ui& ui, const View& v);
        void drawMoments(Ui& ui, const View& v);

        // Health pips falling off
        struct Falling
        {
            int pip = 0;
            float age = 0.0f;
        };
        std::vector<Falling> falling;
        int lastPips = -1;

        int lastLevelUps = 0;
        float badgeAge = 10.0f;       // s since the +N badge changed
        float levelUpAge = 10.0f;     // s since a level up ("LEVEL UP")
        int lastLevel = 0;

        std::vector<int> lastSpellState;
        std::vector<int> spellFlash;  // frames of white left
    };
}
