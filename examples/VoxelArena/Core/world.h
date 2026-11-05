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

    /** How long a step takes in real time: STEP_SECONDS of the game, slowed (or sped up) by the tuning's feel.gameSpeed. */
    inline double stepRealMs(const Tuning& t) { return STEP_SECONDS * 1000.0 / static_cast<double>(t.feel.gameSpeed); }

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
         * WEAPON + i takes weapon i (Content::weaponOrder) before the first wave, FIRST_SPELL + i the
         * first spell i of run.json's chooseSpell (after the weapon), PRESENT + i takes a present's pick i,
         * DIFFICULTY + i plays the run at difficulty level i (before it starts), BACK leaves that choice.
         * LEVEL_REROLL rolls a level up's picks again, BANISH + i takes level-up pick i out of the run.
         */
        int pick = 0;
        static constexpr int START = 9, BUY = 1, LOCK = 11, REROLL = 20, LEVEL_REROLL = 21, LEVEL = 31, WEAPON = 41, BANISH = 51, PRESENT = 61, FIRST_SPELL = 71;
        static constexpr int BACK = 80, DIFFICULTY = 81;

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
        int source = 0;           // who left it (the server's: what it kills is theirs)
        float age = 0.0f;         // s since it first landed (the server's)
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
        float comboCooldown = 0.0f;   // s before an element combo can go off on it again (the server's)

        /** The spells on the spell keys (abilities "spell:1".."spell:3"): recipes the player made or picked. */
        std::vector<SpellRecipe> spells;

        // The arena run: the items taken (in order, each with its rarity, a spell item with its spell)
        // and what the stat ones add up to
        struct Boon
        {
            std::string id;
            int rarity = 0;           // into the run's rarities

            Boon() = default;
            Boon(std::string id, int rarity = 0) : id(std::move(id)), rarity(rarity) {}
            bool operator==(const std::string& other) const { return id == other; }
            bool operator==(const Boon& o) const { return id == o.id and rarity == o.rarity; }
        };
        std::vector<Boon> boons;

        // ... and the spells it was given (level ups), on the spell keys in order
        struct RunSpell
        {
            std::string id;           // spells/*.json (a technique: abilities/*.json; the weapon in hand: weapons/*.json)
            int level = 1;
            int branch = -1;          // the branch taken at the branch level (-1: none yet)
        };
        std::vector<RunSpell> runSpells;
        // ... and its weapon's techniques it was given, with their levels (owned too)
        std::vector<RunSpell> runSkills;
        struct Boons
        {
            float maxHealth = 0.0f, damage = 0.0f, manaCost = 0.0f, manaRegen = 0.0f, staminaRegen = 0.0f;
            float speed = 0.0f, cooldown = 0.0f, lifeOnHit = 0.0f, armor = 0.0f, crit = 0.0f, pickupRange = 0.0f, regen = 0.0f, attackSpeed = 0.0f;
            float horde = 0.0f, xpGain = 0.0f, shardChance = 0.0f;
            int dashCharges = 0, statusStacks = 0;
            float comboDamage = 0.0f, statusDuration = 0.0f;
            Payoff payoff;                              // more damage against statuses
            std::vector<UpgradeDef::Burst> bursts;      // where an enemy it kills dies with a status (damage times rarity)
            float healthScale = 0.0f, damageTaken = 0.0f;
            float hurtDamage = 0.0f, airDamage = 0.0f, executeDamage = 0.0f, revengeDamage = 0.0f;
            float thorns = 0.0f, killHeal = 0.0f;
            GroundDef dodgeGround;                      // the widest, their damage added up
            UpgradeDef::Proc proc;                      // the most often, the most jumps, their damage added up
            UpgradeDef::Landing landing;                // the widest, their damage added up
            // The items whose triggers these are: what their hits are counted under in the recap
            std::string thornsItem, procItem, landingItem, dodgeItem;
        } boon;

        // ... shards (money and XP), the level, what is owned (weapons, skills, keywords, "element:fire"),
        // the shop between waves, and the free stat items of a level up
        int shards = 0, xp = 0, level = 1, levelUps = 0;
        float xpCarry = 0.0f;         // XP short of a whole one (more XP per shard comes in parts)
        std::vector<std::string> owned;
        struct ShopOffer
        {
            std::string id;           // a stat item (upgrades.json) or an unlock
            int price = 0;
            bool locked = false, bought = false;
            int rarity = 0;           // an item's
        };
        std::vector<ShopOffer> shop;
        int presents = 0;             // presents picked up, not opened yet: each a free item of presentOffer
        std::vector<ShopOffer> presentOffer;
        int banked = 0;               // shards kept through the last shop (they earn the interest)
        bool armed = false;           // a run's weapon was chosen (the first spell comes next)
        int interest = 0;             // shards the last interest gave
        std::vector<std::string> levelOffer;   // a level up's picks: a spell (new, or its next level) or a weapon skill
        bool ready = false;
        int rerolls = 0;
        int levelRerolls = 0, banishes = 0;    // a run's rerolls and banishes of the level ups left
        std::vector<std::string> banished;     // what the level ups no longer offer (spells, techniques, the weapon)
        int procHits = 0;                      // weapon hits towards the next proc (an item's every n-th hit)

        // ... and what it did in the run, for the recap at its end
        struct RunStats
        {
            int kills = 0;            // enemies it finished off
            int shards = 0, xp = 0;   // picked up by this player (everyone gets them), and the XP it got
            int presents = 0;         // picked up
            int items = 0;            // bought in the shop
            int downs = 0;            // times it fell
            float damage = 0.0f;      // dealt (to enemies, and to the others in the duel)
            float healing = 0.0f;     // given (to itself and the others)
            /** The damage it dealt by what dealt it: a weapon, a spell, a technique, a combo, a status ("base:burn"), an item ("thorns"). */
            std::map<std::string, float> bySource;
        } stats;

        bool elite = false;           // an enemy of a run: tougher, harder hitting, worth more
        std::vector<std::string> affixes;   // an elite's (affixes.json)
        float trailTimer = 0.0f;      // s to the next of an affix's trail or aura (the server's)
        bool split = false;           // came out of another one splitting: it does not split again
        int phase = 0;                // a boss: the phases it has gone into (EnemyDef::phases)
        bool shielded = false;        // a boss: nothing hurts it until its crystals are broken
        std::vector<int> crystals;    // ... the crystals holding its shield (the server's)
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

        // An enemy climbing a wall (out of a pit): the height its feet go up to (< 0: not climbing), towards where
        float climbTo = -1.0f;
        float climbSpeed = 0.0f;   // m/s up (the tuning's climb: its size, the wall's height)
        float climbWait = 0.0f;    // s before it tries to climb again, after a climb dropped
        bool blownUp = false;      // dead, and its blast went off: nothing of it is left to draw
        glm::vec3 climbDir = glm::vec3(0.0f);

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
        int bounces = 0;          // skips off the floor left before its impact
        float bounciness = 0.7f;  // the share of its speed each skip leaves it
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
        StatusHit status;                       // a zone with no spell (ground): what it leaves
        std::string source;                     // ... and what its hits are counted under in the recap
        BlastDef blast;                         // a blast's warning: it hurts nobody, and goes off as it ends
        int killer = 0;                         // ... the one who killed what left it (a blast that hurts everyone counts for them)
        bool blasting() const { return blast.radius > 0.0f; }
    };

    /** Life on hit from an area's, a zone's or a chain's hit: this share of it. */
    constexpr float AREA_LEECH = 0.2f;
    /** A status this young (s) is not detonated: a spell does not cash in the burn it just left. */
    constexpr float DETONATE_AFTER = 0.25f;

    /** A boost onto an attack: damage (and heals), area (hitboxes, heal radii), duration (heal zones), projectile speed and count. */
    void applyBoost(AttackDef& attack, const SpellTweak& boost);
    /** An attack quicker by `speed` (0.1: 10%): its frames fewer, its lunges faster (as far in less time). */
    void hasten(AttackDef& attack, float speed);
    /** A branch's element and status onto an attack's hits (and its projectile's). */
    void applyBranch(AttackDef& attack, const BranchDef* branch);

    /**
     * Where the arena run is (see ArenaWorld::startRun). Difficulty: before it starts, one player picks
     * its level (see ArenaWorld::chooseDifficulty). Armory: before the first wave, each player picks a
     * weapon. Duel: after the last wave, in multiplayer, everyone for themselves.
     */
    enum class RunPhase { Off, Fighting, Shop, Victory, Defeat, Duel, Armory, Difficulty };

    struct RunState
    {
        RunPhase phase = RunPhase::Off;
        int wave = 0;                 // 1-based, the current or last one
        int waves = 0;
        int difficulty = 0;           // the level (RunDef::difficulties) of this run; outside one, the next run's
        int levelsOpen = 1000;        // the levels that may be picked (the host's progress; a dedicated server: all)
        int chooser = 0;              // the Difficulty phase: the player who picks (the host's, else the first one in)
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
        int presentsDropped = 0;      // this wave's

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
        bool present = false;         // an elite's present, not shards
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
        int alliesHurt = 0;          // other enemies within 10 m with less than 80% of their health (a healer's to mend)
        float targetFacing = 0.0f;   // degrees between where the target looks and self: 0 in its face, 180 behind it
        bool hunting = false;        // in an arena run: come for the target from anywhere (no engage range)
        bool climbable = false;      // in a hole it can climb out of (walking into its wall climbs it)
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

        /** Outside a run: the training ground becomes this arena ("": arena.json), built anew with everyone at its spawn. */
        void showArena(const std::string& id);
        /** Where to stand at a point: its x and z on the nearest floor with room above (from a little over it, down). */
        glm::vec3 groundNear(glm::vec3 point) const;

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
        /**
         * A run asked for: first its difficulty level, picked by one player (the host's, or on a dedicated
         * server the first one in) among the open ones, then startRun. With one level, startRun at once.
         */
        void chooseDifficulty(uint32_t seed);
        /** Who picks the difficulty: the player of this machine (the host's), else the first one in (0: nobody). */
        int difficultyChooser() const;
        /** Training: no run (the arena's own enemies, respawning). */
        void stopRun();
        /** Testing: straight to a wave of the run (the enemies now in the arena go). */
        void skipToWave(int wave);
        bool inRun() const { return run.phase != RunPhase::Off; }
        /** A player's levels and branches (runSkills) count: in a run, or anywhere in the lab (growsOutsideRun). */
        bool grows() const { return inRun() or growsOutsideRun; }

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
        /** The first spell, among run.json's chooseSpell: on key 1. */
        void chooseFirstSpell(Fighter& f, const std::string& id);
        /** A player's START begins a run even outside one (a dedicated server: there is no title there). */
        bool runOnRequest = false;

        /** The arena built (the grid is made from it): the wave's during a run, the training one's else. */
        const ArenaDef& arena() const { return content->arenaById(arenaId); }
        std::string arenaId;          // its id ("": the training arena, arena.json)
        bool useRunArena = false;     // in a run: buildArena() builds the wave's arena (the duel's in the duel)
        std::string trainingArena;    // outside a run: built instead of arena.json ("": arena.json)
        /** Goes up each time the arena is built anew: a server sends it whole, the game meshes it again. */
        int arenaBuilds = 0;
        /** Each player's place in the arena (a row across its spawn point). */
        void placePlayers();
        std::vector<Pickup> pickups;
        /** In a run, whether a player has bought (or started with) something; outside a run, everything is theirs. */
        bool owns(const Fighter& f, const std::string& id) const;
        /** Shards to the next level. */
        int xpFor(int level) const;
        /** Shards to a player: money and XP (its level ups with them). pickedUp: it picked them up itself (its run stats). */
        void collect(Fighter& f, int value, bool pickedUp = true);
        /** Shards a player picked up: every player gets them, money and XP (the prices go up with the players, see priceScale). */
        void shareShards(Fighter& picker, int value);
        /** The shop's offers rolled anew (the locked ones kept), and a level up's picks. */
        void rollShop(Fighter& f);
        void rollLevel(Fighter& f);
        void rollPresent(Fighter& f);
        /** What the shop's reroll costs this player now. */
        int rerollPrice(const Fighter& f) const;
        /** An item taken (bought, a present's, or given by the console): its numbers at once. */
        void takeBoon(Fighter& f, const Fighter::Boon& boon);
        /** What a fighter's boons add up to (from its list: a client works it out the same way). */
        void sumBoons(Fighter& f) const;
        /** Whether one more of an item would do nothing for a player: every number of it at its cap (armor, cooldowns, costs, crit). */
        bool itemMaxed(const Fighter& f, const UpgradeDef& item) const;
        /** What a rarity multiplies an item's numbers by (the run's rarities; 1 without). */
        float rarityPower(int rarity) const;

        /**
         * The spell on a spell key: in a run, the one given to the player (its recipe grown by its
         * levels, its spell items on it); outside one, the player's own recipe.
         */
        SpellRecipe spellRecipe(const Fighter& f, int slot) const;
        /** A run's spell at a level: its recipe and the keywords of each level up to it. */
        SpellRecipe spellAtLevel(const std::string& spellId, int level) const;

        // ---- Growing in a run: levels, branches, tags ----
        /**
         * A spell or technique given in a run, as it is now: its recipe grown by its written levels,
         * its branch's keywords, a mastery boost for each level past those (and past the branch), the
         * branch's boost, and (with a player) the boosts of the player's tag items that reach it.
         */
        SpellRecipe spellAt(const Fighter::RunSpell& spell, const Fighter* f = nullptr) const;
        /** All a technique gets in a run (levels, mastery, branch, the player's tag items); empty outside one. */
        SpellTweak techniqueBoost(const Fighter& f, const std::string& abilityId) const;
        /** A technique's attack with a boost (and a branch's element and status) on it. */
        AttackDef boostedAttack(const AbilityDef& a, const SpellTweak& boost, const BranchDef* branch) const;
        /** What tag items reach: a spell's ("spell", its element, "projectile", "area", "zone", "beam"), a technique's ("technique" and its own). */
        std::vector<std::string> spellTags(const SpellRecipe& recipe) const;
        std::vector<std::string> techniqueTags(const AbilityDef& a, int branch) const;
        /** A weapon's attack's (a run's player's light and heavy): "attack", the weapon's role ("melee") and type, "projectile" when it shoots,
            and its branch's element and tags. */
        std::vector<std::string> attackTags(const Fighter& f, const AttackDef& attack) const;
        /** All a run's player's weapon attack gets: the weapon's levels, mastery and branch, the tag items reaching it; empty outside a run. */
        SpellTweak weaponBoost(const Fighter& f, const AttackDef& attack) const;
        /** The branch the weapon in hand took in the run, if any. */
        const BranchDef* weaponBranch(const Fighter& f) const;
        /** The boosts of a player's tag items reaching something with these tags. */
        SpellTweak tagBoost(const Fighter& f, const std::vector<std::string>& tags) const;
        /** The level a spell or technique with this many written levels (and branches) takes its branch at. */
        int branchAt(int writtenLevels) const;
        /** A level up's pick, split: "base:firebolt#1" is branch 1 of the firebolt (-1: none). */
        static std::string offerTarget(const std::string& offer, int& branch);
        const Fighter::RunSpell* runSkill(const Fighter& f, const std::string& abilityId) const;
        /** Shop prices at a wave, times this (run.json shop.priceScale), and more with more players (shop.coopPrice). */
        float priceScale(int wave) const;
        /** The difficulty level of the run (outside one: of the next). */
        const DifficultyDef& difficulty() const { return content->run.difficulty(run.difficulty); }
        /** Enemies' health and damage at a wave, times these: the difficulty level's. */
        float enemyHealthScale(int wave) const;
        float enemyDamageScale(int wave) const;
        /** What the players' items add for everyone: more enemies, more shards from each kill (with run.json's shardFalloff). */
        float teamHorde() const;
        /** Extra damage (0.5: +50%) from items and mastery, with run.json's diminishing returns (damageFalloff). */
        float damageFalloff(float extra) const;
        float teamShardChance() const;
        int maxDashes(const Fighter& f) const { return tuning.dash.charges + f.boon.dashCharges; }
        /** Health a second a fighter gets back: its regen items and its weapon's mending. */
        float healthRegen(const Fighter& f) const;
        /** How fast a fighter moves, of its walk speed: statuses slow it, items and affixes speed it up. */
        float speedScale(const Fighter& f) const;
        /** A player's most health: the tuning's and its items' (a trade-off item's share off). */
        float playerMaxHealth(const Fighter& f) const;

        // ---- Elites' affixes (affixes.json) and what dies with a blast or splits ----
        /** An elite's affixes, picked by the run's dice: `count` of them, each once. */
        std::vector<std::string> rollAffixes(int count);
        /** What a fighter's affixes add up to (health, damage, speed, regen...). */
        float affixSum(const Fighter& f, float AffixDef::*field) const;
        /** A blast where something died: its warning ring now, its hit as the ring runs out. owner: whom its hits count for. */
        void startBlast(const BlastDef& blast, const glm::vec3& at, int owner, Team team, const std::string& source, int killer = 0);
        /** Ground left on the floor at a point (a trail, a puddle, a roll). */
        void leaveGround(const GroundDef& g, const glm::vec3& at, int owner, Team team, const std::string& source);

        /** A hit landing: damage (boons, weakness, crits, armor), life on hit, poise, knockback, a status, a death. */
        void applyHit(Fighter& attacker, Fighter& target, const AttackDef& attack);
        /** Puts a status on a fighter (stacks add up to its maxStacks): what a hit with one does. */
        void applyStatus(Fighter& target, const std::string& id, int stacks = 1, int source = 0);

        /** Combos (combos.json) the status would set off on the target: their hits go off, and whether one used the
         *  status up. `hitDamage`: the damage of the hit bringing it, before the attacker's items. */
        bool setOffCombo(Fighter& attacker, Fighter& target, const std::string& status, float hitDamage);

        /** What combos and the items' bursts do: grown with the waves as the enemies' health is (1 outside a run). */
        float comboScale() const;
        /** A combo's damage, set off by a hit of `hitDamage` (before the attacker's items). */
        float comboDamageOf(const ComboDef& c, const Fighter& attacker, float hitDamage) const;
        static bool hasStatus(const Fighter& f, const std::string& id);
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
        bool growsOutsideRun = false; // the lab: a weapon's and a technique's levels and branches count in training too

        float time = 0.0f;
        uint64_t steps = 0;

        const Content* content = nullptr;
        Tuning tuning;

        /**
         * An input as the world may trust it: a remote player's comes from the network. The aim's ray
         * starts within the camera's reach of the player, and its direction is a unit vector (or none).
         */
        InputFrame sanitized(const Fighter& f, const InputFrame& in) const;

    private:
        void stepPlayer(Fighter& f, const InputFrame& in);
        void stepEnemy(Fighter& f);
        void stepCommon(Fighter& f, glm::vec3 wantedVelocity, bool canJump, const InputFrame* in);
        /**
         * An enemy walking into a wall it can climb starts up it, when it is in a hole or its target is above it; one
         * climbing goes on, or stops. Returns true while it climbs.
         */
        bool stepClimb(Fighter& f, const EnemyDef& def, const glm::vec3& wanted, bool targetAbove);
        /** Walls on three sides of the fighter at least (a pit, a pen): what it may climb out of. */
        bool inHole(const Fighter& f) const;
        /**
         * Blocks up to a ledge it can climb onto over the nearest wall along dir, within 3 m and anywhere across its width
         * (0: none, a step, or too high); wallAt: how far that wall is.
         */
        int ledgeAbove(const Fighter& f, const glm::vec3& dir, float* wallAt = nullptr) const;
        /** Offsets across a fighter's width, to look at what its whole side touches. */
        std::vector<float> across(const Fighter& f) const;
        /** pitch: NAN aims at the target (aim assist's for the player), a number aims there (first person). */
        void startAttack(Fighter& f, const std::string& name, const glm::vec3& aimDir, bool aimAssist = true, float pitch = NAN);
        void startDodge(Fighter& f, const glm::vec3& dir);
        void startAirDash(Fighter& f, const glm::vec3& dir);
        void spendDash(Fighter& f);
        void resolveHits();
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
        void shopPick(Fighter& f, int pick);
        void takeLevel(Fighter& f, const std::string& id);
        /**
         * What a level up may offer now: new things (a spell while a key is free, a technique of the weapon in
         * hand) and growing ones (each a list: a branch choice is all its branches), none banished, shuffled.
         */
        void levelCandidates(Fighter& f, std::vector<std::string>& fresh, std::vector<std::vector<std::string>>& upgrades);
        /** A level up's pick i out of the run: another one in its place (of the same kind when there is one). */
        void banishPick(Fighter& f, size_t index);
        /** Items for sale (stat and tag items, rarity rolled), not already in `kept`, at most n. */
        std::vector<Fighter::ShopOffer> rollItems(const Fighter& f, int n, const std::vector<Fighter::ShopOffer>& kept, bool unlocks);
        void stepPickups();
        void spawnShards(const Fighter& from, int value);
        glm::vec3 edgePoint(float awayFromPlayers);
        float waveDuration(int wave) const;
        uint32_t runRandom();
        int nextPickup = 1;
        mutable std::map<std::string, std::shared_ptr<const Spell>> spellCache;
        // A run's techniques, boosted, by fighter and ability (the key: what the boost was)
        mutable std::map<std::string, std::pair<std::string, std::shared_ptr<AttackDef>>> techniqueCache;
        int nextZone = 1;
        void impact(const Projectile& p, const glm::vec3& point, Fighter* struck, const RayHit* wall);
        void elementOnTerrain(Element element, const glm::vec3& point, float radius);
        AttackDef hitOf(const std::string& name, float damage, float poiseDamage, float knockback, int hitstop, float shake, Element element, const StatusHit& status) const;
        void stepStatuses(Fighter& f);
        /** reward: the kill counts (shards, score); an enemy that blew itself up gives nothing. */
        void die(Fighter& target, int attacker, bool reward = true);
        /** Every step: the elites' trails, auras and regeneration. */
        void stepAffixes(Fighter& f);
        /** Zones and enemies that deaths left for after the step's hits (nothing moves the lists under a hit). */
        void flushDeaths();
        std::vector<Zone> zonesDue;
        struct SpawnDue
        {
            std::string enemy;
            glm::vec3 position;
            float yaw = 0.0f;
            float health = 1.0f;      // of its own
        };
        std::vector<SpawnDue> spawnsDue;
        /** A blast going off: the hit around it (both sides when it hurts everyone), the terrain. */
        void detonate(const Zone& z);
        /** The damage a hit is counted under in the recap (see RunStats::bySource). */
        std::string sourceOf(const Fighter& attacker, const AttackDef& a) const;
        /** A boss's crystal and the like: nothing pushes it, nothing mends it. */
        bool anchoredEnemy(const Fighter& f) const;

        // ---- A boss's phases (EnemyDef::phases) ----
        /**
         * The health a boss cannot be taken under before its next phase: the highest of its phase lines still
         * under its health (one set lower by hand, the console's kill, has gone past the ones above). 0: none
         * left, it can die. phase: the phase that line begins.
         */
        float phaseFloor(const Fighter& f, int* phase = nullptr) const;
        /** Damage as it lands on f: none on a shield, never past a phase (which then begins). Returns what it took. */
        float takeDamage(Fighter& f, float damage);
        /** The bosses whose shields went up this step: their crystals rise once the step's hits are done. */
        std::vector<int> crystalsDue;
        /** Crystals for the shields that went up, and shields whose crystals are all broken come down. */
        void stepPhases();
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
        /** Air, or a decoration a block put there pushes aside. */
        bool replaceable(const glm::ivec3& cell) const;
        void liftOut(const glm::ivec3& cell);
        void respawn(Fighter& f);
        void spendStamina(Fighter& f, float amount);
        void regenStamina(Fighter& f);
        void turnTowards(Fighter& f, const glm::vec3& dir, float degreesPerSecond);

        Brain* brain = nullptr;
        int nextId = 1;
    };
}
