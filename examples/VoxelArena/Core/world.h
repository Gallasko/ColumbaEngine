#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "content.h"
#include "spells.h"
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
        bool lightHeld = false;             // held: a light or heavy attack with a charge (a bow's draw) charges while it is
        bool heavyHeld = false;
        bool crouchHeld = false;            // in the air: fast fall

        /** First person: the body faces where the camera looks, and attacks go there. */
        bool faceCamera = false;

        // Network stamps (-1 when local): the server tick the player saw enemies at (lag compensation),
        // and the server tick the player pressed this input at, by its own clock (the dodge grace)
        int viewTick = -1;
        int stamp = -1;

        /** Where the player aims: the camera's ray (for the terrain abilities). */
        glm::vec3 aimOrigin = glm::vec3(0.0f);
        glm::vec3 aimDir = glm::vec3(0, 0, 1);

        /** 1..n: wield the n-th weapon (content load order) from this step on; 0: keep the one held. */
        int equip = 0;

        /**
         * The arena run's shop and menus: BUY + i buys offer i, LOCK + i keeps it for the next shop,
         * REROLL rolls the shop again, LEVEL + i takes level-up pick i, START is ready (or plays again),
         * WEAPON + i takes weapon i (Content::weaponOrder) before the first wave.
         */
        int pick = 0;
        static constexpr int START = 9, BUY = 1, LOCK = 11, REROLL = 20, LEVEL = 31, WEAPON = 41;

        static constexpr int ABILITIES = 8;
        bool abilityPressed[ABILITIES] = {};
        bool abilityHeld[ABILITIES] = {};
    };

    enum class Action { Free, Attack, Dodge, Stagger, Land, AirDash, Dead };

    const char* actionName(Action a);

    enum class Team { Player, Enemy };

    /** A status on a fighter (see StatusDef). */
    struct StatusState
    {
        std::string id;
        float remaining = 0.0f;   // seconds
        int stacks = 1;
    };

    struct Fighter
    {
        int id = 0;
        Team team = Team::Enemy;
        std::string type;          // enemy id, or "player"
        int owner = 0;             // player: the peer that controls it (0 = this machine)

        Body body;
        float yaw = 0.0f;

        float health = 100.0f, maxHealth = 100.0f;
        float poise = 10.0f, maxPoise = 10.0f;
        float sinceHit = 100.0f;   // seconds

        float stamina = 100.0f;
        float staminaDelay = 0.0f;
        bool exhausted = false;
        bool sprinting = false;
        float sinceSprint = 100.0f;   // s since the last sprinting step (the dash attack's window)

        float mana = 100.0f;
        float manaDelay = 0.0f;

        float dashCharges = 2.0f;     // rolls and air dashes left; the fraction is the next one refilling

        std::vector<StatusState> statuses;

        /** The spells on the spell keys (abilities "spell:1".."spell:3"): recipes the player made or picked. */
        std::vector<SpellRecipe> spells;

        // The arena run: the stat items taken (ids, in order) and what they add up to
        std::vector<std::string> boons;
        struct Boons
        {
            float maxHealth = 0.0f, damage = 0.0f, manaCost = 0.0f, manaRegen = 0.0f, staminaRegen = 0.0f;
            float speed = 0.0f, cooldown = 0.0f, lifeOnHit = 0.0f, armor = 0.0f, crit = 0.0f, pickupRange = 0.0f, regen = 0.0f;
            int dashCharges = 0, statusStacks = 0;
        } boon;

        // ... shards (money and XP), the level, what is owned (weapons, skills, keywords, "element:fire"),
        // the shop between waves, and the free stat items of a level up
        int shards = 0, xp = 0, level = 1, levelUps = 0;
        std::vector<std::string> owned;
        struct ShopOffer
        {
            std::string id;           // a stat item (upgrades.json) or an unlock
            int price = 0;
            bool locked = false, bought = false;
        };
        std::vector<ShopOffer> shop;
        std::vector<std::string> levelOffer;
        bool ready = false;
        int rerolls = 0;

        bool elite = false;           // an enemy of a run: tougher, harder hitting, worth more
        bool gone = false;            // a dead enemy of a run: taken out of the arena at the end of the step

        // In the air: what was used since the last landing, and a wall jump's push not to steer against
        int airJumpsUsed = 0;
        int airDashesUsed = 0;
        int wallLock = 0;             // frames left
        glm::vec3 wallNormal = glm::vec3(0.0f);

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
        int viewTick = -1;              // network: the tick its owner saw enemies at (lag compensation)
        int dodgeStamp = -1;            // network: the tick its owner pressed the current dodge at
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

        bool attackIframes = false;   // in the invulnerable frames of its attack (AttackDef::iframeFrom), set each step

        // A charge: the key that holds it (a weapon skill's slot, or CHARGE_LIGHT / CHARGE_HEAVY for the
        // light or heavy attack that started on that button), and how long it has been held
        static constexpr int CHARGE_LIGHT = -2, CHARGE_HEAVY = -3;
        int chargeSlot = -1;
        int chargeFrames = 0;
        /** 0 (none) to 1 (full) of the current attack's charge. */
        float chargeRatio(const AttackDef* a) const { return a and a->charge.frames > 0 ? std::min(1.0f, static_cast<float>(chargeFrames) / static_cast<float>(a->charge.frames)) : 0.0f; }

        bool alive() const { return action != Action::Dead; }
        bool invulnerable(const Tuning& t) const
        {
            // Read after the step advanced the counter: the frame just played is frame - 1
            return (action == Action::Dodge and frame - 1 >= t.dodge.iframeStart and frame - 1 < t.dodge.iframeEnd) or
                   (action == Action::Attack and attackIframes);
        }
    };

    /**
     * Something worth a sound or particles: the game turns these into effects through effects.json.
     * kind: hit, stagger, kill, swing, windup, shoot, arrow_hit, dodge, jump, land, hard_land,
     *       chip, break, raise, dig, piece_fall, piece_land, air_jump, wall_jump, air_dash, plunge_land,
     *       cast, fire_burst, ice_burst, volt_arc (a line), ignite, freeze, status
     */
    struct WorldEvent
    {
        std::string kind;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 color = glm::vec3(1.0f);    // the block's colour for terrain events (from `block`)
        float strength = 1.0f;                // damage, fall speed...
        int fighter = 0;                      // who it happened to (0: nobody)
        BlockId block = AIR;                  // the block of a terrain event: over the network the block goes, each machine colours it
        int source = 0;                       // who made it happen, when not `fighter` (the attacker of a hit)
        int tick = 0;                         // the tick of the state it happened in (`steps` once its step is done)
        bool line = false;                    // from `position` to `to` (a volt arc)
        glm::vec3 to = glm::vec3(0.0f);
    };

    /** Something shot by an attack (see ProjectileDef). */
    struct Projectile
    {
        int id = 0;
        int owner = 0;
        Team team = Team::Enemy;
        std::string source;   // what shot it: the enemy type or the player's weapon; a spell's bolt: its element
        std::string attack;   // the attack (with `source`, finds its definition); a spell's bolt: the keyword
        ProjectileDef def;
        std::shared_ptr<const Spell> spell;   // a spell's bolt: the spell, and the stage it is (the next one comes from its impact)
        int stage = -1;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 velocity = glm::vec3(0.0f);
        float age = 0.0f;
    };

    /** A spell's zone on the ground: it hurts whoever stands in it, then the spell's next form comes out of it. */
    struct Zone
    {
        int id = 0;
        int owner = 0;
        Team team = Team::Player;
        Element element = Element::None;
        glm::vec3 position = glm::vec3(0.0f);   // on the ground, at its centre
        glm::vec3 dir = glm::vec3(0, 0, 1);     // the way the spell was going
        float radius = 2.0f;
        float duration = 4.0f;
        float remaining = 4.0f;
        float damage = 0.0f;                    // per second
        float poiseDamage = 0.0f;
        float heal = 0.0f;                      // a healing zone: health a second to its team's players in it
        float tick = 0.0f;
        std::shared_ptr<const Spell> spell;
        int stage = 0;
    };

    /**
     * Where the arena run is (see ArenaWorld::startRun). Armory: before the first wave, each player
     * picks a weapon. Duel: after the last wave, in multiplayer, everyone for themselves.
     */
    enum class RunPhase { Off, Fighting, Shop, Victory, Defeat, Duel, Armory };

    struct RunState
    {
        RunPhase phase = RunPhase::Off;
        int wave = 0;                 // 1-based, the current or last one
        int waves = 0;
        float timer = 0.0f;           // s left in the wave (a boss wave: until the boss falls)
        float elapsed = 0.0f;         // s of the run so far
        int score = 0;
        int kills = 0;
        int enemiesLeft = 0;
        int bossId = 0;
        bool bossWave = false;
        int champion = 0;             // the final duel's winner (0: none, or no duel)
        float warmup = 0.0f;          // s before the duel's fighters may hurt each other
        uint32_t seed = 1;
        float spawnDue = 0.0f;        // enemies owed to the spawn rate

        struct Spawn
        {
            std::string enemy;
            glm::vec3 position;
            float in = 1.0f;          // s until it comes out (its spawn point shows until then)
            bool boss = false, elite = false;
        };
        std::vector<Spawn> pending;
    };

    /** A shard an enemy dropped: it flies to a player in pickup range. */
    struct Pickup
    {
        int id = 0;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 velocity = glm::vec3(0.0f);
        int value = 1;
        int target = 0;               // the player it flies to (0: lying there)
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
        bool hunting = false;        // in an arena run: come for the target from anywhere (no engage range)
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
        /** withLocalPlayer: false for a dedicated server, whose players all come over the network. */
        void start(const Content& content, const Tuning& tuning, bool withLocalPlayer = true);

        /** New definitions after a hot reload: everyone keeps their place, stats are refreshed. */
        void setContent(const Content& content, const Tuning& tuning, bool rebuildArena);

        void setBrain(Brain* b) { brain = b; }

        void step(const InputFrame& input);

        /** One input per player fighter id; a player with none stands still for this step. */
        void step(const std::map<int, InputFrame>& inputs);

        /** A player for a peer (0 = this machine). Returns its fighter id. */
        int addPlayer(int owner);
        void removeFighter(int id);

        /** The alive player closest to a point (enemies target it), or nullptr. */
        const Fighter* nearestPlayer(const glm::vec3& at) const;
        Fighter* nearestPlayer(const glm::vec3& at);

        /**
         * false on a network client: only the fighters given an input are stepped (the predicted
         * local player); AI, hits, projectiles, terrain and abilities belong to the server.
         */
        bool authority = true;

        /** The player of this machine (player() returns it). */
        int localPlayerId = 0;

        /** Network: per peer, the ticks a hit on its player waits for a dodge pressed in time (0 = none). */
        std::map<int, int> defenderGrace;

        /** Bodies of the last ticks, for lag-compensated hits. */
        struct Snapshot
        {
            uint64_t tick;
            std::vector<std::pair<int, Body>> bodies;
        };
        std::vector<Snapshot> history;
        static constexpr size_t HISTORY = 40;

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

        /** The spells on a fighter's spell keys (the server checks nothing: a recipe that makes no spell does nothing). */
        void setSpells(Fighter& f, const std::vector<SpellRecipe>& spells);
        static constexpr int SPELL_SLOTS = 3;

        /** A recipe made into a spell, kept (cleared when the content changes). */
        std::shared_ptr<const Spell> spellOf(const SpellRecipe& recipe) const;
        /** The spell key a slot of the abilities is ("spell:2" -> 1), or -1. */
        int spellSlotAt(int index) const;

        std::vector<Zone> zones;

        // ---- The arena run (run.json): timed waves, shards, a shop between waves, bosses, a score ----
        RunState run;
        /** A run from the start: a fresh arena, the players as new, wave 1. */
        void startRun(uint32_t seed);
        /** Training: no run (the arena's own enemies, respawning). */
        void stopRun();
        /** Testing: straight to a wave of the run (the enemies now in the arena go). */
        void skipToWave(int wave);
        bool inRun() const { return run.phase != RunPhase::Off; }

        /**
         * Whether a hit from `team` (cast or struck by fighter `owner`) can land on `target`: the other
         * team, and in the final duel, once its countdown is over, every other player too.
         */
        bool hostile(Team team, int owner, const Fighter& target) const;
        /** The final duel: everyone revived, in a ring, the last one standing wins (run.json "duel"). */
        void beginDuel();
        /** A light (1) or heavy (2) press started the current attack: that button holds its charge. */
        void chargeOn(Fighter& f, int button);
        /** A player's weapon for the run (the armory, before the first wave). */
        void chooseWeapon(Fighter& f, const std::string& id);
        /** A player's START begins a run even outside one (a dedicated server: there is no title there). */
        bool runOnRequest = false;

        /** The arena being played: the run's own during a run (run.json "arena"), else arena.json. */
        const ArenaDef& arena() const { return useRunArena and content->run.hasArena ? content->run.arena : content->arena; }
        bool useRunArena = false;
        /** Goes up each time the arena is built anew: a server sends it whole, the game meshes it again. */
        int arenaBuilds = 0;
        /** Each player's place in the arena (a row across its spawn point). */
        void placePlayers();
        std::vector<Pickup> pickups;
        /** In a run, whether a player has bought (or started with) something; outside a run, everything is theirs. */
        bool owns(const Fighter& f, const std::string& id) const;
        /** Shards to the next level. */
        int xpFor(int level) const;
        /** Shards to a player: money and XP (its level ups with them). */
        void collect(Fighter& f, int value);
        /** What the shop's reroll costs this player now. */
        int rerollPrice(const Fighter& f) const;
        /** What a fighter's boons add up to (from its list: a client works it out the same way). */
        void sumBoons(Fighter& f) const;
        int maxDashes(const Fighter& f) const { return tuning.dash.charges + f.boon.dashCharges; }

        /** Puts a status on a fighter (stacks add up to its maxStacks): what a hit with one does. */
        void applyStatus(Fighter& target, const std::string& id, int stacks = 1);
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

        /** The ability in slot `index` for this fighter: the first of the slot its weapon allows. */
        const AbilityDef* ability(int index, const Fighter* f = nullptr) const;
        /** An attack by name: of the fighter's moveset, or a weapon skill's ("@<ability id>"). */
        const AttackDef* attackDef(const Fighter& f, const std::string& name) const;

        bool godMode = false;         // the player takes no damage
        bool infiniteStamina = false; // players spend no stamina (testing moves)
        bool infiniteMana = false;    // players spend no mana (testing spells)
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
        void startAirDash(Fighter& f, const glm::vec3& dir);
        void spendDash(Fighter& f);
        void resolveHits();
        void applyHit(Fighter& attacker, Fighter& target, const AttackDef& attack);
        void separate();
        void hitOrDefer(Fighter& attacker, Fighter& target, const AttackDef& attack, const glm::vec3& point);
        void resolvePendingHits();
        const Body& bodyFor(const Fighter& attacker, const Fighter& target) const;
        void recordHistory();

        struct PendingHit
        {
            Fighter attacker;    // a copy: the attacker may be gone when it resolves
            int target;
            AttackDef attack;
            glm::vec3 point;
            uint64_t tick;
            uint64_t resolveAt;
        };
        std::vector<PendingHit> pendingHits;
        void useAbilities(Fighter& f, const InputFrame& in);
        bool castSpell(Fighter& f, int spellSlot, const InputFrame& in, int abilityIndex);
        void runStage(const std::shared_ptr<const Spell>& spell, int stage, const glm::vec3& point, const glm::vec3& dir, int owner, Team team, const InputFrame* aim);
        void strikeBeam(const std::shared_ptr<const Spell>& spell, int stage, const glm::vec3& start, const glm::vec3& hand, const glm::vec3& dir, int owner, Team team);
        void hitAround(const AttackDef& hit, const glm::vec3& centre, float radius, int owner, Team team, bool flat);
        /** Health back to the players this close (on the ground: a disc at the feet), the healer too, never above their most. */
        void healAround(const glm::vec3& centre, float radius, float amount, int owner, Team team, bool flat);
        void chainFrom(const AttackDef& hit, Fighter& first, int chains, int owner, Team team, Element element);
        void stepZones();
        void rideFallingPieces();
        void stepRun(const std::map<int, InputFrame>& inputs);
        void beginWave(int wave);
        void endWave();
        void rollShop(Fighter& f);
        void rollLevel(Fighter& f);
        void shopPick(Fighter& f, int pick);
        void takeBoon(Fighter& f, const std::string& id);
        void stepPickups();
        void spawnShards(const Fighter& from, int value);
        glm::vec3 edgePoint(float awayFromPlayers);
        float waveDuration(int wave) const;
        uint32_t runRandom();
        int nextPickup = 1;
        mutable std::map<std::string, std::shared_ptr<const Spell>> spellCache;
        int nextZone = 1;
        void impact(const Projectile& p, const glm::vec3& point, Fighter* struck, const RayHit* wall);
        void elementOnTerrain(Element element, const glm::vec3& point, float radius);
        AttackDef hitOf(const std::string& name, float damage, float poiseDamage, float knockback, int hitstop, float shake, Element element, const StatusHit& status) const;
        void stepStatuses(Fighter& f);
        void die(Fighter& target, int attacker);
        float speedScale(const Fighter& f) const;
        float heightAboveGround(const Fighter& f) const;
        glm::vec3 touchedWall(const Fighter& f) const;
        const std::string& attackFor(const Fighter& f, const Moveset& m, int queued) const;
        void regenMana(Fighter& f);
        void fireProjectiles();
        void stepProjectiles();
        void event(const std::string& kind, const glm::vec3& position, float strength = 1.0f, int fighter = 0, BlockId block = AIR, int source = 0);
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
