#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "content.h"
#include "physics.h"
#include "tuning.h"

namespace arena
{
    /** The simulation runs at a fixed 60 steps per second; every frame count in content is in these steps. */
    constexpr float STEP_SECONDS = 1.0f / 60.0f;

    /** Facing direction of a yaw in degrees: 0 faces +Z, 90 faces +X. */
    glm::vec3 yawForward(float yawDegrees);

    /** Local (x right, y up, z forward) to world offset for a fighter facing yaw. */
    glm::vec3 localToWorld(float yawDegrees, const glm::vec3& local);

    /** One step of player input. Presses are edges, collected since the previous step. */
    struct InputFrame
    {
        glm::vec2 move = glm::vec2(0.0f);   // x right, y forward, relative to the camera; length <= 1
        float cameraYaw = 0.0f;             // degrees, same convention as yawForward

        bool jumpPressed = false;
        bool jumpHeld = false;
        bool sprintHeld = false;
        bool dodgePressed = false;
        bool lightPressed = false;
        bool heavyPressed = false;

        /** First person: the body faces where the camera looks, and attacks go there. */
        bool faceCamera = false;
    };

    enum class Action { Free, Attack, Dodge, Stagger, Dead };

    const char* actionName(Action a);

    enum class Team { Player, Enemy };

    struct Fighter
    {
        int id = 0;
        Team team = Team::Enemy;
        std::string type;          // enemy id, or "player"

        Body body;
        float yaw = 0.0f;

        float health = 100.0f, maxHealth = 100.0f;
        float poise = 10.0f, maxPoise = 10.0f;
        float sinceHit = 100.0f;   // seconds

        float stamina = 100.0f;
        float staminaDelay = 0.0f;
        bool exhausted = false;
        bool sprinting = false;

        Action action = Action::Free;
        int frame = 0;             // steps into the current action
        int actionLength = 0;      // stagger and hitstun length

        std::string weapon;        // player: weapon id
        std::string attack;        // current attack name
        std::vector<int> hitThisSwing;

        // Buffered presses: kind and seconds since the press (negative = none)
        int queued = 0;            // 1 light, 2 heavy
        float queuedAge = -1.0f;
        float dodgeQueuedAge = -1.0f;

        glm::vec3 dodgeDir = glm::vec3(0, 0, 1);

        int hitstop = 0;           // steps frozen
        int flash = 0;             // steps of hit flash left
        glm::vec3 push = glm::vec3(0.0f);   // knockback velocity, decays on its own

        float coyote = 0.0f;
        float jumpBuffer = 0.0f;
        bool jumping = false;

        float stepOffset = 0.0f;   // visual: smooths the pop of an auto step-up

        float respawnTimer = 0.0f;
        glm::vec3 spawnPosition = glm::vec3(0.0f);
        float spawnYaw = 0.0f;

        // AI orders, refreshed each step by the brain
        std::string aiMove = "hold";
        float aiStrafe = 1.0f;
        float aiSpeed = 1.0f;
        bool aiFace = true;
        float sinceAttack = 100.0f;   // seconds since the last attack ended
        std::string aiLastError;

        bool alive() const { return action != Action::Dead; }
        bool invulnerable(const Tuning& t) const
        {
            // Read after the step advanced the counter: the frame just played is frame - 1
            return action == Action::Dodge and frame - 1 >= t.dodge.iframeStart and frame - 1 < t.dodge.iframeEnd;
        }
    };

    struct HitEvent
    {
        int attacker = 0;
        int target = 0;
        std::string attack;
        glm::vec3 point = glm::vec3(0.0f);
        float damage = 0.0f;
        int hitstop = 0;
        float shake = 0.0f;
        bool staggered = false;
        bool killed = false;
    };

    /** What a brain sees of its enemy, and what it answers. Filled by the world, read by the script. */
    struct BrainView
    {
        const Fighter* self = nullptr;
        const Fighter* target = nullptr;
        const EnemyDef* def = nullptr;
        float distance = 0.0f;       // between feet, horizontal
        float angle = 0.0f;          // degrees from self facing to the target, + = target on the right
        float time = 0.0f;           // seconds since the world started
    };

    struct BrainOrder
    {
        std::string move = "hold";   // hold, chase, strafe, retreat
        std::string attack;          // attack name to start now, empty for none
        float strafe = 1.0f;         // strafe side: + right, - left
        float speed = 1.0f;          // walk speed multiplier
        bool face = true;            // turn to the target
    };

    /** Decides what an enemy does. The game's is pgscript (ScriptBrain); tests use plain C++. */
    struct Brain
    {
        virtual ~Brain() = default;
        /** Returns false (and fills error) when the script failed; the enemy then holds still. */
        virtual bool think(const std::string& scriptId, const BrainView& view, BrainOrder& order, std::string& error) = 0;
    };

    /**
     * The whole arena: the voxel grid, the player and the enemies.
     * Knows nothing of windows, rendering or the ECS; step() advances one 60th of a second.
     */
    class ArenaWorld
    {
    public:
        /** Builds the arena and spawns everyone. content must outlive the world (or be replaced with setContent). */
        void start(const Content& content, const Tuning& tuning);

        /** New definitions after a hot reload: everyone keeps their place, stats are refreshed. */
        void setContent(const Content& content, const Tuning& tuning, bool rebuildArena);

        void setBrain(Brain* b) { brain = b; }

        void step(const InputFrame& input);

        /** Rebuilds the grid from the arena definition. */
        void buildArena();

        Fighter* player();
        const Fighter* player() const;
        Fighter* find(int id);

        int spawnEnemy(const std::string& enemyId, const glm::vec3& position, float yaw);
        void removeEnemies();

        /** Gives the player another weapon. Returns false for an unknown id. */
        bool equip(const std::string& weaponId);

        /** Active hitboxes this step, for the debug overlay. */
        struct DebugBox
        {
            glm::vec3 centre;
            glm::vec3 size;
            float yaw;
            bool hurt;     // hurtbox (target) or hitbox (attack)
            bool active;   // hitbox: hits this step (false: the attack is still winding up)
            bool safe;     // hurtbox: invulnerable this step
            int owner;
        };
        std::vector<DebugBox> debugBoxes() const;

        /** The attack a fighter is running, or nullptr. */
        const AttackDef* currentAttack(const Fighter& f) const;
        const Moveset* movesetOf(const Fighter& f) const;

        VoxelGrid grid;
        std::vector<Fighter> fighters;
        std::vector<HitEvent> hits;   // since the last drain, read by the game for feedback

        bool godMode = false;         // the player takes no damage
        bool aiEnabled = true;

        float time = 0.0f;
        uint64_t steps = 0;

        const Content* content = nullptr;
        Tuning tuning;

    private:
        void stepPlayer(Fighter& f, const InputFrame& in);
        void stepEnemy(Fighter& f);
        void stepCommon(Fighter& f, glm::vec3 wantedVelocity, bool canJump, const InputFrame* in);
        void startAttack(Fighter& f, const std::string& name, const glm::vec3& aimDir);
        void startDodge(Fighter& f, const glm::vec3& dir);
        void resolveHits();
        void applyHit(Fighter& attacker, Fighter& target, const AttackDef& attack);
        void separate();
        void respawn(Fighter& f);
        void spendStamina(Fighter& f, float amount);
        void regenStamina(Fighter& f);
        void turnTowards(Fighter& f, const glm::vec3& dir, float degreesPerSecond);

        Brain* brain = nullptr;
        int nextId = 1;
    };
}
