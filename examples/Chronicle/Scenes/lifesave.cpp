#include "lifesave.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
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

    // A table of a walk, as the rules hand it: its scalars by name. A list of them is a list of these
    template <>
    ElementMap deserialize(const UnserializedObject& serialized)
    {
        if (serialized.isNull())
            return ElementMap{};

        return deserializeUnorderedMap<std::string, ElementType>(serialized);
    }

    template <>
    void serialize(Archive& archive, const chronicle::WalkState& value)
    {
        archive.startSerialization("WalkState");

        serialize(archive, "run", value.run);
        serialize(archive, "record", value.record);
        serialize(archive, "marks", value.marks);
        serialize(archive, "fight", value.fight);
        serialize(archive, "foes", value.foes);
        serialize(archive, "account", value.account);

        archive.endSerialization();
    }

    template <>
    chronicle::WalkState deserialize(const UnserializedObject& serialized)
    {
        chronicle::WalkState data;

        if (serialized.isNull())
            return data;

        defaultDeserialize(serialized, "run", data.run);
        defaultDeserialize(serialized, "record", data.record);
        defaultDeserialize(serialized, "marks", data.marks);
        defaultDeserialize(serialized, "fight", data.fight);
        defaultDeserialize(serialized, "foes", data.foes);
        defaultDeserialize(serialized, "account", data.account);

        return data;
    }

    template <>
    void serialize(Archive& archive, const chronicle::WalkRecipe& value)
    {
        archive.startSerialization("WalkRecipe");

        serialize(archive, "id", value.id);
        serialize(archive, "fields", value.fields);
        serialize(archive, "steps", value.steps);
        serialize(archive, "marks", value.marks);

        archive.endSerialization();
    }

    template <>
    chronicle::WalkRecipe deserialize(const UnserializedObject& serialized)
    {
        chronicle::WalkRecipe data;

        if (serialized.isNull())
            return data;

        defaultDeserialize(serialized, "id", data.id);
        defaultDeserialize(serialized, "fields", data.fields);
        defaultDeserialize(serialized, "steps", data.steps);
        defaultDeserialize(serialized, "marks", data.marks);

        return data;
    }

    template <>
    void serialize(Archive& archive, const chronicle::WalkQueue& value)
    {
        archive.startSerialization("WalkQueue");

        serialize(archive, "recipe", value.recipe);
        serialize(archive, "count", value.count);
        serialize(archive, "left", value.left);
        serialize(archive, "total", value.total);
        serialize(archive, "stop", value.stop);
        serialize(archive, "value", value.value);
        serialize(archive, "item", value.item);

        archive.endSerialization();
    }

    template <>
    chronicle::WalkQueue deserialize(const UnserializedObject& serialized)
    {
        chronicle::WalkQueue data;

        if (serialized.isNull())
            return data;

        defaultDeserialize(serialized, "recipe", data.recipe);
        defaultDeserialize(serialized, "count", data.count);
        defaultDeserialize(serialized, "left", data.left);
        defaultDeserialize(serialized, "total", data.total);
        defaultDeserialize(serialized, "stop", data.stop);
        defaultDeserialize(serialized, "value", data.value);
        defaultDeserialize(serialized, "item", data.item);

        return data;
    }

    template <>
    void serialize(Archive& archive, const chronicle::LifeSave& value)
    {
        archive.startSerialization("LifeSave");

        // Always the format this build writes: what was read is written again in it
        serialize(archive, "version", chronicle::LifeSave::SaveVersion);
        serialize(archive, "name", value.name);
        serialize(archive, "profession", value.profession);
        serialize(archive, "origin", value.origin);
        serialize(archive, "aim", value.aim);
        serialize(archive, "age", value.age);
        serialize(archive, "world", value.world);
        serialize(archive, "lives", value.lives);
        serialize(archive, "savedAt", value.savedAt);
        serialize(archive, "townKnown", value.townKnown);
        serialize(archive, "town", value.town);
        serialize(archive, "raisedBy", value.raisedBy);
        serialize(archive, "fund", value.fund);
        serialize(archive, "seen", value.seen);
        serialize(archive, "told", value.told);
        serialize(archive, "knowledge", value.knowledge);
        serialize(archive, "stash", value.stash);
        serialize(archive, "walk", value.walk);
        serialize(archive, "recipes", value.recipes);
        serialize(archive, "queue", value.queue);
        serialize(archive, "walking", value.walking);
        serialize(archive, "lastWalk", value.lastWalk);
        serialize(archive, "recorded", value.recorded);
        serialize(archive, "walksByHand", value.walksByHand);
        serialize(archive, "walksByRecord", value.walksByRecord);
        serialize(archive, "walkedAt", value.walkedAt);
        serialize(archive, "replayedAt", value.replayedAt);
        serialize(archive, "carried", value.carried);
        serialize(archive, "queued", value.queued);
        serialize(archive, "fights", value.fights);
        serialize(archive, "fightsWon", value.fightsWon);
        serialize(archive, "fightsLost", value.fightsLost);
        serialize(archive, "fightsFled", value.fightsFled);
        serialize(archive, "fightsAuto", value.fightsAuto);
        serialize(archive, "vitalityIn", value.vitalityIn);
        serialize(archive, "running", value.running);
        serialize(archive, "monthsIn", value.monthsIn);
        serialize(archive, "setAside", value.setAside);
        serialize(archive, "setAsideMonths", value.setAsideMonths);
        serialize(archive, "boardUpFront", value.boardUpFront);
        serialize(archive, "coursesTaught", value.coursesTaught);
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

        // A save from before the field has none
        data.version = 0;
        defaultDeserialize(serialized, "version", data.version);

        defaultDeserialize(serialized, "name", data.name);
        defaultDeserialize(serialized, "profession", data.profession);
        defaultDeserialize(serialized, "origin", data.origin);
        defaultDeserialize(serialized, "aim", data.aim);
        defaultDeserialize(serialized, "age", data.age);
        defaultDeserialize(serialized, "world", data.world);
        defaultDeserialize(serialized, "lives", data.lives);
        defaultDeserialize(serialized, "savedAt", data.savedAt);
        defaultDeserialize(serialized, "townKnown", data.townKnown);
        defaultDeserialize(serialized, "town", data.town);
        defaultDeserialize(serialized, "raisedBy", data.raisedBy);
        defaultDeserialize(serialized, "fund", data.fund);
        defaultDeserialize(serialized, "seen", data.seen);
        defaultDeserialize(serialized, "told", data.told);
        defaultDeserialize(serialized, "knowledge", data.knowledge);
        defaultDeserialize(serialized, "stash", data.stash);
        defaultDeserialize(serialized, "walk", data.walk);
        defaultDeserialize(serialized, "recipes", data.recipes);
        defaultDeserialize(serialized, "queue", data.queue);
        defaultDeserialize(serialized, "walking", data.walking);
        defaultDeserialize(serialized, "lastWalk", data.lastWalk);
        defaultDeserialize(serialized, "recorded", data.recorded);
        defaultDeserialize(serialized, "walksByHand", data.walksByHand);
        defaultDeserialize(serialized, "walksByRecord", data.walksByRecord);
        defaultDeserialize(serialized, "walkedAt", data.walkedAt);
        defaultDeserialize(serialized, "replayedAt", data.replayedAt);
        defaultDeserialize(serialized, "carried", data.carried);
        defaultDeserialize(serialized, "queued", data.queued);
        defaultDeserialize(serialized, "fights", data.fights);
        defaultDeserialize(serialized, "fightsWon", data.fightsWon);
        defaultDeserialize(serialized, "fightsLost", data.fightsLost);
        defaultDeserialize(serialized, "fightsFled", data.fightsFled);
        defaultDeserialize(serialized, "fightsAuto", data.fightsAuto);
        defaultDeserialize(serialized, "running", data.running);
        defaultDeserialize(serialized, "monthsIn", data.monthsIn);
        defaultDeserialize(serialized, "setAside", data.setAside);
        defaultDeserialize(serialized, "setAsideMonths", data.setAsideMonths);

        // A save from before the rations of a work that feeds him were given up front has no word
        // of it: its work under way never gave them (LifeScene::catchUpRations)
        data.boardUpFront = false;
        defaultDeserialize(serialized, "boardUpFront", data.boardUpFront);

        defaultDeserialize(serialized, "stats", data.stats);

        // A save of the first build of the walks kept what they brought for the town in a store of
        // the town's: it is his again, among what he holds
        std::unordered_map<std::string, int> store;
        defaultDeserialize(serialized, "store", store);

        for (const auto& [thing, amount] : store)
            data.stats[thing] += amount;

        // A save from before Vitality was counted in the points of a fight (a boy had 8, and
        // five times that on a walk): his own is brought to the same count, less the scratches
        // the walks' rounding had left him
        data.vitalityIn = 1;
        defaultDeserialize(serialized, "vitalityIn", data.vitalityIn);

        if (data.vitalityIn < chronicle::LifeSave::VitalityPoints)
        {
            const int by = chronicle::LifeSave::VitalityPoints / std::max(1, data.vitalityIn);

            if (data.stats.count("vitmax") > 0)
                data.stats["vitmax"] *= by;

            if (data.stats.count("vit") > 0)
                data.stats["vit"] = std::max(1, data.stats["vit"] * by - (data.stats.count("scratches") > 0 ? data.stats["scratches"] : 0));

            data.stats.erase("scratches");
            data.vitalityIn = chronicle::LifeSave::VitalityPoints;
        }

        // A save from before the Collegium's term was played day by day: its scholar learned
        // Firebolt with the class. He keeps it as the fire course's, unless a course is his already
        data.coursesTaught = false;
        defaultDeserialize(serialized, "coursesTaught", data.coursesTaught);

        if (not data.coursesTaught)
        {
            const auto holds = [&data](const char* stat) { auto it = data.stats.find(stat); return it != data.stats.end() and it->second > 0; };

            // A term begun as a row of months (its 35 coin paid) has no story to pass and its row
            // no gains now: what its term gave is his as he is read, and its months run out as
            // they were. So for one a life event had set aside: it is taken up again as a row
            if ((data.running == "collegium" or data.setAside == "collegium") and not holds("collegium"))
            {
                data.stats["collegium"] += 1;
                data.stats["arcana"] += 1;
                data.stats["lore"] += 1;
            }

            if (holds("collegium") and not holds("fire_course") and not holds("frost_course") and not holds("storm_course"))
                data.stats["fire_course"] = 1;

            data.coursesTaught = true;
        }

        defaultDeserialize(serialized, "parts", data.parts);
        defaultDeserialize(serialized, "skills", data.skills);
        defaultDeserialize(serialized, "resources", data.resources);
        defaultDeserialize(serialized, "log", data.log);
        defaultDeserialize(serialized, "done", data.done);

        // A save from before the outskirts: the last month of the sixth year was Start Helping Out,
        // whose row is gone. Under way, set aside or done, it is read as the outskirts' row, the
        // same month: its rations were given as he began it, and its months run out as they were
        // (no story is opened for it). Done, the outskirts are not offered him on top of it
        // (a save of 0.6 had no rations up front: its month at the mill fed him, and gave its six
        // rations at its end. It is owed them, once, and the month eats one: he is seven and fed)
        for (std::string* work : {&data.running, &data.setAside})
        {
            if (*work == "helping")
            {
                *work = "outskirts";

                if (not data.boardUpFront)
                    data.owed["rations"] += chronicle::LifeSave::HelpingRations;
            }
        }

        if (auto it = data.done.find("helping"); it != data.done.end())
        {
            const int terms = it->second;

            data.done.erase(it);
            data.done["outskirts"] += terms;
        }

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

        // How the town is named among the stats the rules read
        const std::string TownKnown = "town_known";
        const std::string TownLevel = "town.";
        const std::string TownFund = "fund.";
        const std::string TownSeen = "seen.";

        // And what else is the world's: what walks have taught
        const std::string Known = "know.";

        // And what the stash keeps between lives, of each basic
        const std::string Stashed = "stash.";

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

        // The town, after his stats: it is the world's, and a stat of the same name would not stand
        map[TownKnown] = ElementType{townKnown ? 1 : 0};

        for (const auto& [place, level] : town)
            map[TownLevel + place] = ElementType{level};

        for (const auto& [place, coin] : fund)
            map[TownFund + place] = ElementType{coin};

        for (const auto& place : seen)
            map[TownSeen + place] = ElementType{1};

        for (const auto& [key, value] : knowledge)
            map[Known + key] = ElementType{value};

        for (const auto& [id, kept] : stash)
            map[Stashed + id] = ElementType{kept};

        return map;
    }

    bool LifeSave::ofTheTown(const std::string& key)
    {
        return key == TownKnown or key.rfind(TownLevel, 0) == 0 or key.rfind(TownFund, 0) == 0 or key.rfind(TownSeen, 0) == 0 or key.rfind(Known, 0) == 0 or key.rfind(Stashed, 0) == 0;
    }

    bool LifeSave::takeTown(const std::string& key, int value)
    {
        if (not ofTheTown(key))
            return false;

        if (key == TownKnown and value > 0)
            townKnown = true;

        // What is known is what the scripts say it is: a walk teaches
        if (key.rfind(Known, 0) == 0)
            knowledge[key.substr(Known.size())] = value;

        // And what is left in the stash is what the scripts say: a draw takes from it
        if (key.rfind(Stashed, 0) == 0)
        {
            if (value > 0)
                stash[key.substr(Stashed.size())] = value;
            else
                stash.erase(key.substr(Stashed.size()));
        }

        return true;
    }

    int LifeSave::townLevels() const
    {
        int levels = 0;

        for (const auto& [place, level] : town)
            levels += level;

        return levels;
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
            auto always = row.find("always");

            // A row the rules show from the first (written time) is held at 0 too
            if (always != row.end() and always->second.type == UnionType::BOOL and always->second.get<bool>())
                stats.emplace(id, 0);
            else if (stat == stats.end() or stat->second <= 0)
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

    std::string LifeSave::digest(const std::vector<std::string>& named) const
    {
        const int terms = termsDone();

        // The deeds alone: a step of the guide or a line of lore is kept with them, under its own name
        size_t deeds = 0;

        for (const auto& id : achieved)
        {
            if (id.rfind("guide.", 0) != 0 and id.rfind("lore.", 0) != 0)
                ++deeds;
        }

        // What the world knows of the lands outside the town, added up: the places studied and
        // the beasts watched
        int known = 0;

        for (const auto& [key, level] : knowledge)
        {
            if (key.rfind("study.", 0) == 0 or key.rfind("observe.", 0) == 0)
                known += level;
        }

        char years[16];
        std::snprintf(years, sizeof(years), "%.2f", age);

        // The ids are the rules' own, plain words: nothing in them to escape
        std::ostringstream line;

        line << "{\"age\":" << years << ",\"world\":" << world << ",\"aim\":\"" << aim << "\",\"running\":\"" << running << "\",\"monthsIn\":" << monthsIn
             << ",\"terms\":" << terms << ",\"deeds\":" << deeds << ",\"log\":" << log.size() << ",\"lives\":" << lives
             << ",\"picks\":" << picks << ",\"begun\":" << begun << ",\"atOnce\":" << atOnce << ",\"skips\":" << skips << ",\"guide\":" << guide << ",\"guideSkipped\":" << guideSkipped
             << ",\"town\":" << townLevels() << ",\"townKnown\":" << (townKnown ? 1 : 0)
             << ",\"walks\":" << walksByHand << ",\"replays\":" << walksByRecord << ",\"records\":" << recipes.size() << ",\"onWalk\":" << (walk.under() ? 1 : 0)
             << ",\"fights\":" << fights << ",\"fightsWon\":" << fightsWon << ",\"fightsLost\":" << fightsLost << ",\"fightsFled\":" << fightsFled << ",\"fightsAuto\":" << fightsAuto
             << ",\"carried\":" << carried << ",\"queued\":" << queued << ",\"known\":" << known << ",\"v\":\"" << DigestBuild << "\"";

        // Place by place, the ones he went to alone: in the order of their names, so that two
        // digests of the same life read the same
        for (const auto& [zone, times] : std::map<std::string, int>(walkedAt.begin(), walkedAt.end()))
        {
            if (times > 0)
                line << ",\"walk." << zone << "\":" << times;
        }

        for (const auto& [zone, times] : std::map<std::string, int>(replayedAt.begin(), replayedAt.end()))
        {
            if (times > 0)
                line << ",\"replay." << zone << "\":" << times;
        }

        // The works followed one by one that he has done, in the order the rules list them
        for (const auto& id : named)
        {
            if (auto it = done.find(id); it != done.end() and it->second > 0)
                line << ",\"did." << id << "\":" << it->second;
        }

        line << "}";

        return line.str();
    }

    bool LifeSave::save(const std::string& path, const Settings* settings) const
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

        // What the file held is not written back: the serializer reads an object with its name and
        // would write the name a second time
        Serializer serializer(false);
        serializer.setFile(path);
        serializer.clear();
        serializer.serializeObject(ObjectName, *this);

        if (settings)
            settings->writeInto(serializer);

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
            // Burnt: the file keeps the settings alone
            LOG_INFO(DOM, path << " holds no life");
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

    LifeSave::Opened LifeSave::open(const std::string& path, LifeSave& out)
    {
        if (not UniversalFileAccessor::exists(path))
            return Opened::NotASave;

        Serializer serializer(false);
        serializer.setFile(path);

        // A file of another kind has no life in it, or not one written by the serializer of this build
        if (serializer.getSerializedMap().count(ObjectName) == 0)
            return Opened::NotASave;

        LifeSave read;

        try
        {
            read = serializer.deserializeObject<LifeSave>(ObjectName);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(DOM, path << ": " << e.what());
            return Opened::NotASave;
        }

        // Written by a build that knew the field, with what every save has: a name, his parts, a log
        if (read.version <= 0 or read.name.empty() or read.parts.empty() or read.log.empty())
            return Opened::NotASave;

        if (read.version > SaveVersion)
            return Opened::Newer;

        out = std::move(read);

        return Opened::Ok;
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
        life.townKnown = true;   // At 17 he knows Bellmoor street by street: its page is open, nothing raised yet

        life.stats = {
            {"str", 12}, {"dex", 8}, {"int", 6}, {"vit", 50}, {"vitmax", 50},
            {"arms", 5}, {"discipline", 2}, {"letters", 0}, {"lore", 0}, {"arcana", 0}, {"stealth", 0}, {"guile", 0}, {"renown", 1},
            {"watch_known", 1}, {"edric_support", 1}, {"keep_oath", 1},
            {"blade_basics", 1},   // What let him into the yard as a boy: he trains there still
            {"coin", 46}, {"rations", 3},
        };

        life.done = {
            {"mill", 3}, {"messages", 2}, {"carters", 13}, {"watch", 1}, {"yard", 1}, {"edric", 2}, {"keep", 1},
            // A boy's works with the wooden blade are behind him, and off his page: the one he
            // bought, and the edge he chose
            {"buy.blade", 1}, {"slashes", 1},
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
            {7.0f, "You were born to the miller's wife at Bellmoor", LogKind::Milestone, "", ""},
            {7.5f, "Help at the Mill", LogKind::Gain, "+1 str +1 vit +8 coin", "work"},
            {7.6f, "You fell in the millrace and were pulled out", LogKind::Note, "", ""},
            {8.5f, "Run Messages", LogKind::Gain, "+1 dex +5 coin", "town"},
            {10.0f, "Every carter on the Bell road knows you by name", LogKind::Milestone, "", ""},
            {10.1f, "Your father went north and did not come back", LogKind::Note, "", ""},
            {11.0f, "Carry for the Watch", LogKind::Gain, "+1 arms +1 str +1 watch", "gate"},
            {11.5f, "Train at the Yard", LogKind::Gain, "+1 str +1 arms", "training"},
            {15.0f, "Train under Sergeant Edric", LogKind::Gain, "+1 arms +1 disc +1 str", "training"},
            {15.0f, "Sergeant Edric will speak for you at the Keep", LogKind::Milestone, "", "training"},
            {17.5f, "Swear Service to the Keep", LogKind::Gain, "+1 oath +1 renown +1 arms", "seal"},
            {17.5f, "You swore your service to the Keep", LogKind::Milestone, "", ""},
        };

        return life;
    }

    LifeSave freshLife()
    {
        LifeSave life;
        life.name = "Aldren of Bellmoor";
        life.profession = "The miller's second son";
        life.origin = "Born at the mill on the Bell.";
        life.aim = "warrior";
        life.age = 6.0f;

        // A boy of six: his sixth year makes him what he is at seven (activitytable.pg)
        life.stats = {
            {"str", 4}, {"dex", 4}, {"int", 4}, {"vit", 35}, {"vitmax", 35},
            {"arms", 0}, {"discipline", 0}, {"letters", 0}, {"lore", 0}, {"arcana", 0}, {"stealth", 0}, {"guile", 0}, {"renown", 0},
            {"coin", 0},
        };

        life.parts = {"str", "dex", "int", "vit"};
        life.skills = skillRows();

        // He holds nothing yet: no rations to eat or to run out of, no row in the ledger, until
        // his first work brings them
        life.log = {{life.age, "Childhood", LogKind::Milestone, "", ""}};

        return life;
    }

    LifeSave boyOfSeven()
    {
        LifeSave life = freshLife();

        life.age = 7.0f;

        for (const char* part : {"str", "dex", "int"})
            life.stats[part] = 6;

        life.stats["vit"] = 40;
        life.stats["vitmax"] = 40;
        life.stats["rations"] = 12;

        life.resources = {
            {"purse", "PURSE", "rations", "trade", "Rations", "", "", 0, false},
        };

        life.log = {{life.age, "Childhood", LogKind::Milestone, "", ""}};

        return life;
    }
}
