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

        float turnRate = 0.0f;    // degrees per frame the attacker may still turn during startup

        float armor = 0.0f;       // extra poise while the attack runs (hyper armor)

        // Cancels, as frame numbers counted from the start of the attack
        int dodgeCancelFrom = -1;  // -1: only once the attack has ended
        int chainFrom = -1;        // earliest frame the next attack in the combo may start (-1: end of active)
        int bufferFrom = 0;        // earliest frame a press is remembered for the chain

        std::string nextLight;     // attack started by a light press during the chain window
        std::string nextHeavy;

        SwingPose swing;

        int total() const { return startup + active + recovery; }
        bool isActive(int frame) const { return frame >= startup and frame < startup + active; }
    };

    struct Moveset
    {
        std::map<std::string, AttackDef> attacks;

        std::string light;        // first light attack
        std::string heavy;
        std::string dodgeAttack;  // light pressed at the end of a dodge

        const AttackDef* find(const std::string& name) const;
    };

    struct WeaponDef
    {
        std::string id;
        std::string name;
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

        std::map<std::string, std::shared_ptr<VoxelModel>> models;

        /** Script id -> path of its .pg file */
        std::map<std::string, std::string> scripts;

        Json tuning;          // the merged tuning tree
        std::string tuningFile;

        ArenaDef arena;

        ContentReport report;

        BlockId block(const std::string& id) const;

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
