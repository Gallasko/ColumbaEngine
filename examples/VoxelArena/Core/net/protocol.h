#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "bytes.h"
#include "Core/content.h"
#include "Core/world.h"

namespace arena::net
{
    /** Bumped when messages change shape: a client and server of different versions refuse each other. */
    constexpr uint16_t PROTOCOL_VERSION = 30;

    /** The server sends a snapshot every this many steps (60 / 2 = 30 per second). */
    constexpr int SNAPSHOT_EVERY = 2;

    enum class Msg : uint8_t
    {
        Hello = 1,       // client -> server, reliable: version, content fingerprint, name
        Welcome,         // server -> client, reliable: accepted or refused, ids, the server tick
        WorldInit,       // server -> client, reliable: the whole arena (blocks, damage, falling pieces)
        Input,           // client -> server, unreliable: the last few inputs
        Snapshot,        // server -> client, unreliable: everyone's state, recent events
        TerrainDelta,    // server -> client, reliable: blocks and damage that changed
        PieceSpawn,      // server -> client, reliable: a piece of the arena broke off (its cells)
        Bye,             // either way, reliable
        Spells,          // client -> server, reliable: the recipes on its spell keys
        Lobby,           // server -> client, reliable: who is in the game, ready or not, their ping
        Ready,           // client -> server, reliable: ready (or not) to start the run
        ModBundle,       // server -> client, reliable: the server's mods, for a client whose content differs
        Name,            // client -> server, reliable: the name the others see, changed
    };

    /**
     * Strings the game names things with (fighter types, weapons, attacks, event kinds), as small
     * numbers. Built the same way from the same gameplay content on both sides: the content
     * fingerprint check makes sure the tables match. Nothing visual is in it (models may differ).
     */
    class Names
    {
    public:
        void build(const Content& content);

        uint16_t id(const std::string& s) const;
        const std::string& name(uint16_t id) const;

    private:
        std::vector<std::string> names;
        std::map<std::string, uint16_t> ids;
    };

    struct Hello
    {
        uint16_t version = PROTOCOL_VERSION;
        uint64_t fingerprint = 0;
        std::string name;
    };

    struct Welcome
    {
        bool accepted = false;
        std::string reason;
        int peer = 0;
        int playerId = 0;
        uint32_t tick = 0;
    };

    /** One file of a mod, by its path inside the mods folder. */
    struct ModFile
    {
        std::string path;
        std::string data;
    };

    /** The server's mods, and the fingerprint they have to give once loaded. */
    struct ModBundle
    {
        uint64_t fingerprint = 0;
        std::vector<ModFile> files;
    };

    /** One step of input with its sequence number and network stamps. */
    struct StampedInput
    {
        uint32_t seq = 0;
        InputFrame input;
    };

    struct InputMsg
    {
        uint32_t clientTimeMs = 0;   // echoed by the server, for the round trip
        uint16_t rttMs = 0;          // the client's measure, for the dodge grace
        uint32_t snapshotAck = 0;    // the newest snapshot the client has read: the next ones come as changes to it (0: none)
        std::vector<StampedInput> inputs;   // newest last
    };

    struct ProjectileState
    {
        int id = 0;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 velocity = glm::vec3(0.0f);
        std::string source;   // the enemy type or weapon that shot it, and its attack: each client
        std::string attack;   // finds the model in its own content
    };

    struct ZoneState
    {
        int id = 0;
        Element element = Element::None;
        glm::vec3 position = glm::vec3(0.0f);
        float radius = 0.0f;
        float remaining = 0.0f;
        float duration = 0.0f;
        float heal = 0.0f;   // a healing zone (drawn as one)
        bool blast = false;  // a blast's warning ring (drawn as a danger zone)
        bool enemy = false;  // the enemies' (it hurts the players: drawn edged in red)
        int owner = 0;       // a blast's: what it came out of (its countdown shows on it)
    };

    struct SpellsMsg
    {
        std::vector<SpellRecipe> spells;
    };

    struct PieceState
    {
        int id = 0;
        float drop = 0.0f;
    };

    struct SnapshotMsg
    {
        uint32_t seq = 0;              // the server's count of snapshots: what a client acks, and later ones build on
        uint32_t baseSeq = 0;          // decoded: the snapshot this one came as changes to (0: whole)
        uint32_t tick = 0;
        uint32_t ackSeq = 0;           // the last input of this client the server played
        uint32_t echoTimeMs = 0;       // the client time of that input
        int playerId = 0;              // this client's fighter
        std::vector<Fighter> fighters; // the client's own in full, others in what is needed to draw them
        std::vector<ProjectileState> projectiles;
        std::vector<PieceState> pieces;
        std::vector<ZoneState> zones;
        std::vector<WorldEvent> events;
        RunState run;                  // the arena run, as it stands (without what only the server needs)
        std::vector<Pickup> pickups;   // the shards lying about or flying to a player

        // Every player's items and run stats come now and then (they change little); the client's own
        // always. A client keeps the last ones it got of each player
        bool extras = false;
        std::vector<int> withExtras;   // decoded: the fighters whose items and stats came in this one
    };

    /**
     * A snapshot's fighters, projectiles, shards and zones as they went on the wire: one record each,
     * by id. A snapshot is sent as the changes since one the client said it read (records gone, new,
     * and the bytes that changed in the others); both sides keep the last few to build on.
     */
    struct SnapshotRecords
    {
        struct Section
        {
            std::vector<uint16_t> ids;        // in order
            std::vector<uint32_t> offsets;    // where each record starts in bytes, and one past the last
            std::vector<uint8_t> bytes;

