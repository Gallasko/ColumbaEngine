#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "content.h"
#include "physics.h"
#include "terrain.h"
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
        float cameraPitch = 0.0f;           // degrees, + looks down (first person aims attacks with it)

        bool jumpPressed = false;
        bool jumpHeld = false;
        bool sprintHeld = false;
        bool dodgePressed = false;
        bool lightPressed = false;
        bool heavyPressed = false;

        /** First person: the body faces where the camera looks, and attacks go there. */
        bool faceCamera = false;

        /** Where the player aims: the camera's ray (for the terrain abilities). */
        glm::vec3 aimOrigin = glm::vec3(0.0f);
        glm::vec3 aimDir = glm::vec3(0, 0, 1);

        static constexpr int ABILITIES = 4;
        bool abilityPressed[ABILITIES] = {};
        bool abilityHeld[ABILITIES] = {};
    };

    enum class Action { Free, Attack, Dodge, Stagger, Land, Dead };

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
        std::vector<size_t> cellsHitThisSwing;   // terrain cells the current swing already damaged

        float abilityCooldown[InputFrame::ABILITIES] = {};
        float mineTimer = 0.0f;

        // Buffered presses: kind and seconds since the press (negative = none)
        int queued = 0;            // 1 light, 2 heavy
        float queuedAge = -1.0f;
        float dodgeQueuedAge = -1.0f;

        glm::vec3 dodgeDir = glm::vec3(0, 0, 1);

        int hitstop = 0;           // steps frozen
        int flash = 0;             // steps of hit flash left
        glm::vec3 push = glm::vec3(0.0f);   // knockback velocity, decays on its own

        float coyote = 0.0f;

        // Momentum: horizontal speed at takeoff, and the speed kept for a moment after landing
        float airSpeed = 0.0f;
        float landGrace = 0.0f;
        float landSpeed = 0.0f;
        float landedSpeed = 0.0f;   // falling speed of a landing made this step, 0 when none
        bool airborneStagger = false;   // knocked into the air: a dodge on landing gets up at once
        float airDodgeAge = -1.0f;      // seconds since a dodge pressed in the air, kept for the landing
        glm::vec3 aimDir = glm::vec3(0, 0, 1);   // player: the crosshair's direction, for what the player shoots
        float aiRadius = 2.5f;          // the circle a flanking enemy keeps around the target
        float attackPitch = 0.0f;       // degrees the current attack aims down (+) or up (-): hitboxes tilt with it
        bool heavyStagger = false;      // the stagger came from a heavy hit: no rolling out of it
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

    /**
     * Something worth a sound or particles: the game turns these into effects through effects.json.
     * kind: hit, stagger, kill, swing, windup, shoot, arrow_hit, dodge, jump, land, hard_land,
     *       chip, break, raise, dig, piece_fall, piece_land
     */
    struct WorldEvent
    {
        std::string kind;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 color = glm::vec3(1.0f);    // the block's colour for terrain events
        float strength = 1.0f;                // damage, fall speed...
        int fighter = 0;                      // who it happened to (0: nobody)
    };

    /** Something shot by an attack (see ProjectileDef). */
    struct Projectile
    {
        int id = 0;
        int owner = 0;
        Team team = Team::Enemy;
        std::string attack;
        ProjectileDef def;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 velocity = glm::vec3(0.0f);
        float age = 0.0f;
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
        bool blocked = false;        // a block stands between self and the target
        float blockedDistance = 0.0f;// how far that block is

        // The rest of the group, for scripts that take turns or work together
        int alliesNear = 0;          // other enemies within 15 m
        int alliesAttacking = 0;     // other enemies in an attack right now
        int alliesNearTarget = 0;    // other enemies within 3.5 m of the target
        float targetFacing = 0.0f;   // degrees between where the target looks and self: 0 in its face, 180 behind it
    };

    struct BrainOrder
    {
        std::string move = "hold";   // hold, chase, strafe, retreat, flank (round to the target's back)
        std::string attack;          // attack name to start now, empty for none
        float strafe = 1.0f;         // strafe side: + right, - left
        float speed = 1.0f;          // walk speed multiplier
        bool face = true;            // turn to the target
        float radius = 2.5f;         // flank: the distance kept from the target while going round
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

        /**
         * A hitbox in the world: turned by the fighter's yaw, and tilted by its attack pitch about the
         * box's own height above the feet (a swing aimed down reaches down). The tilted box is bounded
         * by a box upright in that plane, a little generous at steep angles.
         */
        void hitboxInWorld(const Fighter& f, const HitBox& box, glm::vec3& centre, glm::vec3& size) const;

        VoxelGrid grid;
        std::vector<Fighter> fighters;
        std::vector<HitEvent> hits;   // since the last drain, read by the game for feedback

        std::vector<Projectile> projectiles;
        std::vector<WorldEvent> events;   // since the last drain, read by the game for sounds and particles

        Terrain terrain;
        TerrainEvents terrainEvents;  // since the last drain, read by the game
        float terrainShake = 0.0f;    // camera trauma from falling pieces near the player, since the last drain

        /** The block the player aims at within range of the player, from an aim ray (the camera's). */
        bool aimedCell(const Fighter& f, const glm::vec3& origin, const glm::vec3& dir, float range, RayHit& out) const;

        const AbilityDef* ability(int index) const;

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
        /** pitch: NAN aims at the target (aim assist's for the player), a number aims there (first person). */
        void startAttack(Fighter& f, const std::string& name, const glm::vec3& aimDir, bool aimAssist = true, float pitch = NAN);
        void startDodge(Fighter& f, const glm::vec3& dir);
        void resolveHits();
        void applyHit(Fighter& attacker, Fighter& target, const AttackDef& attack);
        void separate();
        void useAbilities(Fighter& f, const InputFrame& in);
        void fireProjectiles();
        void stepProjectiles();
        void event(const std::string& kind, const glm::vec3& position, float strength = 1.0f, int fighter = 0, const glm::vec3& color = glm::vec3(1.0f));
        glm::vec3 chest(const Fighter& f) const { return f.body.position + glm::vec3(0, f.body.height * 0.6f, 0); }
        int nextProjectile = 1;
        const Fighter* assistTarget = nullptr;   // startAttack: who aim assist picked, for the pitch
        void terrainFromAttacks();
        void stepTerrain();
        bool cellOccupied(const glm::ivec3& cell) const;
        void liftOut(const glm::ivec3& cell);
        void respawn(Fighter& f);
        void spendStamina(Fighter& f, float amount);
        void regenStamina(Fighter& f);
        void turnTowards(Fighter& f, const glm::vec3& dir, float degreesPerSecond);

        Brain* brain = nullptr;
        int nextId = 1;
    };
}
