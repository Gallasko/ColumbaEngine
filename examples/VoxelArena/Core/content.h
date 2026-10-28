#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "json.h"
#include "voxelgrid.h"

namespace arena
{
    /** Version of the content API the game offers. Mods name the one they were written for. */
    constexpr int CONTENT_API_VERSION = 1;

    struct ContentMessage
    {
        std::string file;   // path as given to the loader
        int line = 0;       // 0 when the message is about the whole file
        std::string text;

        std::string str() const;
    };

    /** Everything that went wrong while loading. Loading never throws; a bad entry is skipped and reported here. */
    struct ContentReport
    {
        std::vector<ContentMessage> errors;
        std::vector<ContentMessage> warnings;
        std::vector<std::string> info;

        void error(const std::string& file, int line, const std::string& text) { errors.push_back({file, line, text}); }
        void warn(const std::string& file, int line, const std::string& text) { warnings.push_back({file, line, text}); }
    };

    // --------------------------------------------------------------------------------------------
    // Mods
    // --------------------------------------------------------------------------------------------

    struct ModInfo
    {
        std::string id;          // namespace of everything it defines
        std::string name;
        std::string version;
        int api = 0;
        std::vector<std::string> dependencies;   // must be present, load first
        std::vector<std::string> loadAfter;      // load first when present
        std::string dir;         // folder of the mod
    };

    /**
     * Joins a reference to an ID. "stone" inside mod "base" is "base:stone";
     * a reference that already has a namespace is kept as is.
     */
    std::string resolveId(const std::string& ref, const std::string& currentMod);

    /** "base:stone" -> {"base", "stone"}. A reference with no namespace gives an empty first part. */
    std::pair<std::string, std::string> splitId(const std::string& id);

    // --------------------------------------------------------------------------------------------
    // Definitions
    // --------------------------------------------------------------------------------------------

    /** A definition as read from a file, before patches are applied and before it is typed. */
    struct RawDef
    {
        std::string id;
        std::string mod;     // mod that defined (or last replaced) it
        std::string file;
        Json data;
        std::vector<std::string> patchedBy;
    };

    struct BlockDef
    {
        std::string id;
        BlockId runtimeId = 0;
        BlockLook look;
    };

    /** An axis-aligned box in a fighter's local space: x right, y up, z forward. */
    struct HitBox
    {
        glm::vec3 offset = glm::vec3(0.0f);   // centre
        glm::vec3 size = glm::vec3(1.0f);
    };

    /** How the weapon model moves during an attack (degrees). Pure visuals. */
    struct SwingPose
    {
        float yawFrom = 0.0f, yawTo = 0.0f;       // around the body's up axis, 0 = straight ahead
        float pitchFrom = 0.0f, pitchTo = 0.0f;   // blade elevation, + = up
        float reach = 0.0f;                       // pushes the weapon forward along the swing (thrusts)
    };

    /** What a hit is made of, beyond its force: fire burns, ice freezes, volt shocks (see StatusDef). */
    enum class Element { None, Fire, Ice, Volt };
    constexpr int ELEMENTS = 4;

    /** "fire", "ice", "volt" (or "" for none). False for any other name. */
    bool elementFromName(const std::string& name, Element& out);
    const char* elementName(Element e);

    /**
     * Something a hit leaves on its target for a while (statuses.json): burning, chilled, frozen,
     * shocked. Everything it does is here, so a mod makes new ones without code.
     */
    struct StatusDef
    {
        std::string id;
        float duration = 3.0f;         // seconds, renewed by a new hit
        float damagePerSecond = 0.0f;  // burning
        float slow = 0.0f;             // move speed taken away, per stack (0.15 = 15%)
        int maxStacks = 1;
        std::string atMax;             // reaching maxStacks turns it into this status (chill -> frozen)
        int stagger = 0;               // frames of stagger when it lands (0: none): interrupts
        bool heavy = false;            // that stagger cannot be rolled out of (frozen solid)
    };

    /** A status a hit applies: which, and how many stacks. */
    struct StatusHit
    {
        std::string status;            // "" = none
        int stacks = 1;
    };

    // --------------------------------------------------------------------------------------------
    // Spells: an element, then keywords (elements.json, keywords.json; ready-made ones in spells/)
    // --------------------------------------------------------------------------------------------

    /** What every spell of an element carries (fire burns...), and what it adds to the cost. */
    struct ElementDef
    {
        Element element = Element::None;
        std::string name;
        StatusHit status;
        float mana = 0.0f;
    };