            size_t size() const { return ids.size(); }
            const uint8_t* record(size_t i) const { return bytes.data() + offsets[i]; }
            size_t length(size_t i) const { return offsets[i + 1] - offsets[i]; }
            void add(uint16_t id, const uint8_t* data, size_t n);
        };

        static constexpr int FIGHTERS = 0, PROJECTILES = 1, PICKUPS = 2, ZONES = 3, SECTIONS = 4;

        uint32_t seq = 0;
        Section sections[SECTIONS];
    };

    /** The snapshots each side keeps to build later ones on: two seconds of them, more than any round trip. */
    constexpr size_t SNAPSHOTS_KEPT = 64;

    /** The most zones a snapshot carries (each costs nothing once sent: they do not change). */
    constexpr size_t MAX_SNAPSHOT_ZONES = 1024;

    struct TerrainDeltaMsg
    {
        std::vector<std::pair<uint32_t, BlockId>> blocks;
        std::vector<std::pair<uint32_t, float>> damage;   // ratio 0..1 (0: repaired or gone)
    };

    struct WorldInitMsg
    {
        glm::ivec3 size = glm::ivec3(0);
        std::vector<BlockId> cells;
        std::vector<std::pair<uint32_t, float>> damage;
    };

    struct PieceSpawnMsg
    {
        int id = 0;
        glm::ivec3 origin = glm::ivec3(0);
        float drop = 0.0f;
        VoxelGrid cells;
    };

    /** One player of the game, as the lobby shows them. */
    struct LobbyPlayer
    {
        int playerId = 0;
        std::string name;
        bool host = false;       // the listen server's own player
        bool ready = false;
        uint16_t pingMs = 0;
    };

    struct LobbyMsg
    {
        bool dedicated = false;  // no host: the run starts when everyone is ready
        std::vector<LobbyPlayer> players;
    };

    struct ReadyMsg
    {
        bool ready = false;
    };

    struct NameMsg
    {
        std::string name;
    };

    /** The longest player name, in bytes. */
    constexpr size_t MAX_NAME_BYTES = 32;

    /**
     * A name another machine sent, made safe to show and log: valid UTF-8 only, without control or
     * text-direction characters, trimmed, at most maxBytes (cut between characters). May come back empty.
     */
    std::string cleanName(const std::string& name, size_t maxBytes = MAX_NAME_BYTES);

    Msg peek(const std::vector<uint8_t>& data);

    std::vector<uint8_t> encode(const Hello& m);
    std::vector<uint8_t> encode(const Welcome& m);
    std::vector<uint8_t> encode(const ModBundle& m);
    std::vector<uint8_t> encode(const InputMsg& m);
    /** A snapshot sent whole. */
    std::vector<uint8_t> encode(const SnapshotMsg& m, const Names& names);
    /** A snapshot as the changes since `base` (null: whole). Its records go to `sent`, for later ones to build on. */
    std::vector<uint8_t> encode(const SnapshotMsg& m, const Names& names, const SnapshotRecords* base, SnapshotRecords* sent);
    std::vector<uint8_t> encode(const TerrainDeltaMsg& m);
    std::vector<uint8_t> encode(const WorldInitMsg& m);
    std::vector<uint8_t> encode(const PieceSpawnMsg& m);
    std::vector<uint8_t> encodeBye();
    std::vector<uint8_t> encode(const SpellsMsg& m, const Names& names);
    bool decode(const std::vector<uint8_t>& data, SpellsMsg& m, const Names& names);
    std::vector<uint8_t> encode(const LobbyMsg& m);
    bool decode(const std::vector<uint8_t>& data, LobbyMsg& m);
    std::vector<uint8_t> encode(const ReadyMsg& m);
    bool decode(const std::vector<uint8_t>& data, ReadyMsg& m);
    std::vector<uint8_t> encode(const NameMsg& m);
    bool decode(const std::vector<uint8_t>& data, NameMsg& m);

    // Decoders return false for a message that does not read cleanly (it is then ignored)
    bool decode(const std::vector<uint8_t>& data, Hello& m);
    bool decode(const std::vector<uint8_t>& data, Welcome& m);
    bool decode(const std::vector<uint8_t>& data, ModBundle& m);
    bool decode(const std::vector<uint8_t>& data, InputMsg& m);
    /** A whole snapshot (one sent as changes cannot be read without what it builds on). */
    bool decode(const std::vector<uint8_t>& data, SnapshotMsg& m, const Names& names);
    /**
     * A snapshot, whole or as changes to the one `baseOf` gives for its base (null: not kept, and the
     * snapshot cannot be read). Its records go to `got`, for later ones to build on.
     */
    bool decode(const std::vector<uint8_t>& data, SnapshotMsg& m, const Names& names,
                const std::function<const SnapshotRecords*(uint32_t seq)>& baseOf, SnapshotRecords* got);
    bool decode(const std::vector<uint8_t>& data, TerrainDeltaMsg& m);
    bool decode(const std::vector<uint8_t>& data, WorldInitMsg& m);
    bool decode(const std::vector<uint8_t>& data, PieceSpawnMsg& m);

    /**
     * A fighter in a message: full (all the client's prediction reads: its own player, exactly) or for
     * display only (where it is, to a centimetre). extras: a player's items and run stats (always with full).
     */
    void writeFighter(Writer& w, const Fighter& f, bool full, const Names& names, bool extras = false);
    /** Returns whether the player's items and run stats came with it. */
    bool readFighter(Reader& r, Fighter& f, const Names& names);
}
