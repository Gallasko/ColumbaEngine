#include "protocol.h"

#include <algorithm>
#include <set>

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
            "wave_start", "arena_reset", "despawn", "elite_warn", "pickup", "level_up", "buy", "crit",
        };

        const char* const AI_MOVES[] = { "hold", "chase", "strafe", "retreat", "flank" };

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

            if (count != expected or runs > count or r.left() < runs * 4u)
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

            if (r.left() < n * 5u)
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

        for (const auto& [id, a] : content.abilities)
        {
            all.insert(id);
            if (a.kind == "attack")
                all.insert("@" + id);   // a weapon skill's attack, while it runs
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
            p.name = r.str();
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

    void writeFighter(Writer& w, const Fighter& f, bool full, const Names& names)
    {
        w.boolean(full);
        w.u16(static_cast<uint16_t>(f.id));
        w.u8(static_cast<uint8_t>(f.team));
        w.u16(names.id(f.type));
        w.u16(static_cast<uint16_t>(f.owner));

        // What every fighter needs to be drawn
        w.vec3(f.body.position);
        w.vec3(f.body.velocity);
        w.f32(f.body.width);
        w.f32(f.body.height);
        w.boolean(f.body.onGround);
        w.f32(f.yaw);
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
        w.boolean(f.elite);

        // Statuses: they slow and they show
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.statuses.size(), 16)));
        for (size_t i = 0; i < f.statuses.size() and i < 16; ++i)
        {
            w.u16(names.id(f.statuses[i].id));
            w.f32(f.statuses[i].remaining);
            w.u8(static_cast<uint8_t>(std::clamp(f.statuses[i].stacks, 0, 255)));
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

        // The run: the boons picked and the ones offered now
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.boons.size(), 64)));
        for (size_t i = 0; i < f.boons.size() and i < 64; ++i)
            w.u16(names.id(f.boons[i]));

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
        }
        w.u8(static_cast<uint8_t>(std::min<size_t>(f.levelOffer.size(), 8)));
        for (size_t i = 0; i < f.levelOffer.size() and i < 8; ++i)
            w.u16(names.id(f.levelOffer[i]));
    }

    void readFighter(Reader& r, Fighter& f, const Names& names)
    {
        const bool full = r.boolean();
        f.id = r.u16();
        f.team = static_cast<Team>(r.u8());
        f.type = names.name(r.u16());
        f.owner = r.u16();

        f.body.position = r.vec3();
        f.body.velocity = r.vec3();
        f.body.width = r.f32();
        f.body.height = r.f32();
        f.body.onGround = r.boolean();
        f.yaw = r.f32();
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
        f.elite = r.boolean();

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

        if (not full)
            return;

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

        f.boons.clear();
        const uint8_t boons = r.u8();
        for (uint8_t k = 0; k < boons and k < 64 and not r.failed; ++k)
            f.boons.push_back(names.name(r.u16()));

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
            f.shop.push_back(o);
        }
        f.levelOffer.clear();
        const uint8_t level = r.u8();
        for (uint8_t k = 0; k < level and k < 8 and not r.failed; ++k)
            f.levelOffer.push_back(names.name(r.u16()));
    }

    // ---- Snapshot ---------------------------------------------------------------------------------

    std::vector<uint8_t> encode(const SnapshotMsg& m, const Names& names)
    {
        Writer w;
        w.u8(static_cast<uint8_t>(Msg::Snapshot));
        w.u32(m.tick);
        w.u32(m.ackSeq);
        w.u32(m.echoTimeMs);
        w.u16(static_cast<uint16_t>(m.playerId));

        w.u8(static_cast<uint8_t>(m.fighters.size()));
        for (const auto& f : m.fighters)
            writeFighter(w, f, f.id == m.playerId, names);

        w.u8(static_cast<uint8_t>(std::min<size_t>(m.projectiles.size(), 255)));
        for (size_t i = 0; i < m.projectiles.size() and i < 255; ++i)
        {
            const auto& p = m.projectiles[i];
            w.u16(static_cast<uint16_t>(p.id));
            w.vec3(p.position);
            w.vec3(p.velocity);
            w.u16(names.id(p.source));
            w.u16(names.id(p.attack));
        }

        w.u8(static_cast<uint8_t>(std::min<size_t>(m.pieces.size(), 255)));
        for (size_t i = 0; i < m.pieces.size() and i < 255; ++i)
        {
            w.u16(static_cast<uint16_t>(m.pieces[i].id));
            w.f32(m.pieces[i].drop);
        }

        w.u8(static_cast<uint8_t>(m.run.phase));
        w.u8(static_cast<uint8_t>(std::clamp(m.run.wave, 0, 255)));
        w.u8(static_cast<uint8_t>(std::clamp(m.run.waves, 0, 255)));
        w.f32(m.run.timer);
        w.f32(m.run.elapsed);
        w.u32(static_cast<uint32_t>(std::max(0, m.run.score)));
        w.u16(static_cast<uint16_t>(std::clamp(m.run.kills, 0, 65535)));
        w.u8(static_cast<uint8_t>(std::clamp(m.run.enemiesLeft, 0, 255)));
        w.u16(static_cast<uint16_t>(m.run.bossId));
        w.boolean(m.run.bossWave);
        w.u16(static_cast<uint16_t>(m.run.champion));
        w.f32(m.run.warmup);

        // Shards: where they are, to a centimetre
        w.u8(static_cast<uint8_t>(std::min<size_t>(m.pickups.size(), 255)));
        for (size_t i = 0; i < m.pickups.size() and i < 255; ++i)
        {
            const Pickup& p = m.pickups[i];
            w.u16(static_cast<uint16_t>(p.id));
            for (int a = 0; a < 3; ++a)
                w.i16(static_cast<int16_t>(std::clamp(std::lround(p.position[a] * 100.0f), -32767l, 32767l)));
            w.u8(static_cast<uint8_t>(std::clamp(p.value, 0, 255)));
            w.u16(static_cast<uint16_t>(p.target));
        }

        w.u8(static_cast<uint8_t>(std::min<size_t>(m.zones.size(), 64)));
        for (size_t i = 0; i < m.zones.size() and i < 64; ++i)
        {
            const ZoneState& z = m.zones[i];
            w.u16(static_cast<uint16_t>(z.id));
            w.u8(static_cast<uint8_t>(z.element));
            w.vec3(z.position);
            w.f32(z.radius);
            w.f32(z.remaining);
            w.f32(z.duration);
            w.f32(z.heal);
        }

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

        return w.data;
    }

    bool decode(const std::vector<uint8_t>& data, SnapshotMsg& m, const Names& names)
    {
        Reader r(data);
        if (r.u8() != static_cast<uint8_t>(Msg::Snapshot))
            return false;

        m.tick = r.u32();
        m.ackSeq = r.u32();
        m.echoTimeMs = r.u32();
        m.playerId = r.u16();

        const uint8_t fighters = r.u8();
        for (uint8_t k = 0; k < fighters and not r.failed; ++k)
        {
            Fighter f;
            readFighter(r, f, names);
            m.fighters.push_back(std::move(f));
        }

        const uint8_t projectiles = r.u8();
        for (uint8_t k = 0; k < projectiles and not r.failed; ++k)
        {
            ProjectileState p;
            p.id = r.u16();
            p.position = r.vec3();
            p.velocity = r.vec3();
            p.source = names.name(r.u16());
            p.attack = names.name(r.u16());
            m.projectiles.push_back(p);
        }

        const uint8_t pieces = r.u8();
        for (uint8_t k = 0; k < pieces and not r.failed; ++k)
        {
            PieceState p;
            p.id = r.u16();
            p.drop = r.f32();
            m.pieces.push_back(p);
        }

        m.run.phase = static_cast<RunPhase>(std::min<uint8_t>(r.u8(), static_cast<uint8_t>(RunPhase::Armory)));
        m.run.wave = r.u8();
        m.run.waves = r.u8();
        m.run.timer = r.f32();
        m.run.elapsed = r.f32();
        m.run.score = static_cast<int>(r.u32());
        m.run.kills = r.u16();
        m.run.enemiesLeft = r.u8();
        m.run.bossId = r.u16();
        m.run.bossWave = r.boolean();
        m.run.champion = r.u16();
        m.run.warmup = r.f32();

        const uint8_t pickups = r.u8();
        for (uint8_t k = 0; k < pickups and not r.failed; ++k)
        {
            Pickup p;
            p.id = r.u16();
            for (int a = 0; a < 3; ++a)
                p.position[a] = static_cast<float>(r.i16()) / 100.0f;
            p.value = r.u8();
            p.target = r.u16();
            m.pickups.push_back(p);
        }

        const uint8_t zones = r.u8();
        for (uint8_t k = 0; k < zones and k < 64 and not r.failed; ++k)
        {
            ZoneState z;
            z.id = r.u16();
            z.element = static_cast<Element>(std::min<uint8_t>(r.u8(), static_cast<uint8_t>(Element::Volt)));
            z.position = r.vec3();
            z.radius = r.f32();
            z.remaining = r.f32();
            z.duration = r.f32();
            z.heal = r.f32();
            m.zones.push_back(z);
        }

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

        return not r.failed;
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
        if (r.left() < n * 6u)
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