    /**
     * A keyword of a spell. A form is a shape the spell takes: the first one leaves from the caster,
     * each later one comes out of the one before it. A modifier changes the last form before it.
     */
    struct KeywordDef
    {
        std::string id, name;
        bool form = true;
        std::string shape;              // form: "bolt", "zone", "nova" or "beam"
        float mana = 0.0f;              // added to the spell's cost (per copy of its form)
        float manaScale = 1.0f;         // modifier: multiplies the cost of its form
        float cooldown = 0.0f;          // added to the spell's cooldown

        // A form's numbers
        float speed = 28.0f;            // bolt, m/s
        float gravity = 2.0f;           // bolt, m/s^2
        float radius = 0.2f;            // bolt: its size; zone, nova: their reach
        float range = 30.0f;            // bolt: how far it flies; beam: its reach
        float duration = 4.0f;          // zone, s
        float damage = 10.0f;           // zone: per second
        float poiseDamage = 8.0f;
        float knockback = 3.0f;
        float terrainDamage = 0.0f;     // to the block it strikes

        // A modifier's: multipliers of its form's numbers (1 changes nothing)
        int count = 1;                  // copies of the form
        float durationScale = 1.0f, rangeScale = 1.0f, radiusScale = 1.0f, damageScale = 1.0f;
        int chains = 0;                 // jumps to more enemies after a hit
        std::vector<std::string> forms; // the shapes it can follow (empty: any)

        // Looks: the model a bolt flies as, by element
        std::map<std::string, std::string> model;
        float modelScale = 0.08f;
    };

    /** The words of a spell: "fire", then keyword ids ("base:bolt", "base:zone"). */
    struct SpellRecipe
    {
        std::string element;
        std::vector<std::string> keywords;

        bool operator==(const SpellRecipe& o) const { return element == o.element and keywords == o.keywords; }
    };

    /** A ready-made spell (spells/<name>.json): a name and a recipe. */
    struct SpellDef
    {
        std::string id, name;
        SpellRecipe recipe;
    };

    /**
     * What an attack shoots, fired on its first active frame. An enemy aims it at the player's chest
     * (dropping for the arc and leading a moving target by `lead`); the player aims it with the crosshair.
     */
    struct ProjectileDef
    {
        bool enabled = false;
        float speed = 22.0f;          // m/s
        float gravity = 6.0f;         // m/s^2
        float radius = 0.12f;
        float lifetime = 3.0f;        // s
        float lead = 0.5f;            // 0 shoots where the target is, 1 where it will be
        float damage = 10.0f;
        float poiseDamage = 8.0f;
        float knockback = 3.0f;
        float terrainDamage = 15.0f;  // to the block it hits
        int hitstop = 3;
        float shake = 0.15f;
        glm::vec3 spawn = glm::vec3(0.0f, 1.4f, 0.6f);   // fighter space: where it leaves from
        std::string model;            // model id drawn along the flight, "" = none
        float modelScale = 0.0625f;

        Element element = Element::None;
        StatusHit status;
        float burst = 0.0f;           // > 0: on impact, hits every enemy this close (m), not only the one struck
        float terrainRadius = 0.0f;   // fire sets alight, ice freezes pits over, this close to the impact
    };

    /**
     * One attack. Frame counts are at 60 frames per second.
     * Timeline: [startup][active][recovery]. Hitboxes only exist during active frames.
     */
    struct AttackDef
    {
        std::string name;

        int startup = 10;
        int active = 4;
        int recovery = 16;

        float damage = 10.0f;
        float poiseDamage = 10.0f;
        float staminaCost = 0.0f;

        float knockback = 4.0f;   // horizontal speed given to the target, m/s
        float knockup = 0.0f;     // vertical speed given to the target, m/s
        int hitstop = 3;          // frames both fighters freeze on hit
        float shake = 0.2f;       // camera shake added when the player lands or takes this hit

        std::vector<HitBox> hitboxes;

        // Forward motion of the attacker
        float lunge = 0.0f;       // m/s while lunging
        int lungeFrom = 0;
        int lungeTo = 0;
        bool dash = false;        // the lunge sets the speed outright (a dash), instead of speeding up to it

        float turnRate = 0.0f;    // degrees per frame the attacker may still turn during startup

        // Player movement while the attack runs. 0 roots the player (the default): the speed brakes
        // to zero, only the lunge moves. Above 0 the player keeps steering at that fraction of the
        // walk speed, with the normal run acceleration, so a swing started on the run keeps its momentum.
        float moveSpeed = 0.0f;
        int moveUntil = -1;       // from this frame on the player is rooted (-1: the whole attack)

        float armor = 0.0f;       // extra poise while the attack runs (hyper armor)
        float terrainDamage = 0.0f;   // damage to every breakable block its hitboxes touch (once per swing)

