#pragma once

#include <algorithm>
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
        std::string model;                       // the voxel model of a "model" shaped block ("base:mushroom")
        std::string textures[3];                 // texture ids by BlockFace ("base:grass_top"); "" = none
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
        float rollFrom = 0.0f, rollTo = 0.0f;     // turned about its own length (the handle), + = its +X side up
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
        int bounces = 0;                // bolt: skips off the floor this many times before its impact
        float bounciness = 0.7f;        // bolt: the share of its speed each skip leaves it

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
    /**
     * What a run did to one spell or technique, on top of what it is: its tag items (upgrades.json),
     * its levels past the written ones, its branch. damage 0.2 is 20% more, area 0.2 a 20% wider reach,
     * cooldown 0.2 recharges 20% faster, cost 0.2 is 20% cheaper (mana, stamina), duration 0.2 lasts 20%
     * longer (zones), speed 0.2 is a 20% faster projectile, count 1 is one more projectile.
     */
    struct SpellTweak
    {
        float damage = 0.0f, area = 0.0f, cooldown = 0.0f, cost = 0.0f, duration = 0.0f, speed = 0.0f;
        float attackSpeed = 0.0f;   // an attack's frames fewer (a weapon's or a technique's; spells have none)
        int count = 0;

        bool empty() const
        {
            return damage == 0.0f and area == 0.0f and cooldown == 0.0f and cost == 0.0f and duration == 0.0f and speed == 0.0f and attackSpeed == 0.0f and count == 0;
        }
        bool operator==(const SpellTweak& o) const
        {
            return damage == o.damage and area == o.area and cooldown == o.cooldown and cost == o.cost and duration == o.duration and speed == o.speed and
                   attackSpeed == o.attackSpeed and count == o.count;
        }

        /** Adds another, its numbers (not its count) times k. */
        void add(const SpellTweak& o, float k = 1.0f)
        {
            damage += o.damage * k;
            area += o.area * k;
            cooldown += o.cooldown * k;
            cost += o.cost * k;
            duration += o.duration * k;
            speed += o.speed * k;
            attackSpeed += o.attackSpeed * k;
            count += o.count;
        }
    };

    struct SpellRecipe
    {
        std::string element;
        std::vector<std::string> keywords;
        SpellTweak tweak;

        bool operator==(const SpellRecipe& o) const { return element == o.element and keywords == o.keywords and tweak == o.tweak; }
    };

    /**
     * A ready-made spell (spells/<name>.json): a name and a recipe. In a run, spells are given by
     * level ups, not made: each level after the first adds its `levels` entry's keywords to the recipe.
     */
    /**
     * One of the ways a spell or a technique can grow at its branch level (run.json "branchLevel"): the
     * player picks one. A spell's adds keywords to its recipe; a technique's gives its hits an element
     * and a status. Either may add tags (what tag items reach) and a boost.
     */
    struct BranchDef
    {
        std::string name, text;
        std::vector<std::string> keywords;   // a spell's: added to its recipe
        std::vector<std::string> tags;
        SpellTweak boost;
        Element element = Element::None;     // a technique's hits
        StatusHit status;
    };

    struct SpellDef
    {
        std::string id, name, text;
        SpellRecipe recipe;
        std::vector<std::vector<std::string>> levels;   // what levels 2, 3... add
        std::vector<BranchDef> branches;

        int maxLevel() const { return 1 + static_cast<int>(levels.size()); }
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
        int count = 1;                // shot at once, fanned out
        float spread = 0.0f;          // degrees between two of them
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
            float speed = 1.0f;       // a projectile's speed at full charge, times its own
            bool hold = false;        // full, it waits for the key to let go (a bow's draw) instead of going by itself
        } charge;

        /** Healing (a healer's): on the first active frame, the players this close (the attacker too) get this back. */
        struct Heal
        {
            float amount = 0.0f;
            float radius = 0.0f;
        } heal;

        /** A healing zone on the ground where the attacker stands, from the first active frame: health a second to the players in it. */
        struct HealZone
        {
            float radius = 0.0f;
            float duration = 0.0f;
            float perSecond = 0.0f;
        } healZone;

        float leech = 1.0f;       // the share of life on hit it gives (set as it hits, not read: 0.2 for an area's, a zone's, a chain's)
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
        std::string role;        // "melee", "ranged", "support": the weapon choice shows it
        float regen = 0.0f;      // health a second while it is in hand (a melee weapon's mending)
        float mana = 0.0f;       // mana back for each point of damage its light and heavy attacks do (tuning mana.fromWeapon if not set)
        bool hasMana = false;
        // In a run, level ups grow it like a technique: what its levels 2, 3... add, and the branches to pick from
        std::vector<SpellTweak> levels;
        std::vector<BranchDef> branches;
        std::string text;        // a line on how it plays, for the weapon choice
        std::string lightText, heavyText;   // what its light and heavy attacks do, a line each
        std::string model;       // model id, "base:sword"
        float modelScale = 0.0625f;
        glm::vec3 grip = glm::vec3(0.0f);    // model-space point held in the hand (in model voxels)
        SwingPose rest;                       // pose when no attack runs
        bool followSwing = false;             // the head (the model's +X side) turns to lead each swing: a scythe's blade
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
        bool anchored = false;           // nothing pushes it about (a boss's crystal)

        // The arena run: damage taken by element (volt 1.5: weak to volt), points for the kill, a boss
        float weakness[4] = {1, 1, 1, 1};
        int score = 10;
        bool boss = false;

        int drops = 1;                   // shards dropped in a run

        /**
         * A boss's phases: as its health reaches each share of `at` (no hit takes it past one), it shields
         * itself and `crystals` crystals (the enemy `crystal`) rise `radius` m around the arena's middle.
         * Nothing hurts it until every one of them is broken; then it reels for `stagger` frames.
         */
        struct Phases
        {
            std::vector<float> at;            // health shares, highest first (0.66, 0.33)
            std::string crystal;
            int crystals = 3;
            float radius = 9.0f;
            int stagger = 150;

            bool any() const { return not at.empty() and not crystal.empty(); }
        } phases;

        std::string script;              // script id, "base:brute"; empty = stands still
        Json params;                     // handed to the script as me.params

        Moveset moveset;
        std::string file;
        bool followArm = false;               // in an attack, held where the arm points (a bow out at the shoulder), not at the hand point
        glm::vec3 view = glm::vec3(0.0f);     // first person: where it is held from the eye, right, up, forward (tuning firstPerson.weapon if not set)
        bool hasView = false;
        float viewTilt = 0.0f;                // first person: leant this many degrees, the top to the left
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
        std::string text;          // what it does, in a line (the level up's card)
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

        // A technique in a run: what tag items reach it ("melee", "area"...), what its levels 2, 3...
        // add, and the branches to pick from at the branch level
        std::vector<std::string> tags;
        std::vector<SpellTweak> levels;
        std::vector<BranchDef> branches;

        std::string file;
    };

    struct ArenaSpawn
    {
        std::string enemy;
        glm::vec3 position = glm::vec3(0.0f);
        float yaw = 0.0f;
    };

    /** Sows a decoration (tall grass, flowers) over the arena: on each "on" block with air above, by chance. */
    struct ArenaScatter
    {
        std::string block;           // full ids
        std::string on;
        float chance = 0.1f;         // 0..1, per block
    };

    struct ArenaDef
    {
        std::string id;              // "base:gorge" (arenas/gorge.json); "" for the training arena
        std::string name;            // shown as a run moves into it ("The Gorge")
        glm::ivec3 size = glm::ivec3(64, 32, 64);
        Json shapes;                 // list of { block, from, to } applied in order
        glm::vec3 playerSpawn = glm::vec3(32, 2, 32);
        glm::vec3 centre = glm::vec3(32, 4, 32);   // the run's waves: enemies come in round it, the duel's ring is round it
        float spawnRadius = 12.0f;   // m from the centre enemies come in at
        bool hasCentre = false, hasSpawnRadius = false;   // given (else made from the size)
        std::vector<ArenaSpawn> spawns;
        std::vector<ArenaScatter> scatter;   // decorations sown once the shapes are built
        std::string file;
        std::string mod;             // block names without a namespace are this mod's
    };

    /**
     * A boon (upgrades.json), picked between the waves of a run. Its numbers add up with each pick:
     * damage 0.15 is 15% more, manaCost 0.15 is 15% cheaper, cooldown 0.2 is 20% shorter.
     */
    /**
     * How an item shows on a player who has it (upgrades.json "look"): pieces worn on the body, or a gem
     * circling it. A piece is a model whose bottom centre goes at `at` (in the player model's voxels) on a
     * body part (head, body, arm_l, arm_r, leg_l, leg_r: it moves with it), turned by yaw degrees, its
     * voxels `scale` m (the player's are 0.125).
     */
    struct LookDef
    {
        struct Piece
        {
            std::string model;
            std::string part = "body";
            glm::vec3 at = glm::vec3(0.0f);
            float yaw = 0.0f;
            float scale = 0.0625f;
        };
        std::vector<Piece> pieces;

        bool orbit = false;           // a gem circling the player, in this colour (0-1)
        glm::vec3 colour = glm::vec3(1.0f);

        bool empty() const { return pieces.empty() and not orbit; }
    };

    struct UpgradeDef
    {
        std::string id, name, text;
        LookDef look;
        float maxHealth = 0.0f;
        float damage = 0.0f;
        int dashCharges = 0;
        float manaCost = 0.0f;
        float manaRegen = 0.0f;
        float staminaRegen = 0.0f;
        float speed = 0.0f;
        float cooldown = 0.0f;
        float lifeOnHit = 0.0f;
        int statusStacks = 0;
        float armor = 0.0f;           // damage taken: armor / (armor + 20) of it is stopped
        float crit = 0.0f;            // chance of a critical hit (1.75 times the damage)
        float pickupRange = 0.0f;     // m more that shards fly to you from
        float regen = 0.0f;           // health a second
        float attackSpeed = 0.0f;     // the weapon's light and heavy attacks this much quicker (0.1: 10%)
        float horde = 0.0f;           // this much more enemies come (0.15: 15%), for everyone in the run
        float xpGain = 0.0f;          // this much more XP from each shard picked up (0.15: 15%)
        float shardChance = 0.0f;     // the chance each shard of a kill comes with one more, for everyone in the run

        // A tag item: its boost goes to every spell and technique of the player with the tag ("fire",
        // "projectile", "area", "spell", "technique"...)
        std::string tag;
        SpellTweak boost;
        bool forTag() const { return not tag.empty(); }

        int most = 0;                 // bought at most this many times in a run (0: no limit; it stops when it would do nothing more)
        std::vector<std::string> tiers;   // the rarities (run.json shop.rarities, by name) it comes at; none: any
        /** Whether it may come at a rarity of the run. */
        bool comesAt(const std::string& rarity) const { return tiers.empty() or std::find(tiers.begin(), tiers.end(), rarity) != tiers.end(); }
        int price = 10;               // in shards, at the shop (it goes up with the waves)
    };

    /** How rare a shop item is: its numbers and its price times these, and how often it comes up. */
    struct RarityDef
    {
        std::string name;
        float power = 1.0f;           // the item's numbers (not its whole ones: dash charges, stacks) times this
        float price = 1.0f;
        glm::vec2 weight = glm::vec2(1.0f);   // how often, at the first wave and at the last
    };

    /**
     * A difficulty level of the run (run.json "difficulties"): the first is open, each next one opens
     * once the one before it is won. On top of the run as written.
     */
    struct DifficultyDef
    {
        std::string name = "Normal";
        int waves = 0;                // the run's first this many waves (0: all of them)
        float health = 1.0f;          // enemies' health times this
        float damage = 1.0f;          // the damage enemies do times this
        float healthGrowth = 0.0f;    // enemies' health grows this much more each wave (on top of the run's)
        float damageGrowth = 0.0f;    // their damage grows this much each wave
        float spawn = 1.0f;           // the spawn rate and the most at once times this
        float eliteChance = 0.0f;     // added to the run's
    };

    /** Who comes in during the waves of a run, from which wave on, how often (weight). */
    struct SpawnPool
    {
        std::string enemy;
        int from = 1, to = 1000;
        float weight = 1.0f;
    };

    /** The arena run (run.json): timed waves, the enemies, the bosses, the shop between waves. */
    struct RunDef
    {
        /** How often a rarity comes up in the shop after a wave: its weight, from the first wave's to the last's. */
        float rarityWeight(int rarity, int wave) const;
        /** The same as a chance, 0..1, out of all the rarities. */
        float rarityChance(int rarity, int wave) const;

        /** Difficulty levels, the easiest first. None: one level, the run as written. */
        std::vector<DifficultyDef> difficulties;
        int levels() const { return std::max(1, static_cast<int>(difficulties.size())); }
        /** A level (clamped to the ones there are). */
        const DifficultyDef& difficulty(int level) const;
        /** The waves a run at a level lasts. */
        int wavesAt(int level) const;

        int waves = 20;
        glm::vec2 duration = glm::vec2(20, 60);   // s a wave lasts, the first to the last
        glm::vec2 spawnRate = glm::vec2(0.6f, 2.4f);   // enemies a second, the first wave to the last
        glm::ivec2 maxAlive = glm::ivec2(6, 30);
        float eliteFrom = 8;          // wave elites start to come in from
        float eliteChance = 0.1f;
        float healthGrowth = 0.05f;   // enemies' health: this much more each wave
        float spawnScale = 1.0f;      // the spawn rate and the most at once, times this (for one player)
        std::vector<SpawnPool> pools;
        std::map<int, std::string> bosses;   // wave -> boss: the wave lasts until it falls
        float warn = 1.0f;            // s a spawn point shows before its enemy comes out
        glm::vec3 centre = glm::vec3(32, 4, 38);
        float spawnRadius = 12.0f;
        float coopScale = 0.5f;       // more enemies per player after the first (0.5: half again each)
        int waveScore = 50;           // per wave survived, times its number
        int victoryScore = 2000;

        // The economy: shards dropped (each enemy's "drops"), picked up in range, money and XP
        float pickupRange = 2.5f;
        float shardFalloff = 0.0f;    // the players' extra shard chances (greed) add up to t, counted as t / (1 + shardFalloff x t)
        /** Extra damage from items and mastery: the first `from` counts whole, the rest e as e / (1 + rate x e). */
        struct Falloff
        {
            float from = 0.4f;
            float rate = 0.6f;
        } damageFalloff;
        float bossCoopHealth = 0.75f; // a boss's health up this much per player past the first
        int xpBase = 10, xpStep = 6;  // shards to the next level: base + step x (level - 1)
        std::string startWeapon = "base:sword";
        std::vector<std::string> startOwned = {"element:fire", "base:bolt"};
        std::vector<std::string> startSpells;   // the spells a run starts with (spells/*.json ids), on keys 1, 2, 3
        std::vector<std::string> startChoices;  // the first spell each player picks among, with the weapon (then on key 1)

        // Level ups, picked as a wave ends: a new spell, a spell's next level, or one of these weapon skills
        std::vector<std::string> levelSkills;
        float intermission = 2.5f;    // s between a wave's end and its level ups and shop (no misclicks)
        int branchLevel = 5;          // the level a spell or technique with branches takes one of them
        SpellTweak mastery;           // what each level past the written ones (and the branch) adds

        // Presents: an elite may drop one (presentChance), at most presentsPerWave a wave; each is a free
        // pick of presentChoices shop items
        float presentChance = 0.35f;
        int presentsPerWave = 3;
        int presentChoices = 3;
        std::vector<RarityDef> rarities;   // the shop's, commonest first (none: everything is the first)

        // The shop between waves: offers, rerolls, prices
        int shopSlots = 4;
        int weaponPrice = 30;         // a weapon not in unlocks (another mod's) sells for this
        int rerollPrice = 2, rerollStep = 2;   // the first reroll of a shop, then this much more each
        glm::vec2 priceScale = glm::vec2(0.5f, 1.5f);   // prices times this, at the first wave and the last (in between, evenly)
        float coopPrice = 0.5f;       // prices up this much per player after the first: every player gets every shard
        bool interest = true;         // shards kept through a shop grow with the prices: what could be bought still can
        int levelChoices = 3;         // picks offered for a level
        std::map<std::string, int> unlocks;   // weapons, skills, keywords, elements ("element:ice") -> price
        bool resetEachWave = true;    // the arena is built anew before each wave (and the players put back)
        bool chooseWeapon = true;     // each player picks their weapon before the first wave (none are for sale then)

        // After the last wave, two players or more: a final duel, everyone for themselves
        struct Duel
        {
            bool enabled = true;
            float duration = 120.0f;  // s; then the one with the most health left (of their max) wins
            float countdown = 3.0f;   // s before anyone may hurt anyone
            float radius = 9.0f;      // m from the arena's centre the players start, in a ring
            std::string arena;        // the arena it is fought in ("": the last wave's)
        } duel;

        // The arenas the waves are fought in: each from its wave on, until the next one's
        struct ArenaPick
        {
            std::string arena;        // an arenas/*.json id ("base:gorge")
            int from = 1;
        };
        std::vector<ArenaPick> arenas;
        std::string file;
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

        std::map<std::string, UpgradeDef> upgrades;  // upgrades.json: the boons of a run
        RunDef run;                                  // run.json (the last mod with one)

        std::map<std::string, std::shared_ptr<VoxelModel>> models;

        /** Texture id ("base:grass_top") -> path of its textures/*.png */
        std::map<std::string, std::string> textureFiles;
        /** The png of each layer of the block textures (BlockLook::texture), only those a block uses. */
        std::vector<std::string> blockTextures;

        /** Script id -> path of its .pg file */
        std::map<std::string, std::string> scripts;

        Json tuning;          // the merged tuning tree

        /** effects.json of every mod merged: per game event, a sound and particles */
        Json effects;
        /** sounds.json of every mod merged: per sound, a recipe to synthesise it or a .wav (paths made absolute) */
        Json sounds;
        std::string tuningFile;

        ArenaDef arena;               // the training arena (arena.json)
        std::map<std::string, ArenaDef> arenas;   // arenas/*.json, and a run.json's own "arena" ("<mod>:run")

        /** The arena of a wave of the run (duel: the final duel's), by id ("": none, the training one). */
        std::string runArenaId(int wave, bool duel) const;

        /** An arena by id: the training arena for "" or one unknown. */
        const ArenaDef& arenaById(const std::string& id) const;

        ContentReport report;

        /**
         * A hash of the gameplay content (definitions after patches, tuning, arena, scripts), with
         * what only looks left out: models, effects, sounds, colours, swing poses, camera shake,
         * names, the camera and the controls. Two machines with the same fingerprint run the same
            float length = 0.0f;   // how far it reaches from its joint, in voxels (a limb's top to its bottom)
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
