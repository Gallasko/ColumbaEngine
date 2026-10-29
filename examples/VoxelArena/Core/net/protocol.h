#pragma once

#include <map>
#include <string>
#include <vector>

#include "bytes.h"
#include "Core/content.h"
#include "Core/world.h"

namespace arena::net
{
    /** Bumped when messages change shape: a client and server of different versions refuse each other. */
    constexpr uint16_t PROTOCOL_VERSION = 10;

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
    };

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

    Msg peek(const std::vector<uint8_t>& data);

    std::vector<uint8_t> encode(const Hello& m);
    std::vector<uint8_t> encode(const Welcome& m);
    std::vector<uint8_t> encode(const InputMsg& m);
    std::vector<uint8_t> encode(const SnapshotMsg& m, const Names& names);
    std::vector<uint8_t> encode(const TerrainDeltaMsg& m);
    std::vector<uint8_t> encode(const WorldInitMsg& m);
    std::vector<uint8_t> encode(const PieceSpawnMsg& m);
    std::vector<uint8_t> encodeBye();
    std::vector<uint8_t> encode(const SpellsMsg& m, const Names& names);
    bool decode(const std::vector<uint8_t>& data, SpellsMsg& m, const Names& names);

    // Decoders return false for a message that does not read cleanly (it is then ignored)
    bool decode(const std::vector<uint8_t>& data, Hello& m);
    bool decode(const std::vector<uint8_t>& data, Welcome& m);
    bool decode(const std::vector<uint8_t>& data, InputMsg& m);
    bool decode(const std::vector<uint8_t>& data, SnapshotMsg& m, const Names& names);
    bool decode(const std::vector<uint8_t>& data, TerrainDeltaMsg& m);
    bool decode(const std::vector<uint8_t>& data, WorldInitMsg& m);
    bool decode(const std::vector<uint8_t>& data, PieceSpawnMsg& m);

    /** A fighter in a message: full (everything prediction needs) or for display only. */
    void writeFighter(Writer& w, const Fighter& f, bool full, const Names& names);
    void readFighter(Reader& r, Fighter& f, const Names& names);
}
