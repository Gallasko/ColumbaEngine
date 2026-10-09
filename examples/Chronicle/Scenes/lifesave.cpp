#include "lifesave.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>

#include "logger.h"
#include "serialization.h"
#include "Files/filemanager.h"

#include "UI/resourceledger.h"   // LedgerTone

using namespace pg;

namespace pg
{
    template <>
    void serialize(Archive& archive, const chronicle::LogEntry& value)
    {
        archive.startSerialization("LogEntry");

        serialize(archive, "age", value.age);
        serialize(archive, "text", value.text);
        serialize(archive, "kind", static_cast<int>(value.kind));
        serialize(archive, "figure", value.figure);
        serialize(archive, "glyph", value.glyph);

        archive.endSerialization();
    }

    template <>
    chronicle::LogEntry deserialize(const UnserializedObject& serialized)
    {
        chronicle::LogEntry data;

        if (serialized.isNull())
            return data;

        int kind = 0;

        defaultDeserialize(serialized, "age", data.age);
        defaultDeserialize(serialized, "text", data.text);
        defaultDeserialize(serialized, "kind", kind);
        defaultDeserialize(serialized, "figure", data.figure);
        defaultDeserialize(serialized, "glyph", data.glyph);

        data.kind = static_cast<chronicle::LogKind>(kind);

        return data;
    }

    template <>
    void serialize(Archive& archive, const chronicle::LifeResource& value)
    {
        archive.startSerialization("LifeResource");

        serialize(archive, "group", value.group);
        serialize(archive, "groupLabel", value.groupLabel);
        serialize(archive, "id", value.id);
        serialize(archive, "glyph", value.glyph);
        serialize(archive, "name", value.name);
        serialize(archive, "value", value.value);
        serialize(archive, "rate", value.rate);
        serialize(archive, "tone", value.tone);
        serialize(archive, "muted", value.muted);

        archive.endSerialization();
    }

    template <>
    chronicle::LifeResource deserialize(const UnserializedObject& serialized)
    {
        chronicle::LifeResource data;

        if (serialized.isNull())
            return data;

        defaultDeserialize(serialized, "group", data.group);
        defaultDeserialize(serialized, "groupLabel", data.groupLabel);
        defaultDeserialize(serialized, "id", data.id);
        defaultDeserialize(serialized, "glyph", data.glyph);
        defaultDeserialize(serialized, "name", data.name);
        defaultDeserialize(serialized, "value", data.value);
        defaultDeserialize(serialized, "rate", data.rate);
        defaultDeserialize(serialized, "tone", data.tone);
        defaultDeserialize(serialized, "muted", data.muted);

        return data;
    }

    template <>
    void serialize(Archive& archive, const chronicle::LifeSave& value)
    {
        archive.startSerialization("LifeSave");

        serialize(archive, "name", value.name);
        serialize(archive, "profession", value.profession);
        serialize(archive, "origin", value.origin);
        serialize(archive, "aim", value.aim);
        serialize(archive, "age", value.age);
        serialize(archive, "world", value.world);
        serialize(archive, "lives", value.lives);
        serialize(archive, "running", value.running);
        serialize(archive, "monthsIn", value.monthsIn);
        serialize(archive, "stats", value.stats);
        serialize(archive, "parts", value.parts);
        serialize(archive, "skills", value.skills);
        serialize(archive, "resources", value.resources);
        serialize(archive, "log", value.log);
        serialize(archive, "done", value.done);
        serialize(archive, "achieved", value.achieved);
        serialize(archive, "picks", value.picks);
        serialize(archive, "begun", value.begun);
        serialize(archive, "atOnce", value.atOnce);
        serialize(archive, "skips", value.skips);
        serialize(archive, "guide", value.guide);
        serialize(archive, "guideSkipped", value.guideSkipped);

        archive.endSerialization();
    }

