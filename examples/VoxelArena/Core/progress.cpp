#include "progress.h"

#include "json.h"
#include "paths.h"

#include "serialization.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace arena
{
    std::string progressPath()
    {
        return (std::filesystem::path(userDataDir()) / "progress.sz").string();
    }

    namespace
    {
        constexpr const char* OBJECT = "Progress";
        constexpr int VERSION = 1;

        /** As the serializer writes it: the goals reached as a list, the stats as a map (unordered: no order to keep). */
        struct Stored
        {
            int version = 0;
            std::vector<std::string> done;
            std::unordered_map<std::string, float> stats;
        };
    }
}

namespace pg
{
    template <>
    void serialize(Archive& archive, const arena::Stored& value)
    {
        archive.startSerialization(arena::OBJECT);

        serialize(archive, "version", value.version);
        serialize(archive, "done", value.done);
        serialize(archive, "stats", value.stats);

        archive.endSerialization();
    }

    template <>
    arena::Stored deserialize(const UnserializedObject& serialized)
    {
        arena::Stored data;

        if (serialized.isNull())
            return data;

        defaultDeserialize(serialized, "version", data.version);
        defaultDeserialize(serialized, "done", data.done);
        defaultDeserialize(serialized, "stats", data.stats);

        return data;
    }
}

namespace arena
{
    bool loadProgress(const std::string& path, Progress& out)
    {
        if (not std::filesystem::exists(path))
            return false;

        pg::Serializer serializer(false);
        serializer.setFile(path);

        if (serializer.getSerializedMap().count(OBJECT) == 0)
            return false;

        const Stored stored = serializer.deserializeObject<Stored>(OBJECT);
        if (stored.version <= 0)
            return false;

        Progress p;
        p.stats.insert(stored.stats.begin(), stored.stats.end());
        p.done.insert(stored.done.begin(), stored.done.end());
        out = std::move(p);
        return true;
    }

    bool importJsonProgress(const std::string& path, Progress& out)
    {
        std::ifstream file(path);
        if (not file)
            return false;

        std::stringstream text;
        text << file.rdbuf();
        const JsonParseResult parsed = parseJson(text.str());
        if (not parsed.ok or not parsed.value.isObject())
            return false;

        Progress p;
        if (const Json* stats = parsed.value.find("stats"); stats and stats->isObject())
            for (const auto& [key, value] : stats->object)
                if (value.isNumber())
                    p.stats[key] = static_cast<float>(value.number);
        if (const Json* done = parsed.value.find("done"); done and done->isArray())
            for (const auto& id : done->array)
                if (id.isString())
                    p.done.insert(id.string);

        out = std::move(p);
        return true;
    }

    bool saveProgress(const std::string& path, const Progress& p)
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        Stored stored;
        stored.version = VERSION;
        stored.done.assign(p.done.begin(), p.done.end());
        stored.stats.insert(p.stats.begin(), p.stats.end());

        // What the file held is not read back into it: the serializer would write the object a second time
        pg::Serializer serializer(false);
        serializer.setFile(path);
        serializer.clear();
        serializer.serializeObject(OBJECT, stored);
        serializer.save();

        return std::filesystem::exists(path);
    }

    void countRun(Progress& p, const Fighter& player, const RunState& run, bool won)
    {
        auto& s = p.stats;
        const auto& st = player.stats;

        auto best = [&](const std::string& key, float value) { s[key] = std::max(s[key], value); };

        s["runs"] += 1;
        s["kills"] += static_cast<float>(st.kills);
        s["damage"] += st.damage;
        s["items"] += static_cast<float>(st.items);
        s["presents"] += static_cast<float>(st.presents);
        s["shards"] += static_cast<float>(st.shards);

        for (const auto& [source, amount] : st.bySource)
            s["damage:" + source] += amount;

        // The world's counts: "elites:" and "combos:" are all of them
        for (const auto& [key, amount] : st.counts)
            s[key.back() == ':' ? key.substr(0, key.size() - 1) : key] += amount;

        const int cleared = won ? run.waves : std::max(0, run.wave - 1);
        s["waves"] += static_cast<float>(cleared);
        best("bestWave", static_cast<float>(cleared));
        best("bestLevel", static_cast<float>(player.level));
        if (not player.weapon.empty())
            best("bestWave:" + player.weapon, static_cast<float>(cleared));

        if (won)
        {
            s["wins"] += 1;
            if (not player.weapon.empty())
                s["wins:" + player.weapon] += 1;
            // A win at a level counts for the easier ones too
            for (int level = 0; level <= run.difficulty; ++level)
                s["wins:level:" + std::to_string(level + 1)] += 1;
        }
    }

    float goalProgress(const Progress& p, const AchievementDef& a)
    {
        const auto it = p.stats.find(a.stat);
        return it == p.stats.end() ? 0.0f : std::min(it->second, a.goal);
    }

    std::vector<std::string> reachGoals(Progress& p, const Content& content)
    {
        std::vector<std::string> reached;
        for (const auto& id : content.achievementOrder)
        {
            const AchievementDef& a = content.achievements.at(id);
            if (p.done.count(id) == 0 and goalProgress(p, a) >= a.goal)
            {
                p.done.insert(id);
                reached.push_back(id);
            }
        }
        return reached;
    }

    std::set<std::string> lockedContent(const Progress& p, const Content& content)
    {
        std::set<std::string> locked;
        if (p.unlockAll)
            return locked;

        for (const auto& [id, goals] : content.unlockedBy)
            if (std::none_of(goals.begin(), goals.end(), [&](const std::string& g) { return p.done.count(g) > 0; }))
                locked.insert(id);
        return locked;
    }
}
