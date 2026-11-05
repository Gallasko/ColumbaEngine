#include "protocol.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace arena::net
{
    namespace
    {
        // Every event kind the world makes: event names travel as numbers too
        const char* const EVENT_KINDS[] = {
            "hit", "stagger", "kill", "swing", "windup", "shoot", "arrow_hit", "arrow_hit_fighter", "dodge", "jump", "land",
            "hard_land", "step", "chip", "break", "raise", "dig", "piece_fall", "piece_land",
            "air_jump", "wall_jump", "air_dash", "plunge_land", "fire_cast", "ice_cast", "volt_cast",
            "fire_burst", "ice_burst", "volt_arc", "ignite", "freeze", "status", "leap",
            "volt_burst", "fire_nova", "ice_nova", "volt_nova", "fire_zone", "ice_zone", "volt_zone", "fire_beam", "ice_beam", "volt_beam",
            "run_start", "spawn_warn", "boss_warn", "spawn", "boss_spawn", "wave_clear", "run_victory", "run_defeat", "boon",
            "wave_start", "arena_reset", "arena_change", "despawn", "elite_warn", "pickup", "level_up", "buy", "crit", "present", "present_drop",
            "difficulty_pick", "boss_shield", "shield_break", "shield_hit", "detonate", "burst",
            "duel_start", "duel_fight", "duel_end", "heal", "heal_burst", "heal_zone", "weapon_pick",
            "blast_warn", "blast", "split", "quake", "thorns", "proc", "level_reroll", "banish", "enemy_heal",
        };

        // What a player's damage is counted under in the recap, beyond the content's ids (see ArenaWorld::sourceOf)
        const char* const SOURCES[] = { "blast", "zone", "thorns", "proc", "landing" };

        const char* const AI_MOVES[] = { "hold", "chase", "strafe", "retreat", "flank" };

        // What positions and facings go across in two bytes as: centimetres, and -180..180 degrees over the whole range
        constexpr float CENTIMETRES = 100.0f;
        constexpr float YAW_SCALE = 32767.0f / 180.0f;

        uint8_t colorByte(float c) { return static_cast<uint8_t>(std::clamp(c, 0.0f, 1.0f) * 255.0f); }

        // Run-length blocks: (id, run) pairs
        void writeCells(Writer& w, const std::vector<BlockId>& cells)
        {
            size_t i = 0;
            std::vector<std::pair<BlockId, uint16_t>> runs;

            while (i < cells.size())
            {
                const BlockId id = cells[i];
                uint16_t run = 0;

                while (i < cells.size() and cells[i] == id and run < 65535)
                {
                    ++i;
                    ++run;
                }

                runs.emplace_back(id, run);
            }

            w.u32(static_cast<uint32_t>(cells.size()));
            w.u32(static_cast<uint32_t>(runs.size()));

            for (const auto& [id, run] : runs)
            {
                w.u16(id);
                w.u16(run);
            }
        }

        bool readCells(Reader& r, std::vector<BlockId>& cells, size_t expected)
        {
            const uint32_t count = r.u32();
            const uint32_t runs = r.u32();

            if (count != expected or runs > count or r.left() / 4u < runs)
                return false;

            cells.clear();
            cells.reserve(count);

            for (uint32_t k = 0; k < runs and not r.failed; ++k)
            {
                const BlockId id = r.u16();
                const uint16_t run = r.u16();

                if (cells.size() + run > count)
                    return false;

                cells.insert(cells.end(), run, id);
            }

            return not r.failed and cells.size() == count;
        }

        void writeDamage(Writer& w, const std::vector<std::pair<uint32_t, float>>& damage)
        {
            w.u32(static_cast<uint32_t>(damage.size()));

            for (const auto& [index, ratio] : damage)
            {
                w.u32(index);
                w.u8(colorByte(ratio));
            }
        }

        void readDamage(Reader& r, std::vector<std::pair<uint32_t, float>>& damage)
        {
            const uint32_t n = r.u32();

            // Divided, not multiplied: a huge count must not wrap around to a small size
            if (r.left() / 5u < n)
            {
                r.failed = true;
                return;
            }

            for (uint32_t k = 0; k < n; ++k)
            {
                const uint32_t index = r.u32();
                damage.emplace_back(index, r.u8() / 255.0f);
            }
        }
    }

    void Names::build(const Content& content)
    {
        std::set<std::string> all = {"", "player"};

        for (const char* k : EVENT_KINDS)
            all.insert(k);
        for (const char* m : AI_MOVES)
            all.insert(m);
        for (const char* s : SOURCES)
            all.insert(s);
        for (const auto& [id, a] : content.affixes)
            all.insert(id);

        for (const auto& [id, e] : content.enemies)
        {
            all.insert(id);
            for (const auto& [name, a] : e.moveset.attacks)
                all.insert(name);
        }

        for (const auto& [id, w] : content.weapons)
        {
            all.insert(id);
            for (const auto& [name, a] : w.moveset.attacks)
                all.insert(name);
        }

        for (const auto& [id, s] : content.statuses)
            all.insert(id);
        for (const auto& [id, c] : content.combos)
            all.insert(id);   // a combo going off: its event

        // Spells: their elements and keywords
        for (const char* e : {"fire", "ice", "volt"})
        {
            all.insert(e);
            all.insert(std::string("element:") + e);   // a run's unlock of it
        }
        for (const auto& [id, k] : content.keywords)
            all.insert(id);
        for (const auto& [id, u] : content.upgrades)
            all.insert(id);
        for (const auto& [id, sp] : content.spells)
        {
            all.insert(id);   // a run's spells and its level ups
            for (size_t b = 0; b < sp.branches.size(); ++b)
                all.insert(id + "#" + std::to_string(b));   // a level up's branch
        }

        for (const auto& [id, a] : content.abilities)
        {
            all.insert(id);
            if (a.kind == "attack")
                all.insert("@" + id);   // a weapon skill's attack, while it runs
            for (size_t b = 0; b < a.branches.size(); ++b)
                all.insert(id + "#" + std::to_string(b));
        }

        names.assign(all.begin(), all.end());
        ids.clear();

        for (size_t i = 0; i < names.size(); ++i)
            ids[names[i]] = static_cast<uint16_t>(i);
    }

    uint16_t Names::id(const std::string& s) const
    {
        auto it = ids.find(s);
        return it == ids.end() ? 0 : it->second;   // 0 is "": an unknown name reads back empty
    }

    const std::string& Names::name(uint16_t id) const
    {
        static const std::string empty;
        return id < names.size() ? names[id] : empty;
    }

    std::string cleanName(const std::string& name, size_t maxBytes)
    {
        std::string out;
        size_t i = 0;

        while (i < name.size())
        {
            const auto byte = [&](size_t k) { return static_cast<uint8_t>(name[k]); };
            const uint8_t b = byte(i);
            size_t len = b < 0x80 ? 1 : (b >> 5) == 0x6 ? 2 : (b >> 4) == 0xE ? 3 : (b >> 3) == 0x1E ? 4 : 0;

            // A broken sequence: skip the byte
            bool valid = len > 0 and i + len <= name.size();
            uint32_t cp = len == 1 ? b : len == 2 ? (b & 0x1Fu) : len == 3 ? (b & 0x0Fu) : (b & 0x07u);

            for (size_t k = 1; valid and k < len; ++k)
            {
                valid = (byte(i + k) & 0xC0) == 0x80;
                cp = (cp << 6) | (byte(i + k) & 0x3Fu);
            }

            // Overlong forms, surrogates and beyond Unicode are not characters
            static const uint32_t least[] = {0, 0, 0x80, 0x800, 0x10000};
            valid = valid and cp >= least[len] and cp <= 0x10FFFF and not (cp >= 0xD800 and cp <= 0xDFFF);

            if (not valid)
            {
                ++i;
                continue;
            }

            // Controls, and what turns the text around (a name could make the next words read backwards)
            const bool control = cp < 0x20 or (cp >= 0x7F and cp < 0xA0);
            const bool direction = (cp >= 0x200E and cp <= 0x200F) or (cp >= 0x202A and cp <= 0x202E) or (cp >= 0x2066 and cp <= 0x2069);

            if (not control and not direction)
            {
                if (out.size() + len > maxBytes)
                    break;

                out.append(name, i, len);
            }

            i += len;
        }

        const size_t first = out.find_first_not_of(' ');
        if (first == std::string::npos)
            return "";

        return out.substr(first, out.find_last_not_of(' ') - first + 1);
    }

    Msg peek(const std::vector<uint8_t>& data)
    {
        return data.empty() ? Msg::Bye : static_cast<Msg>(data[0]);
    }

    // ---- Hello / Welcome / Bye -------------------------------------------------------------------

    std::vector<uint8_t> encode(const Hello& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Hello));
        w.u16(m.version);
        w.u64(m.fingerprint);
        w.str(m.name);
        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, Hello& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Hello))
            return false;
        m.version = r.u16();
        m.fingerprint = r.u64();
        m.name = r.str();
        return not r.failed;
    }

    std::vector<uint8_t> encode(const ModBundle& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::ModBundle));
        w.u64(m.fingerprint);
        w.u32(static_cast<uint32_t>(m.files.size()));

        for (const auto& f : m.files)
        {
            w.str(f.path);
            w.blob(f.data);
        }

        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, ModBundle& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::ModBundle))
            return false;
        m.fingerprint = r.u64();

        const uint32_t count = r.u32();

        // Each file takes 6 bytes at least: a count that cannot be true reserves nothing
        for (uint32_t i = 0; i < count and not r.failed; ++i)
        {
            ModFile f;
            f.path = r.str();
            f.data = r.blob();
            m.files.push_back(std::move(f));
        }

        return not r.failed;
    }

    std::vector<uint8_t> encode(const Welcome& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Welcome));
        w.boolean(m.accepted);
        w.str(m.reason);
        w.u16(static_cast<uint16_t>(m.peer));
        w.u16(static_cast<uint16_t>(m.playerId));
        w.u32(m.tick);
        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, Welcome& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Welcome))
            return false;
        m.accepted = r.boolean();
        m.reason = r.str();
        m.peer = r.u16();
        m.playerId = r.u16();
        m.tick = r.u32();
        return not r.failed;
    }

    std::vector<uint8_t> encodeBye()
    {
        return {static_cast<uint8_t>(Msg::Bye)};
    }

    // ---- Lobby ------------------------------------------------------------------------------------

    std::vector<uint8_t> encode(const LobbyMsg& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Lobby));
        w.boolean(m.dedicated);
        w.u8(static_cast<uint8_t>(std::min<size_t>(m.players.size(), 32)));

        for (size_t i = 0; i < m.players.size() and i < 32; ++i)
        {
            const LobbyPlayer& p = m.players[i];
            w.i32(p.playerId);
            w.str(p.name);
            w.boolean(p.host);
            w.boolean(p.ready);
            w.u16(p.pingMs);
        }

        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, LobbyMsg& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Lobby))
            return false;

        m = LobbyMsg{};
        m.dedicated = r.boolean();
        const int n = r.u8();

        for (int i = 0; i < n and not r.failed; ++i)
        {
            LobbyPlayer p;
            p.playerId = r.i32();
            p.name = cleanName(r.str());
            p.host = r.boolean();
            p.ready = r.boolean();
            p.pingMs = r.u16();
            m.players.push_back(p);
        }

        return not r.failed;
    }

    std::vector<uint8_t> encode(const ReadyMsg& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Ready));
        w.boolean(m.ready);
        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, ReadyMsg& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Ready))
            return false;
        m.ready = r.boolean();
        return not r.failed;
    }

    std::vector<uint8_t> encode(const NameMsg& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Name));
        w.str(cleanName(m.name));
        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, NameMsg& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Name))
            return false;
        m.name = r.str();
        return not r.failed;
    }

    // ---- Spells -----------------------------------------------------------------------------------

    std::vector<uint8_t> encode(const SpellsMsg& m, const Names& names)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Spells));
        w.u8(static_cast<uint8_t>(std::min<size_t>(m.spells.size(), 8)));

        for (size_t i = 0; i < m.spells.size() and i < 8; ++i)
        {
            const SpellRecipe& r = m.spells[i];
            w.u16(names.id(r.element));
            w.u8(static_cast<uint8_t>(std::min<size_t>(r.keywords.size(), 16)));
            for (size_t k = 0; k < r.keywords.size() and k < 16; ++k)
                w.u16(names.id(r.keywords[k]));
        }

        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, SpellsMsg& m, const Names& names)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Spells))
            return false;

        const uint8_t n = r.u8();
        for (uint8_t i = 0; i < n and i < 8 and not r.failed; ++i)
        {
            SpellRecipe recipe;
            recipe.element = names.name(r.u16());
            const uint8_t k = r.u8();
            for (uint8_t j = 0; j < k and j < 16 and not r.failed; ++j)
                recipe.keywords.push_back(names.name(r.u16()));
            m.spells.push_back(recipe);
        }

        return not r.failed;
    }

    // ---- Input ------------------------------------------------------------------------------------

    std::vector<uint8_t> encode(const InputMsg& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Input));
        w.u32(m.clientTimeMs);
        w.u16(m.rttMs);
        w.u32(m.snapshotAck);
        w.u8(static_cast<uint8_t>(m.inputs.size()));

        for (const auto& s : m.inputs)
        {
            const InputFrame& in = s.input;
            w.u32(s.seq);
            w.i32(in.stamp);
            w.i32(in.viewTick);
            w.i8(static_cast<int8_t>(glm::clamp(in.move.x, -1.0f, 1.0f) * 127.0f));
            w.i8(static_cast<int8_t>(glm::clamp(in.move.y, -1.0f, 1.0f) * 127.0f));

            uint32_t bits = 0;
            bits |= in.jumpPressed ? 1u << 0 : 0u;
            bits |= in.jumpHeld ? 1u << 1 : 0u;
            bits |= in.sprintHeld ? 1u << 2 : 0u;
            bits |= in.dodgePressed ? 1u << 3 : 0u;
            bits |= in.lightPressed ? 1u << 4 : 0u;
            bits |= in.heavyPressed ? 1u << 5 : 0u;
            bits |= in.faceCamera ? 1u << 6 : 0u;
            bits |= in.crouchHeld ? 1u << 7 : 0u;
            bits |= in.lightHeld ? 1u << 24 : 0u;
            bits |= in.heavyHeld ? 1u << 25 : 0u;
            for (int i = 0; i < InputFrame::ABILITIES; ++i)
            {
                bits |= in.abilityPressed[i] ? 1u << (8 + i) : 0u;
                bits |= in.abilityHeld[i] ? 1u << (16 + i) : 0u;
            }
            w.u32(bits);

            w.f32(in.cameraYaw);
            w.f32(in.cameraPitch);
            w.vec3(in.aimOrigin);
            w.dir(in.aimDir);
            w.u8(static_cast<uint8_t>(std::clamp(in.equip, 0, 255)));
            w.u8(static_cast<uint8_t>(std::clamp(in.pick, 0, 255)));
        }

        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, InputMsg& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Input))
            return false;

        m.clientTimeMs = r.u32();
        m.rttMs = r.u16();
        m.snapshotAck = r.u32();
        const uint8_t n = r.u8();

        if (n > 16)
            return false;

        for (uint8_t k = 0; k < n and not r.failed; ++k)
        {
            StampedInput s;
            InputFrame& in = s.input;
            s.seq = r.u32();
            in.stamp = r.i32();
            in.viewTick = r.i32();
            in.move.x = r.i8() / 127.0f;
            in.move.y = r.i8() / 127.0f;

            const uint32_t bits = r.u32();
            in.jumpPressed = bits & (1u << 0);
            in.jumpHeld = bits & (1u << 1);
            in.sprintHeld = bits & (1u << 2);
            in.dodgePressed = bits & (1u << 3);
            in.lightPressed = bits & (1u << 4);
            in.heavyPressed = bits & (1u << 5);
            in.faceCamera = bits & (1u << 6);
            in.crouchHeld = bits & (1u << 7);
            in.lightHeld = bits & (1u << 24);
            in.heavyHeld = bits & (1u << 25);
            for (int i = 0; i < InputFrame::ABILITIES; ++i)
            {
                in.abilityPressed[i] = bits & (1u << (8 + i));
                in.abilityHeld[i] = bits & (1u << (16 + i));
            }

            in.cameraYaw = r.f32();
            in.cameraPitch = r.f32();
            in.aimOrigin = r.vec3();
            in.aimDir = r.dir();
            in.equip = r.u8();
            in.pick = r.u8();

            if (glm::length(in.move) > 1.0f)
                in.move = glm::normalize(in.move);

            m.inputs.push_back(s);
        }

        return not r.failed;
    }

    // ---- Fighters ---------------------------------------------------------------------------------

    void writeFighter(Writer& w, const Fighter& f, bool full, const Names& names, bool extras)
    {
        w.boolean(full);
        w.u16(static_cast<uint16_t>(f.id));
        w.u8(static_cast<uint8_t>(f.team));
        w.u16(names.id(f.type));
        w.u16(static_cast<uint16_t>(f.owner));

        // What every fighter needs to be drawn: the client's own exactly (its prediction starts from it),
        // the others where they are to a centimetre (an arena is 256 blocks at most), facing to a hundredth of a degree
        if (full)
        {
            w.vec3(f.body.position);
            w.vec3(f.body.velocity);
        }
        else
        {
            for (int a = 0; a < 3; ++a)
                w.fixed(f.body.position[a], CENTIMETRES);
            for (int a = 0; a < 3; ++a)
                w.fixed(f.body.velocity[a], CENTIMETRES);
        }
        w.f32(f.body.width);
        w.f32(f.body.height);
        w.boolean(f.body.onGround);
        if (full)
            w.f32(f.yaw);
        else
            w.fixed(std::isfinite(f.yaw) ? f.yaw - 360.0f * std::floor((f.yaw + 180.0f) / 360.0f) : 0.0f, YAW_SCALE);
        w.f32(f.health);
        w.f32(f.maxHealth);
        w.f32(f.poise);
        w.f32(f.maxPoise);
        w.u8(static_cast<uint8_t>(f.action));
        w.i16(static_cast<int16_t>(f.frame));
        w.i16(static_cast<int16_t>(f.actionLength));
        w.u16(names.id(f.weapon));
        w.u16(names.id(f.attack));
        w.u8(static_cast<uint8_t>(std::clamp(f.hitstop, 0, 255)));
        w.f32(f.attackPitch);
        w.vec3(f.dodgeDir);
        w.u16(names.id(f.aiMove));

        w.u8(static_cast<uint8_t>(std::clamp(f.chargeFrames, 0, 255)));   // a charge shows
        w.u8((f.elite ? 0x01 : 0) | (f.climbTo >= 0.0f ? 0x02 : 0) | (f.blownUp ? 0x04 : 0));   // an elite; climbing (its pose); blown up
        w.u8(static_cast<uint8_t>(std::clamp(f.phase, 0, 127)) | (f.shielded ? 0x80 : 0));   // a boss's phase, its shield
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.affixes.size(), 8)));                  // an elite's affixes: they show
        for (size_t i = 0; i < f.affixes.size() and i < 8; ++i)
            w.u16(names.id(f.affixes[i]));

        // Statuses: they slow and they show
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.statuses.size(), 16)));
        for (size_t i = 0; i < f.statuses.size() and i < 16; ++i)
        {
            w.u16(names.id(f.statuses[i].id));
            w.f32(f.statuses[i].remaining);
            w.u8(static_cast<uint8_t>(std::clamp(f.statuses[i].stacks, 0, 255)));
        }

        // A player's items (everyone sees them on it) and what it did in the run (the recap at its end)
        const bool withExtras = f.team == Team::Player and (full or extras);
        if (f.team == Team::Player)
            w.boolean(withExtras);
        if (withExtras)
        {
            w.u8(static_cast<uint8_t>(std::min<size_t>(f.boons.size(), 64)));
            for (size_t i = 0; i < f.boons.size() and i < 64; ++i)
            {
                w.u16(names.id(f.boons[i].id));
                w.u8(static_cast<uint8_t>(std::clamp(f.boons[i].rarity, 0, 255)));
            }

            const auto& st = f.stats;
            w.u16(static_cast<uint16_t>(std::clamp(st.kills, 0, 65535)));
            w.u32(static_cast<uint32_t>(std::max(0, st.shards)));
            w.u32(static_cast<uint32_t>(std::max(0, st.xp)));
            w.u8(static_cast<uint8_t>(std::clamp(st.presents, 0, 255)));
            w.u8(static_cast<uint8_t>(std::clamp(st.items, 0, 255)));
            w.u8(static_cast<uint8_t>(std::clamp(st.downs, 0, 255)));
            w.u32(static_cast<uint32_t>(std::lround(std::clamp(st.damage, 0.0f, 4e9f))));
            w.u32(static_cast<uint32_t>(std::lround(std::clamp(st.healing, 0.0f, 4e9f))));
            w.u8(static_cast<uint8_t>(std::clamp(f.level, 0, 255)));
        }

        if (not full)
            return;

        // Everything the client's prediction of its own player reads
        w.f32(f.sinceHit);
        w.f32(f.stamina);
        w.f32(f.staminaDelay);
        w.boolean(f.exhausted);
        for (float c : f.abilityCooldown)
            w.f32(c);
        w.f32(f.mineTimer);
        w.u8(static_cast<uint8_t>(f.queued));
        w.f32(f.queuedAge);
        w.f32(f.dodgeQueuedAge);
        w.vec3(f.push);
        w.f32(f.coyote);
        w.f32(f.jumpBuffer);
        w.boolean(f.jumping);
        w.f32(f.airSpeed);
        w.f32(f.landGrace);
        w.f32(f.landSpeed);
        w.boolean(f.airborneStagger);
        w.f32(f.airDodgeAge);
        w.boolean(f.heavyStagger);
        w.f32(f.respawnTimer);
        w.vec3(f.spawnPosition);
        w.f32(f.spawnYaw);
        w.f32(f.mana);
        w.f32(f.manaDelay);
        w.f32(f.sinceSprint);
        w.u8(static_cast<uint8_t>(std::clamp(f.airJumpsUsed, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(f.airDashesUsed, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(f.wallLock, 0, 255)));
        w.vec3(f.wallNormal);
        w.i8(static_cast<int8_t>(std::clamp(f.chargeSlot, -1, 127)));
        w.f32(f.dashCharges);

        // The run: the spells and techniques it was given, their levels and branches
        for (const auto* list : {&f.runSpells, &f.runSkills})
        {
            w.u8(static_cast<uint8_t>(std::min<size_t>(list->size(), 8)));
            for (size_t i = 0; i < list->size() and i < 8; ++i)
            {
                w.u16(names.id((*list)[i].id));
                w.u8(static_cast<uint8_t>(std::clamp((*list)[i].level, 0, 255)));
                w.i8(static_cast<int8_t>(std::clamp((*list)[i].branch, -1, 127)));
            }
        }

        // ... and what the shop has: shards, the level, what is owned, the offers, the level up's picks
        w.u16(static_cast<uint16_t>(std::clamp(f.shards, 0, 65535)));
        w.u16(static_cast<uint16_t>(std::clamp(f.xp, 0, 65535)));
        w.u8(static_cast<uint8_t>(std::clamp(f.level, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(f.levelUps, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(f.rerolls, 0, 255)));
        w.boolean(f.ready);
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.owned.size(), 64)));
        for (size_t i = 0; i < f.owned.size() and i < 64; ++i)
            w.u16(names.id(f.owned[i]));
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.shop.size(), 8)));
        for (size_t i = 0; i < f.shop.size() and i < 8; ++i)
        {
            w.u16(names.id(f.shop[i].id));
            w.u16(static_cast<uint16_t>(std::clamp(f.shop[i].price, 0, 65535)));
            w.u8(static_cast<uint8_t>((f.shop[i].locked ? 1 : 0) | (f.shop[i].bought ? 2 : 0)));
            w.u8(static_cast<uint8_t>(std::clamp(f.shop[i].rarity, 0, 255)));
        }
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.levelOffer.size(), 8)));
        for (size_t i = 0; i < f.levelOffer.size() and i < 8; ++i)
            w.u16(names.id(f.levelOffer[i]));

        // ... presents, the interest
        w.u8(static_cast<uint8_t>(std::clamp(f.presents, 0, 255)));
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.presentOffer.size(), 8)));
        for (size_t i = 0; i < f.presentOffer.size() and i < 8; ++i)
        {
            w.u16(names.id(f.presentOffer[i].id));
            w.u8(static_cast<uint8_t>(std::clamp(f.presentOffer[i].rarity, 0, 255)));
        }
        w.u16(static_cast<uint16_t>(std::clamp(f.banked, 0, 65535)));
        w.u16(static_cast<uint16_t>(std::clamp(f.interest, 0, 65535)));
        w.boolean(f.armed);

        // ... the level ups' rerolls and banishes, and what was banished
        w.u8(static_cast<uint8_t>(std::clamp(f.levelRerolls, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(f.banishes, 0, 255)));
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.banished.size(), 64)));
        for (size_t i = 0; i < f.banished.size() and i < 64; ++i)
            w.u16(names.id(f.banished[i]));

        // ... and its damage by what dealt it (the recap)
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.stats.bySource.size(), 32)));
        size_t sources = 0;
        for (const auto& [id, damage] : f.stats.bySource)
        {
            if (++sources > 32)
                break;
            w.u16(names.id(id));
            w.u32(static_cast<uint32_t>(std::lround(std::clamp(damage, 0.0f, 4e9f))));
        }
    }

    bool readFighter(Reader& r, Fighter& f, const Names& names)
    {
        const bool full = r.boolean();
        f.id = r.u16();
        f.team = static_cast<Team>(r.u8());
        f.type = names.name(r.u16());
        f.owner = r.u16();

        if (full)
        {
            f.body.position = r.vec3();
            f.body.velocity = r.vec3();
        }
        else
        {
            for (int a = 0; a < 3; ++a)
                f.body.position[a] = r.fixed(CENTIMETRES);
            for (int a = 0; a < 3; ++a)
                f.body.velocity[a] = r.fixed(CENTIMETRES);
        }
        f.body.width = r.f32();
        f.body.height = r.f32();
        f.body.onGround = r.boolean();
        f.yaw = full ? r.f32() : r.fixed(YAW_SCALE);
        f.health = r.f32();
        f.maxHealth = r.f32();
        f.poise = r.f32();
        f.maxPoise = r.f32();
        f.action = static_cast<Action>(std::min<uint8_t>(r.u8(), static_cast<uint8_t>(Action::Dead)));
        f.frame = r.i16();
        f.actionLength = r.i16();
        f.weapon = names.name(r.u16());
        f.attack = names.name(r.u16());
        f.hitstop = r.u8();
        f.attackPitch = r.f32();
        f.dodgeDir = r.vec3();
        f.aiMove = names.name(r.u16());

        f.chargeFrames = r.u8();
        const uint8_t flags = r.u8();
        f.elite = (flags & 0x01) != 0;
        f.climbTo = (flags & 0x02) != 0 ? f.body.position.y + 1.0f : -1.0f;   // the pose only: the server climbs it
        f.blownUp = (flags & 0x04) != 0;
        const uint8_t phase = r.u8();
        f.phase = phase & 0x7F;
        f.shielded = (phase & 0x80) != 0;
        f.affixes.clear();
        const uint8_t affixes = r.u8();
        for (uint8_t k = 0; k < affixes and k < 8 and not r.failed; ++k)
            f.affixes.push_back(names.name(r.u16()));

        f.statuses.clear();
        const uint8_t statuses = r.u8();
        for (uint8_t k = 0; k < statuses and k < 16 and not r.failed; ++k)
        {
            StatusState s;
            s.id = names.name(r.u16());
            s.remaining = r.f32();
            s.stacks = r.u8();
            f.statuses.push_back(s);
        }

        const bool withExtras = f.team == Team::Player and r.boolean();
        if (withExtras)
        {
            f.boons.clear();
            const uint8_t boons = r.u8();
            for (uint8_t k = 0; k < boons and k < 64 and not r.failed; ++k)
            {
                Fighter::Boon b;
                b.id = names.name(r.u16());
                b.rarity = r.u8();
                f.boons.push_back(b);
            }

            auto& st = f.stats;
            st.kills = r.u16();
            st.shards = static_cast<int>(std::min<uint32_t>(r.u32(), 1u << 30));
            st.xp = static_cast<int>(std::min<uint32_t>(r.u32(), 1u << 30));
            st.presents = r.u8();
            st.items = r.u8();
            st.downs = r.u8();
            st.damage = static_cast<float>(r.u32());
            st.healing = static_cast<float>(r.u32());
            f.level = r.u8();
        }

        if (not full)
            return withExtras;

        f.sinceHit = r.f32();
        f.stamina = r.f32();
        f.staminaDelay = r.f32();
        f.exhausted = r.boolean();
        for (float& c : f.abilityCooldown)
            c = r.f32();
        f.mineTimer = r.f32();
        f.queued = r.u8();
        f.queuedAge = r.f32();
        f.dodgeQueuedAge = r.f32();
        f.push = r.vec3();
        f.coyote = r.f32();
        f.jumpBuffer = r.f32();
        f.jumping = r.boolean();
        f.airSpeed = r.f32();
        f.landGrace = r.f32();
        f.landSpeed = r.f32();
        f.airborneStagger = r.boolean();
        f.airDodgeAge = r.f32();
        f.heavyStagger = r.boolean();
        f.respawnTimer = r.f32();
        f.spawnPosition = r.vec3();
        f.spawnYaw = r.f32();
        f.mana = r.f32();
        f.manaDelay = r.f32();
        f.sinceSprint = r.f32();
        f.airJumpsUsed = r.u8();
        f.airDashesUsed = r.u8();
        f.wallLock = r.u8();
        f.wallNormal = r.vec3();
        f.chargeSlot = r.i8();
        f.dashCharges = r.f32();

        for (auto* list : {&f.runSpells, &f.runSkills})
        {
            list->clear();
            const uint8_t n = r.u8();
            for (uint8_t k = 0; k < n and k < 8 and not r.failed; ++k)
            {
                Fighter::RunSpell rs;
                rs.id = names.name(r.u16());
                rs.level = r.u8();
                rs.branch = r.i8();
                list->push_back(rs);
            }
        }

        f.shards = r.u16();
        f.xp = r.u16();
        f.level = r.u8();
        f.levelUps = r.u8();
        f.rerolls = r.u8();
        f.ready = r.boolean();
        f.owned.clear();
        const uint8_t owned = r.u8();
        for (uint8_t k = 0; k < owned and k < 64 and not r.failed; ++k)
            f.owned.push_back(names.name(r.u16()));
        f.shop.clear();
        const uint8_t shop = r.u8();
        for (uint8_t k = 0; k < shop and k < 8 and not r.failed; ++k)
        {
            Fighter::ShopOffer o;
            o.id = names.name(r.u16());
            o.price = r.u16();
            const uint8_t flags = r.u8();
            o.locked = flags & 1;
            o.bought = flags & 2;
            o.rarity = r.u8();
            f.shop.push_back(o);
        }
        f.levelOffer.clear();
        const uint8_t level = r.u8();
        for (uint8_t k = 0; k < level and k < 8 and not r.failed; ++k)
            f.levelOffer.push_back(names.name(r.u16()));

        f.presents = r.u8();
        f.presentOffer.clear();
        const uint8_t presents = r.u8();
        for (uint8_t k = 0; k < presents and k < 8 and not r.failed; ++k)
        {
            Fighter::ShopOffer o;
            o.id = names.name(r.u16());
            o.rarity = r.u8();
            f.presentOffer.push_back(o);
        }
        f.banked = r.u16();
        f.interest = r.u16();
        f.armed = r.boolean();

        f.levelRerolls = r.u8();
        f.banishes = r.u8();
        f.banished.clear();
        const uint8_t banished = r.u8();
        for (uint8_t k = 0; k < banished and k < 64 and not r.failed; ++k)
            f.banished.push_back(names.name(r.u16()));

        f.stats.bySource.clear();
        const uint8_t sources = r.u8();
        for (uint8_t k = 0; k < sources and k < 32 and not r.failed; ++k)
        {
            const std::string id = names.name(r.u16());
            const float damage = static_cast<float>(r.u32());
            if (not id.empty())
                f.stats.bySource[id] += damage;
        }
        return withExtras;
    }

    // ---- Snapshot ---------------------------------------------------------------------------------

    void SnapshotRecords::Section::add(uint16_t id, const uint8_t* data, size_t n)
    {
        if (offsets.empty())
            offsets.push_back(0);

        ids.push_back(id);
        bytes.insert(bytes.end(), data, data + n);
        offsets.push_back(static_cast<uint32_t>(bytes.size()));
    }

    namespace
    {
        using Section = SnapshotRecords::Section;

        // Whole: every record. As changes to a base: the ids gone, the records new or changed (a changed
        // one of the same length as only the bytes that differ, after a mask of which), then the order
        // when it is not the base's with the new ones at the end
        enum : uint8_t { WHOLE = 0, CHANGES = 1 };
        enum : uint8_t { NEW = 0, PATCH = 1 };

        void writeSection(Writer& w, const Section& now, const Section* base)
        {
            if (not base)
            {
                w.u8(WHOLE);
                w.u16(static_cast<uint16_t>(now.size()));

                for (size_t i = 0; i < now.size(); ++i)
                {
                    w.u16(now.ids[i]);
                    w.u16(static_cast<uint16_t>(now.length(i)));
                    w.bytes(now.record(i), now.length(i));
                }

                return;
            }

            w.u8(CHANGES);

            std::unordered_map<uint16_t, size_t> inBase, inNow;
            for (size_t i = 0; i < base->size(); ++i)
                inBase[base->ids[i]] = i;
            for (size_t i = 0; i < now.size(); ++i)
                inNow[now.ids[i]] = i;

            std::vector<uint16_t> gone;
            for (uint16_t id : base->ids)
                if (not inNow.count(id))
                    gone.push_back(id);

            w.u16(static_cast<uint16_t>(gone.size()));
            for (uint16_t id : gone)
                w.u16(id);

            // The order the client makes without being told: the base's, then the new ones
            std::vector<uint16_t> order;
            order.reserve(now.size());
            for (uint16_t id : base->ids)
                if (inNow.count(id))
                    order.push_back(id);

            std::vector<size_t> changed;
            for (size_t i = 0; i < now.size(); ++i)
            {
                auto b = inBase.find(now.ids[i]);

                if (b == inBase.end())
                {
                    changed.push_back(i);
                    order.push_back(now.ids[i]);
                }
                else if (base->length(b->second) != now.length(i) or std::memcmp(base->record(b->second), now.record(i), now.length(i)) != 0)
                {
                    changed.push_back(i);
                }
            }

            w.u16(static_cast<uint16_t>(changed.size()));

            for (size_t i : changed)
            {
                w.u16(now.ids[i]);
                auto b = inBase.find(now.ids[i]);
                const size_t n = now.length(i);
                const uint8_t* rec = now.record(i);

                if (b == inBase.end() or base->length(b->second) != n)
                {
                    w.u8(NEW);
                    w.u16(static_cast<uint16_t>(n));
                    w.bytes(rec, n);
                    continue;
                }

                w.u8(PATCH);
                const uint8_t* was = base->record(b->second);
                const size_t maskAt = w.data.size();
                w.data.resize(maskAt + (n + 7) / 8, 0);

                for (size_t k = 0; k < n; ++k)
                {
                    if (rec[k] == was[k])
                        continue;

                    w.data[maskAt + k / 8] |= static_cast<uint8_t>(1u << (k % 8));
                    w.u8(rec[k]);
                }
            }

            const bool reordered = order != now.ids;
            w.boolean(reordered);

            if (reordered)
            {
                w.u16(static_cast<uint16_t>(now.size()));
                for (uint16_t id : now.ids)
                    w.u16(id);
            }
        }

        bool readSection(Reader& r, Section& out, const Section* base)
        {
            const uint8_t mode = r.u8();

            if (mode == WHOLE)
            {
                const uint16_t n = r.u16();

                for (uint16_t k = 0; k < n and not r.failed; ++k)
                {
                    const uint16_t id = r.u16();
                    const uint16_t length = r.u16();
                    const uint8_t* rec = r.bytes(length);
                    if (rec)
                        out.add(id, rec, length);
                }

                return not r.failed;
            }

            if (mode != CHANGES or not base)
                return false;

            std::unordered_map<uint16_t, size_t> inBase;
            for (size_t i = 0; i < base->size(); ++i)
                inBase[base->ids[i]] = i;

            std::unordered_set<uint16_t> gone;
            const uint16_t goneCount = r.u16();
            for (uint16_t k = 0; k < goneCount and not r.failed; ++k)
                gone.insert(r.u16());

            std::vector<uint16_t> order;
            order.reserve(base->size());
            for (uint16_t id : base->ids)
                if (not gone.count(id))
                    order.push_back(id);

            std::unordered_map<uint16_t, std::vector<uint8_t>> changed;
            const uint16_t changedCount = r.u16();

            for (uint16_t k = 0; k < changedCount and not r.failed; ++k)
            {
                const uint16_t id = r.u16();
                const uint8_t kind = r.u8();
                auto b = inBase.find(id);
                const bool known = b != inBase.end() and not gone.count(id);

                if (kind == NEW)
                {
                    const uint16_t length = r.u16();
                    const uint8_t* rec = r.bytes(length);
                    if (not rec)
                        return false;

                    if (not known and not changed.count(id))
                        order.push_back(id);
                    changed[id].assign(rec, rec + length);
                }
                else if (kind == PATCH and known)
                {
                    const size_t n = base->length(b->second);
                    const uint8_t* mask = r.bytes((n + 7) / 8);
                    if (not mask)
                        return false;

                    std::vector<uint8_t> rec(base->record(b->second), base->record(b->second) + n);
                    for (size_t i = 0; i < n; ++i)
                        if (mask[i / 8] & (1u << (i % 8)))
                            rec[i] = r.u8();

                    changed[id] = std::move(rec);
                }
                else
                {
                    return false;
                }
            }

            if (r.boolean())
            {
                order.clear();
                const uint16_t n = r.u16();
                for (uint16_t k = 0; k < n and not r.failed; ++k)
                    order.push_back(r.u16());
            }

            if (r.failed)
                return false;

            for (uint16_t id : order)
            {
                if (auto c = changed.find(id); c != changed.end())
                {
                    out.add(id, c->second.data(), c->second.size());
                    continue;
                }

                auto b = inBase.find(id);
                if (b == inBase.end() or gone.count(id))
                    return false;

                out.add(id, base->record(b->second), base->length(b->second));
            }

            return true;
        }

        /** The records of a snapshot's fighters, projectiles, shards and zones (an id taken twice keeps its first). */
        SnapshotRecords recordsOf(const SnapshotMsg& m, const Names& names)
        {
            SnapshotRecords out;
            out.seq = m.seq;
            Writer w;

            auto put = [&](Section& section, std::unordered_set<uint16_t>& taken, int id) {
                if (taken.insert(static_cast<uint16_t>(id)).second)
                    section.add(static_cast<uint16_t>(id), w.data.data(), w.data.size());
                w.data.clear();
            };

            std::unordered_set<uint16_t> taken;
            for (const auto& f : m.fighters)
            {
                writeFighter(w, f, f.id == m.playerId, names, m.extras);
                put(out.sections[SnapshotRecords::FIGHTERS], taken, f.id);
            }

            taken.clear();
            for (const auto& p : m.projectiles)
            {
                w.vec3(p.position);
                w.vec3(p.velocity);
                w.u16(names.id(p.source));
                w.u16(names.id(p.attack));
                put(out.sections[SnapshotRecords::PROJECTILES], taken, p.id);
            }

            // Shards: where they are, to a centimetre
            taken.clear();
            for (const auto& p : m.pickups)
            {
                for (int a = 0; a < 3; ++a)
                    w.fixed(p.position[a], CENTIMETRES);
                w.u8(static_cast<uint8_t>(std::clamp(p.value, 0, 255)));
                w.u16(static_cast<uint16_t>(p.target));
                w.boolean(p.present);
                put(out.sections[SnapshotRecords::PICKUPS], taken, p.id);
            }

            // Zones: the tick they end at rather than the time left, so a zone's record never changes
            taken.clear();
            for (size_t i = 0; i < m.zones.size() and i < MAX_SNAPSHOT_ZONES; ++i)
            {
                const ZoneState& z = m.zones[i];
                const float left = std::isfinite(z.remaining) ? std::clamp(z.remaining, 0.0f, 3600.0f) : 0.0f;
                w.u8(static_cast<uint8_t>(z.element));
                w.vec3(z.position);
                w.f32(z.radius);
                w.u32(m.tick + static_cast<uint32_t>(std::lround(left / STEP_SECONDS)));
                w.f32(z.duration);
                w.f32(z.heal);
                w.boolean(z.blast);
                w.boolean(z.enemy);
                w.u16(static_cast<uint16_t>(std::clamp(z.owner, 0, 65535)));
                put(out.sections[SnapshotRecords::ZONES], taken, z.id);
            }

            return out;
        }
    }

    std::vector<uint8_t> encode(const SnapshotMsg& m, const Names& names)
    {
        return encode(m, names, nullptr, nullptr);
    }

    std::vector<uint8_t> encode(const SnapshotMsg& m, const Names& names, const SnapshotRecords* base, SnapshotRecords* sent)
    {
        SnapshotRecords records = recordsOf(m, names);

        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Snapshot));
        w.u32(m.seq);
        w.u32(base ? base->seq : 0);
        w.u32(m.tick);
        w.u32(m.ackSeq);
        w.u32(m.echoTimeMs);
        w.u16(static_cast<uint16_t>(m.playerId));

        for (int s = 0; s < SnapshotRecords::SECTIONS; ++s)
            writeSection(w, records.sections[s], base ? &base->sections[s] : nullptr);

        w.u16(static_cast<uint16_t>(std::min<size_t>(m.pieces.size(), 65535)));
        for (size_t i = 0; i < m.pieces.size() and i < 65535; ++i)
        {
            w.u16(static_cast<uint16_t>(m.pieces[i].id));
            w.f32(m.pieces[i].drop);
        }

        w.u8(static_cast<uint8_t>(m.run.phase));
        w.u8(static_cast<uint8_t>(std::clamp(m.run.wave, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(m.run.waves, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(m.run.difficulty, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(m.run.levelsOpen, 0, 255)));
        w.u16(static_cast<uint16_t>(m.run.chooser));
        w.f32(m.run.timer);
        w.f32(m.run.elapsed);
        w.u32(static_cast<uint32_t>(std::max(0, m.run.score)));
        w.u16(static_cast<uint16_t>(std::clamp(m.run.kills, 0, 65535)));
        w.u16(static_cast<uint16_t>(std::clamp(m.run.enemiesLeft, 0, 65535)));
        w.u16(static_cast<uint16_t>(m.run.bossId));
        w.boolean(m.run.bossWave);
        w.u16(static_cast<uint16_t>(m.run.champion));
        w.f32(m.run.warmup);

        w.u8(static_cast<uint8_t>(std::min<size_t>(m.events.size(), 255)));
        for (size_t i = 0; i < m.events.size() and i < 255; ++i)
        {
            const auto& e = m.events[i];
            w.u16(names.id(e.kind));
            w.vec3(e.position);
            w.u16(e.block);
            w.f32(e.strength);
            w.u16(static_cast<uint16_t>(e.fighter));
            w.u16(static_cast<uint16_t>(e.source));
            w.boolean(e.line);
            if (e.line)
                w.vec3(e.to);
            w.u8(static_cast<uint8_t>(std::clamp(static_cast<int64_t>(m.tick) - e.tick, int64_t(0), int64_t(255))));   // ticks before the snapshot
        }

        if (sent)
            *sent = std::move(records);

        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, SnapshotMsg& m, const Names& names)
    {
        return decode(data, m, names, nullptr, nullptr);
    }

    bool decode(const std::vector<uint8_t>& data, SnapshotMsg& m, const Names& names,
                const std::function<const SnapshotRecords*(uint32_t seq)>& baseOf, SnapshotRecords* got)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Snapshot))
            return false;

        m.seq = r.u32();
        m.baseSeq = r.u32();

        // Changes to a snapshot this client no longer has (or never read): nothing to build on
        const SnapshotRecords* base = nullptr;
        if (m.baseSeq != 0)
        {
            base = baseOf ? baseOf(m.baseSeq) : nullptr;
            if (not base)
                return false;
        }

        m.tick = r.u32();
        m.ackSeq = r.u32();
        m.echoTimeMs = r.u32();
        m.playerId = r.u16();

        SnapshotRecords records;
        records.seq = m.seq;

        for (int s = 0; s < SnapshotRecords::SECTIONS; ++s)
            if (not readSection(r, records.sections[s], base ? &base->sections[s] : nullptr))
                return false;

        const uint16_t pieces = r.u16();
        for (uint16_t k = 0; k < pieces and not r.failed; ++k)
        {
            PieceState p;
            p.id = r.u16();
            p.drop = r.f32();
            m.pieces.push_back(p);
        }

        m.run.phase = static_cast<RunPhase>(std::min<uint8_t>(r.u8(), static_cast<uint8_t>(RunPhase::Difficulty)));
        m.run.wave = r.u8();
        m.run.waves = r.u8();
        m.run.difficulty = r.u8();
        m.run.levelsOpen = r.u8();
        m.run.chooser = r.u16();
        m.run.timer = r.f32();
        m.run.elapsed = r.f32();
        m.run.score = static_cast<int>(r.u32());
        m.run.kills = r.u16();
        m.run.enemiesLeft = r.u16();
        m.run.bossId = r.u16();
        m.run.bossWave = r.boolean();
        m.run.champion = r.u16();
        m.run.warmup = r.f32();

        const uint8_t events = r.u8();
        for (uint8_t k = 0; k < events and not r.failed; ++k)
        {
            WorldEvent e;
            e.kind = names.name(r.u16());
            e.position = r.vec3();
            e.block = r.u16();
            e.strength = r.f32();
            e.fighter = r.u16();
            e.source = r.u16();
            e.line = r.boolean();
            if (e.line)
                e.to = r.vec3();
            e.tick = static_cast<int>(m.tick) - r.u8();
            m.events.push_back(e);
        }

        if (r.failed)
            return false;

        // The records, read as the snapshot's own parts
        const Section& fighters = records.sections[SnapshotRecords::FIGHTERS];
        for (size_t i = 0; i < fighters.size(); ++i)
        {
            Reader fr(fighters.record(i), fighters.length(i));
            Fighter f;
            if (readFighter(fr, f, names))
                m.withExtras.push_back(f.id);
            if (fr.failed)
                return false;
            m.fighters.push_back(std::move(f));
        }

        const Section& projectiles = records.sections[SnapshotRecords::PROJECTILES];
        for (size_t i = 0; i < projectiles.size(); ++i)
        {
            Reader pr(projectiles.record(i), projectiles.length(i));
            ProjectileState p;
            p.id = projectiles.ids[i];
            p.position = pr.vec3();
            p.velocity = pr.vec3();
            p.source = names.name(pr.u16());
            p.attack = names.name(pr.u16());
            if (pr.failed)
                return false;
            m.projectiles.push_back(p);
        }

        const Section& pickups = records.sections[SnapshotRecords::PICKUPS];
        for (size_t i = 0; i < pickups.size(); ++i)
        {
            Reader pr(pickups.record(i), pickups.length(i));
            Pickup p;
            p.id = pickups.ids[i];
            for (int a = 0; a < 3; ++a)
                p.position[a] = pr.fixed(CENTIMETRES);
            p.value = pr.u8();
            p.target = pr.u16();
            p.present = pr.boolean();
            if (pr.failed)
                return false;
            m.pickups.push_back(p);
        }

        const Section& zones = records.sections[SnapshotRecords::ZONES];
        for (size_t i = 0; i < zones.size() and i < MAX_SNAPSHOT_ZONES; ++i)
        {
            Reader zr(zones.record(i), zones.length(i));
            ZoneState z;
            z.id = zones.ids[i];
            z.element = static_cast<Element>(std::min<uint8_t>(zr.u8(), static_cast<uint8_t>(Element::Volt)));
            z.position = zr.vec3();
            z.radius = zr.f32();
            const uint32_t endTick = zr.u32();
            z.remaining = static_cast<float>(static_cast<int64_t>(endTick) - static_cast<int64_t>(m.tick)) * STEP_SECONDS;
            z.duration = zr.f32();
            z.heal = zr.f32();
            z.blast = zr.boolean();
            z.enemy = zr.boolean();
            z.owner = zr.u16();
            if (zr.failed)
                return false;
            m.zones.push_back(z);
        }

        if (got)
            *got = std::move(records);

        return true;
    }

    // ---- Terrain ----------------------------------------------------------------------------------

    std::vector<uint8_t> encode(const TerrainDeltaMsg& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::TerrainDelta));
        w.u32(static_cast<uint32_t>(m.blocks.size()));

        for (const auto& [index, id] : m.blocks)
        {
            w.u32(index);
            w.u16(id);
        }

        writeDamage(w, m.damage);
        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, TerrainDeltaMsg& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::TerrainDelta))
            return false;

        const uint32_t n = r.u32();
        if (r.left() / 6u < n)
            return false;

        for (uint32_t k = 0; k < n; ++k)
        {
            const uint32_t index = r.u32();
            m.blocks.emplace_back(index, r.u16());
        }

        readDamage(r, m.damage);
        return not r.failed;
    }

    std::vector<uint8_t> encode(const WorldInitMsg& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::WorldInit));
        w.u16(static_cast<uint16_t>(m.size.x));
        w.u16(static_cast<uint16_t>(m.size.y));
        w.u16(static_cast<uint16_t>(m.size.z));
        writeCells(w, m.cells);
        writeDamage(w, m.damage);
        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, WorldInitMsg& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::WorldInit))
            return false;

        m.size.x = r.u16();
        m.size.y = r.u16();
        m.size.z = r.u16();

        if (m.size.x <= 0 or m.size.y <= 0 or m.size.z <= 0 or m.size.x > 512 or m.size.y > 512 or m.size.z > 512)
            return false;

        if (not readCells(r, m.cells, static_cast<size_t>(m.size.x) * m.size.y * m.size.z))
            return false;

        readDamage(r, m.damage);
        return not r.failed;
    }

    std::vector<uint8_t> encode(const PieceSpawnMsg& m)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::PieceSpawn));
        w.u16(static_cast<uint16_t>(m.id));
        w.i16(static_cast<int16_t>(m.origin.x));
        w.i16(static_cast<int16_t>(m.origin.y));
        w.i16(static_cast<int16_t>(m.origin.z));
        w.f32(m.drop);
        w.u8(static_cast<uint8_t>(m.cells.width()));
        w.u8(static_cast<uint8_t>(m.cells.height()));
        w.u8(static_cast<uint8_t>(m.cells.depth()));
        writeCells(w, m.cells.raw());
        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, PieceSpawnMsg& m)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::PieceSpawn))
            return false;

        m.id = r.u16();
        m.origin.x = r.i16();
        m.origin.y = r.i16();
        m.origin.z = r.i16();
        m.drop = r.f32();
        const int w = r.u8(), h = r.u8(), d = r.u8();

        if (w <= 0 or h <= 0 or d <= 0)
            return false;

        std::vector<BlockId> cells;
        if (not readCells(r, cells, static_cast<size_t>(w) * h * d))
            return false;

        m.cells = VoxelGrid(w, h, d);
        m.cells.outsideSolid = false;

        for (int y = 0; y < h; ++y)
            for (int z = 0; z < d; ++z)
                for (int x = 0; x < w; ++x)
                    m.cells.set(x, y, z, cells[m.cells.index(x, y, z)]);

        return not r.failed;
    }
}