    template <>
    chronicle::LifeSave deserialize(const UnserializedObject& serialized)
    {
        chronicle::LifeSave data;

        if (serialized.isNull())
            return data;

        defaultDeserialize(serialized, "name", data.name);
        defaultDeserialize(serialized, "profession", data.profession);
        defaultDeserialize(serialized, "origin", data.origin);
        defaultDeserialize(serialized, "aim", data.aim);
        defaultDeserialize(serialized, "age", data.age);
        defaultDeserialize(serialized, "world", data.world);
        defaultDeserialize(serialized, "lives", data.lives);
        defaultDeserialize(serialized, "running", data.running);
        defaultDeserialize(serialized, "monthsIn", data.monthsIn);
        defaultDeserialize(serialized, "stats", data.stats);
        defaultDeserialize(serialized, "parts", data.parts);
        defaultDeserialize(serialized, "skills", data.skills);
        defaultDeserialize(serialized, "resources", data.resources);
        defaultDeserialize(serialized, "log", data.log);
        defaultDeserialize(serialized, "done", data.done);
        defaultDeserialize(serialized, "achieved", data.achieved);
        defaultDeserialize(serialized, "picks", data.picks);
        defaultDeserialize(serialized, "begun", data.begun);
        defaultDeserialize(serialized, "atOnce", data.atOnce);
        defaultDeserialize(serialized, "skips", data.skips);
        defaultDeserialize(serialized, "guide", data.guide);
        defaultDeserialize(serialized, "guideSkipped", data.guideSkipped);

        return data;
    }
}

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Save";

        constexpr const char * const ObjectName = "life";

        std::string textOf(const ElementMap& map, const std::string& key)
        {
            auto it = map.find(key);

            return it == map.end() ? std::string() : it->second.toString();
        }
    }

    ElementMap LifeSave::character() const
    {
        ElementMap map;

        for (const auto& [key, value] : stats)
            map[key] = ElementType{value};

        return map;
    }

    ElementMap LifeSave::terms() const
    {
        ElementMap map;

        for (const auto& [key, value] : done)
            map[key] = ElementType{value};

        return map;
    }

    int LifeSave::termsDone() const
    {
        int count = 0;

        for (const auto& [id, times] : done)
            count += times;

        return count;
    }

    std::vector<LifeResource> LifeSave::holdEarned(const RecordList& rows)
    {
        std::vector<LifeResource> earned;

        for (const auto& row : rows)
        {
            const std::string id = textOf(row, "id");
            auto stat = stats.find(id);

            if (stat == stats.end() or stat->second <= 0)
                continue;

            const bool held = std::any_of(resources.begin(), resources.end(), [&id](const LifeResource& r) { return r.id == id; });

            if (held)
                continue;

            auto tone = row.find("tone");

            LifeResource resource;
            resource.group = textOf(row, "group");
            resource.groupLabel = textOf(row, "groupLabel");
            resource.id = id;
            resource.glyph = textOf(row, "glyph");
            resource.name = textOf(row, "name");
            resource.tone = tone != row.end() and tone->second.type == UnionType::INT ? tone->second.get<int>() : 0;

            resources.push_back(resource);
            earned.push_back(resource);
        }

        return earned;
    }

    std::string LifeSave::digest() const
    {
        const int terms = termsDone();

        // The deeds alone: a step of the guide or a line of lore is kept with them, under its own name
        size_t deeds = 0;

        for (const auto& id : achieved)
        {
            if (id.rfind("guide.", 0) != 0 and id.rfind("lore.", 0) != 0)
                ++deeds;
        }

        char years[16];
        std::snprintf(years, sizeof(years), "%.2f", age);

        // The ids are the rules' own, plain words: nothing in them to escape
        std::ostringstream line;

        line << "{\"age\":" << years << ",\"world\":" << world << ",\"aim\":\"" << aim << "\",\"running\":\"" << running << "\",\"monthsIn\":" << monthsIn
             << ",\"terms\":" << terms << ",\"deeds\":" << deeds << ",\"log\":" << log.size() << ",\"lives\":" << lives
             << ",\"picks\":" << picks << ",\"begun\":" << begun << ",\"atOnce\":" << atOnce << ",\"skips\":" << skips << ",\"guide\":" << guide << ",\"guideSkipped\":" << guideSkipped << "}";

        return line.str();
    }

    bool LifeSave::save(const std::string& path) const
    {
        std::error_code ec;
        const auto folder = std::filesystem::path(path).parent_path();

        if (not folder.empty())
            std::filesystem::create_directories(folder, ec);

        if (ec)
        {
            LOG_ERROR(DOM, "Cannot create the folder of " << path << ": " << ec.message());
            return false;
        }

        Serializer serializer(false);
        serializer.setFile(path);
        serializer.serializeObject(ObjectName, *this);
        serializer.save();

        return UniversalFileAccessor::exists(path);
    }

    bool LifeSave::load(const std::string& path)
    {
        if (not UniversalFileAccessor::exists(path))
            return false;

        Serializer serializer(false);
        serializer.setFile(path);

        if (serializer.getSerializedMap().count(ObjectName) == 0)
        {
            LOG_ERROR(DOM, path << " holds no life");
            return false;
        }

        LifeSave loaded = serializer.deserializeObject<LifeSave>(ObjectName);

        if (loaded.name.empty())
        {
            LOG_ERROR(DOM, path << ": the life it holds has no name");
            return false;
        }

        *this = std::move(loaded);

        return true;
    }

    namespace
    {
        std::vector<LifeResource> skillRows()
        {
            return {
                {"skills", "", "arms", "swordsmanship", "Arms", "", "", 0, false},
                {"skills", "", "discipline", "training", "Discipline", "", "", 0, false},
                {"skills", "", "letters", "quill", "Letters", "", "", 0, false},
                {"skills", "", "lore", "study", "Lore", "", "", 0, false},
                {"skills", "", "arcana", "magic", "Arcana", "", "", 0, false},
                {"skills", "", "stealth", "dexterity", "Stealth", "", "", 0, false},
                {"skills", "", "guile", "trade", "Guile", "", "", 0, false},
                {"skills", "", "renown", "reputation", "Renown", "", "", 0, false},
            };
        }
    }

    // The life of the balance's good Warrior (res/chronicle/chronicle-balance, Warrior-good) the
    // month he swore to the Keep
    LifeSave firstLife()
    {
        LifeSave life;
        life.name = "Aldren of Bellmoor";
        life.profession = "Sworn man of the Keep at Bellmoor";
        life.origin = "Second son of the miller, born at the mill on the Bell.";
        life.aim = "warrior";
        life.age = 17.5f;
        life.world = 126;   // Ten and a half years since he was 7, when the world's count began

        life.stats = {
            {"str", 12}, {"dex", 8}, {"int", 6}, {"vit", 10}, {"vitmax", 10},
            {"arms", 5}, {"discipline", 2}, {"letters", 0}, {"lore", 0}, {"arcana", 0}, {"stealth", 0}, {"guile", 0}, {"renown", 1},
            {"watch_known", 1}, {"edric_support", 1}, {"keep_oath", 1}, {"market_known", 1},
            {"coin", 46}, {"rations", 3},
        };

        life.done = {
            {"mill", 3}, {"messages", 2}, {"carters", 13}, {"watch", 1}, {"yard", 1}, {"edric", 2}, {"keep", 1},
        };

        // He is past his first wages, knows the carters' road, and has sworn
        life.achieved = {"first.coin", "carters.road", "sworn"};

        life.parts = {"str", "dex", "int", "vit"};
        life.skills = skillRows();

        life.resources = {
            {"purse", "PURSE", "coin", "gold", "Coin", "", "", static_cast<int>(LedgerTone::Coin), false},
            {"purse", "PURSE", "rations", "trade", "Rations", "", "", 0, false},
            {"standing", "STANDING", "keep_oath", "seal", "Sworn to the Keep", "", "", static_cast<int>(LedgerTone::Guild), false},
            {"stores", "STORES", "iron", "forge", "Iron", "6", "", 0, false},
            {"kept", "KEPT BETWEEN LIVES", "relic.sunstone", "relic", "Sunstone relic", "1 of 3", "", static_cast<int>(LedgerTone::Relic), false},
        };

        life.log = {
            {7.0f, "Born to the miller's wife at Bellmoor", LogKind::Milestone, "", ""},
            {7.5f, "Help at the Mill", LogKind::Gain, "+1 str +1 vit +8 coin", "work"},
            {7.6f, "Fell in the millrace and was pulled out", LogKind::Note, "", ""},
            {8.5f, "Run Messages", LogKind::Gain, "+1 dex +5 coin", "town"},
            {10.0f, "Every carter on the Bell road knows him by name", LogKind::Milestone, "", ""},
            {10.1f, "His father went north and did not come back", LogKind::Note, "", ""},
            {11.0f, "Carry for the Watch", LogKind::Gain, "+1 arms +1 str +1 watch", "gate"},
            {11.5f, "Train at the Yard", LogKind::Gain, "+1 str +1 arms", "training"},
            {15.0f, "Train under Sergeant Edric", LogKind::Gain, "+1 arms +1 disc +1 str", "training"},
            {15.0f, "Sergeant Edric will speak for him at the Keep", LogKind::Milestone, "", "training"},
            {17.5f, "Swear Service to the Keep", LogKind::Gain, "+1 oath +1 renown +1 arms", "seal"},
            {17.5f, "Swore his service to the Keep", LogKind::Milestone, "", ""},
        };

        return life;
    }

    LifeSave freshLife(bool first)
    {
        LifeSave life;
        life.name = "Aldren of Bellmoor";
        life.profession = "The miller's second son";
        life.origin = "Born at the mill on the Bell.";
        life.aim = "warrior";
        life.age = first ? 6.0f + 11.0f / 12.0f : 7.0f;

        life.stats = {
            {"str", 6}, {"dex", 6}, {"int", 6}, {"vit", 8}, {"vitmax", 8},
            {"arms", 0}, {"discipline", 0}, {"letters", 0}, {"lore", 0}, {"arcana", 0}, {"stealth", 0}, {"guile", 0}, {"renown", 0},
            {"coin", 0}, {"rations", 12},
        };

        life.parts = {"str", "dex", "int", "vit"};
        life.skills = skillRows();

        // Nothing earned yet: the ledger shows only what he was sent off with
        life.resources = {
            {"purse", "PURSE", "rations", "trade", "Rations", "", "", 0, false},
        };

        life.log = {{life.age, "Childhood", LogKind::Milestone, "", ""}};

        // A first life holds nothing yet: no rations to eat or to run out of, no row in the
        // ledger, until its first task brings them
        if (first)
        {
            life.stats.erase("rations");
            life.resources.clear();
        }

        return life;
    }
}
