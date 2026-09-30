#include "lifesave.h"

#include <filesystem>

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
        serialize(archive, "running", value.running);
        serialize(archive, "monthsIn", value.monthsIn);
        serialize(archive, "stats", value.stats);
        serialize(archive, "parts", value.parts);
        serialize(archive, "skills", value.skills);
        serialize(archive, "resources", value.resources);
        serialize(archive, "log", value.log);

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
        defaultDeserialize(serialized, "running", data.running);
        defaultDeserialize(serialized, "monthsIn", data.monthsIn);
        defaultDeserialize(serialized, "stats", data.stats);
        defaultDeserialize(serialized, "parts", data.parts);
        defaultDeserialize(serialized, "skills", data.skills);
        defaultDeserialize(serialized, "resources", data.resources);
        defaultDeserialize(serialized, "log", data.log);

        return data;
    }
}

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Save";

        constexpr const char * const ObjectName = "life";
    }

    ElementMap LifeSave::character() const
    {
        ElementMap map;

        for (const auto& [key, value] : stats)
            map[key] = ElementType{value};

        return map;
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
                {"skills", "", "swd", "swordsmanship", "Swordsmanship", "", "", 0, false},
                {"skills", "", "ride", "work", "Riding", "", "", 0, false},
                {"skills", "", "letters", "quill", "Letters", "", "", 0, false},
                {"skills", "", "haggle", "trade", "Haggling", "", "", 0, false},
            };
        }
    }

    LifeSave firstLife()
    {
        LifeSave life;
        life.name = "Aldren of Bellmoor";
        life.profession = "Apprentice at the Bellmoor Guild";
        life.origin = "Second son of the miller, born at the mill on the Bell.";
        life.aim = "squire";
        life.age = 17.5f;

        life.stats = {
            {"str", 15}, {"dex", 11}, {"int", 9}, {"vit", 12},
            {"swd", 3}, {"ride", 1}, {"letters", 2}, {"haggle", 1},
            {"letter", 0}, {"coin", 412},
        };

        life.parts = {"str", "dex", "int", "vit"};
        life.skills = skillRows();

        life.resources = {
            {"purse", "PURSE", "coin", "gold", "Coin", "", "+2 / mo", static_cast<int>(LedgerTone::Coin), false},
            {"purse", "PURSE", "rations", "trade", "Rations", "18", "\xE2\x88\x92" "1 / mo", 0, false},
            {"standing", "STANDING", "guild.bellmoor", "guild", "Bellmoor Guild", "3", "", static_cast<int>(LedgerTone::Guild), false},
            {"standing", "STANDING", "guild.academy", "academy", "The Academy", "1", "", static_cast<int>(LedgerTone::Guild), true},
            {"stores", "STORES", "iron", "forge", "Iron", "6", "", 0, false},
            {"kept", "KEPT BETWEEN LIVES", "relic.sunstone", "relic", "Sunstone relic", "1 of 3", "", static_cast<int>(LedgerTone::Relic), false},
        };

        const std::string minus = "\xE2\x88\x92";

        life.log = {
            {7.0f, "Born to the miller's wife at Bellmoor", LogKind::Milestone, "", ""},
            {7.6f, "Fell in the millrace and was pulled out", LogKind::Note, "", ""},
            {8.2f, "Carried sacks for the mill", LogKind::Gain, "+1 str", ""},
            {9.3f, "Learned his letters from the priest", LogKind::Gain, "+1 let", ""},
            {9.8f, "A fever that took the winter", LogKind::Loss, minus + "2 vit", ""},
            {10.1f, "His father went north and did not come back", LogKind::Note, "", ""},
            {10.7f, "Sold the old mule", LogKind::Coin, "+9", ""},
            {12.2f, "Taken on by the Bellmoor Guild", LogKind::Milestone, "", ""},
            {13.5f, "Wages from the guild", LogKind::Coin, "+15", ""},
            {14.0f, "Apprenticeship", LogKind::Milestone, "", ""},
            {14.3f, "Gored in the North Forest", LogKind::Loss, minus + "9 vit", ""},
            {14.9f, "Walked again by midsummer", LogKind::Gain, "+3 vit", ""},
            {16.1f, "Trained at the yard", LogKind::Gain, "+2 str", "training"},
            {17.2f, "Sparred with the guard at the gate", LogKind::Gain, "+1 swd", "swordsmanship"},
        };

        return life;
    }

    LifeSave freshLife()
    {
        LifeSave life;
        life.name = "Aldren of Bellmoor";
        life.profession = "The miller's second son";
        life.origin = "Born at the mill on the Bell.";
        life.aim = "squire";
        life.age = 7.0f;

        life.stats = {
            {"str", 6}, {"dex", 6}, {"int", 6}, {"vit", 8},
            {"swd", 0}, {"ride", 0}, {"letters", 0}, {"haggle", 0},
            {"letter", 0}, {"coin", 0},
        };

        life.parts = {"str", "dex", "int", "vit"};
        life.skills = skillRows();

        // Nothing earned yet: the ledger shows nothing
        life.log = {{7.0f, "Childhood", LogKind::Milestone, "", ""}};

        return life;
    }
}