        // Cancels, as frame numbers counted from the start of the attack
        int dodgeCancelFrom = -1;  // -1: only once the attack has ended
        int chainFrom = -1;        // earliest frame the next attack in the combo may start (-1: end of active)
        int bufferFrom = 0;        // earliest frame a press is remembered for the chain

        std::string nextLight;     // attack started by a light press during the chain window
        std::string nextHeavy;

        SwingPose swing;

        ProjectileDef projectile;

        Element element = Element::None;
        StatusHit status;

        // In the air: gravity while the attack runs (an air slash hangs a little), and the plunge:
        // above 0, the attack dives at this speed once its startup is over and holds its first
        // active frame until it lands, the hitboxes then strike around the landing
        float gravityScale = 1.0f;
        float plungeSpeed = 0.0f;

        // Movement: frames [iframeFrom, iframeTo) the attacker cannot be hit and passes through
        // others (a dash through), and a leap: at frame leapAt the attacker is thrown `leap.x` m/s
        // along its facing (negative: backwards) and `leap.y` m/s up (a backflip)
        int iframeFrom = -1, iframeTo = -1;
        glm::vec2 leap = glm::vec2(0.0f);
        int leapAt = -1;

        // Looks: how the body turns over frames [from, to) of the attack, in degrees (pitch -360: a
        // backflip, yaw 360: a spin)
        struct Turn
        {
            float pitch = 0.0f, yaw = 0.0f;
            int from = 0, to = 0;
        } turn;

        // A charge (weapon skills): while the skill's key is held the attack holds at frame `at`, up to
        // `frames` frames; released (or full), it goes on with its lunge and damage scaled up to
        // `lunge` and `damage` times at a full charge
        struct Charge
        {
            int at = -1;
            int frames = 0;
            float lunge = 1.0f;
            float damage = 1.0f;
        } charge;

        bool tellHeavy = false;   // looks: an enemy's heavy attack shows a danger zone while it winds up ("tell": "heavy")

        int total() const { return startup + active + recovery; }
        bool isActive(int frame) const { return frame >= startup and frame < startup + active; }
        bool movesAt(int frame) const { return moveSpeed > 0.0f and (moveUntil < 0 or frame < moveUntil); }
    };

    struct Moveset
    {
        std::map<std::string, AttackDef> attacks;

        std::string light;        // first light attack
        std::string heavy;
        std::string dodgeAttack;  // light pressed at the end of a dodge
        std::string dashAttack;   // light pressed while sprinting
        std::string airLight;     // light in the air
        std::string airHeavy;     // heavy in the air (a plunge)

        const AttackDef* find(const std::string& name) const;
    };

    struct WeaponDef
    {
        std::string id;
        std::string name;
        std::string type;        // "dagger", "sword"...: the weapon skills it can use (AbilityDef::weapons)
        std::string model;       // model id, "base:sword"
        float modelScale = 0.0625f;
        glm::vec3 grip = glm::vec3(0.0f);    // model-space point held in the hand (in model voxels)
        SwingPose rest;                       // pose when no attack runs
        Moveset moveset;
        std::string file;
    };

    struct EnemyDef
    {
        std::string id;
        std::string name;
        std::string model;
        float modelScale = 0.125f;

        float width = 0.9f;
        float height = 1.9f;

        float health = 100.0f;
        float poise = 30.0f;
        float poiseRegenDelay = 2.0f;   // seconds without being hit before poise refills
        int staggerFrames = 45;
        float knockbackScale = 1.0f;     // heavier enemies fly less far

        float walkSpeed = 3.0f;
        float turnSpeed = 360.0f;        // degrees per second
        float respawnSeconds = 3.0f;
        bool regenerates = false;        // a training dummy refills its health

        std::string script;              // script id, "base:brute"; empty = stands still
        Json params;                     // handed to the script as me.params

        Moveset moveset;
        std::string file;
    };

    /**
     * A terrain ability of the player. kind picks the behaviour (C++); the rest is its numbers.
     *   wall: raises width x height blocks of `block`, `distance` m ahead, across the view
     *   pit:  digs width x width x depth where the player aims, within `range`
     *   mine: chips `damage` off the aimed block, `rate` times a second while held
     * lifetime: seconds until a raised wall crumbles or a pit fills back in (0 = for good).
     */
    struct AbilityDef
    {
        std::string id;
        std::string name;
        std::string kind;          // wall, pit, mine; weapon skills: attack
        std::string block;
        float staminaCost = 0.0f;
        float cooldown = 0.5f;
        float range = 6.0f;
        float distance = 2.0f;
        float lifetime = 10.0f;
        float damage = 40.0f;
        float rate = 4.0f;
        int width = 3;
        int height = 2;
        int depth = 2;

        // Weapon skills: usable only with a weapon of one of these types (empty: with any). kind
        // "attack" runs `attack`, an attack like a moveset's, named "@<ability id>" while it runs
        std::vector<std::string> weapons;
        AttackDef attack;

        std::string file;
    };

    struct ArenaSpawn
    {
        std::string enemy;
        glm::vec3 position = glm::vec3(0.0f);
        float yaw = 0.0f;
    };

    struct ArenaDef
    {
        glm::ivec3 size = glm::ivec3(64, 32, 64);
        Json shapes;                 // list of { block, from, to } applied in order
        glm::vec3 playerSpawn = glm::vec3(32, 2, 32);
        std::vector<ArenaSpawn> spawns;
        std::string file;
        std::string mod;             // block names without a namespace are this mod's
    };

    /** A voxel model from the VoxelTest editor (.vxl.json). */
    struct VoxelModel
    {
        std::string id;
        std::string file;
        VoxelGrid grid;
        BlockTable table;
        /** Point under the model's feet, in voxels: the bottom centre of its occupied cells. */
        glm::vec3 pivot = glm::vec3(0.0f);

        /**
         * Body parts, one per visible layer of the editor file (a single layer = one rigid part).
         * Layers named leg_l, leg_r, leg_fl, leg_fr, leg_bl, leg_br, arm_l, arm_r and head are
         * animated; any other name stays still. joint: where the part turns, in voxels (the top
         * centre of a limb, the bottom centre of a head).
         */
        struct Part
        {
            std::string name;
            VoxelGrid grid;
            glm::vec3 joint = glm::vec3(0.0f);
        };
        std::vector<Part> parts;
    };

    /** Reads a .vxl.json file. Palette index i becomes block i + 1. */
    bool loadVoxelModel(const std::string& path, VoxelModel& out, ContentReport& report);

    // --------------------------------------------------------------------------------------------
    // The loaded content
    // --------------------------------------------------------------------------------------------

    struct Content
    {
        std::vector<ModInfo> mods;   // in load order

        // Raw, patched definitions per kind ("block", "weapon", "enemy"), by full id
        std::map<std::string, std::map<std::string, RawDef>> raw;

        std::vector<BlockDef> blocks;              // index == runtime id; 0 is base:air
        std::map<std::string, BlockId> blockIds;
        BlockTable blockTable;

        std::map<std::string, WeaponDef> weapons;
        std::vector<std::string> weaponOrder;      // load order, for the weapon hot keys

        std::map<std::string, EnemyDef> enemies;

        std::map<std::string, AbilityDef> abilities;

        std::map<std::string, StatusDef> statuses;   // statuses.json

        std::map<Element, ElementDef> elements;      // elements.json
        std::map<std::string, KeywordDef> keywords;  // keywords.json
        std::map<std::string, SpellDef> spells;      // spells/: ready-made

        std::map<std::string, std::shared_ptr<VoxelModel>> models;

        /** Script id -> path of its .pg file */
        std::map<std::string, std::string> scripts;

        Json tuning;          // the merged tuning tree

        /** effects.json of every mod merged: per game event, a sound and particles */
        Json effects;
        /** sounds.json of every mod merged: per sound, a recipe to synthesise it or a .wav (paths made absolute) */
        Json sounds;
        std::string tuningFile;

        ArenaDef arena;

        ContentReport report;

        /**
         * A hash of the gameplay content (definitions after patches, tuning, arena, scripts), with
         * what only looks left out: models, effects, sounds, colours, swing poses, camera shake,
         * names, the camera and the controls. Two machines with the same fingerprint run the same
         * game: a client whose fingerprint differs from the server's is refused, while a client with
         * its own looks (a texture pack, other sounds) can join.
         */
        uint64_t fingerprint = 0;

        BlockId block(const std::string& id) const;

        /** An attack of an enemy type or a weapon (source: "base:archer", "base:sword"); null if none. */
        const AttackDef* attackOf(const std::string& source, const std::string& attack) const;

        /** Path of an asset of a mod: ("base:sword", "models", ".vxl.json") -> mods/base/models/sword.vxl.json */
        std::string assetPath(const std::string& id, const std::string& folder, const std::string& extension) const;

        const ModInfo* mod(const std::string& id) const;
    };

    /**
     * Loads every mod found in modsDir: reads mod.json files, orders mods by their
     * dependencies, reads each mod's definitions, applies patches, then types and checks
     * everything. Problems end up in content.report; loading itself never fails.
     */
    Content loadContent(const std::string& modsDir);

    /** Parses an attack (or a whole moveset) from JSON. Exposed for tests. */
    bool readAttack(const Json& json, const std::string& name, const std::string& file, AttackDef& out, ContentReport& report);
    bool readMoveset(const Json& json, const std::string& file, Moveset& out, ContentReport& report);
}
