#include "skilltree.h"

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "SkillTree7";
    }

    template <>
    void serialize(Archive& archive, const LevelProgression& value)
    {
        archive.startSerialization("LevelProgression");

        for (size_t i = 0; i < MAXLEVEL + 1; i++)
        {
            serialize(archive, "itemNeededAt" + std::to_string(i), value.neededMat[i]);
        }

        archive.endSerialization();
    }

    template <>
    LevelProgression deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing LevelProgression");

            LevelProgression data;

            for (size_t i = 0; i < MAXLEVEL + 1; i++)
            {
                defaultDeserialize(serializedString, "itemNeededAt" + std::to_string(i), data.neededMat[i]);
            }

            return data;
        }

        return LevelProgression{};
    }

    template <>
    void serialize(Archive& archive, const LevelIncrease& value)
    {
        archive.startSerialization("LevelIncrease");

        serialize(archive, "stats", value.stats);
        serialize(archive, "spells", value.learntSpells);
        serialize(archive, "passives", value.learntPassives);

        archive.endSerialization();
    }

    template <>
    LevelIncrease deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing LevelIncrease");

            LevelIncrease data;

            defaultDeserialize(serializedString, "stats", data.stats);
            defaultDeserialize(serializedString, "spells", data.learntSpells);
            defaultDeserialize(serializedString, "passives", data.learntPassives);

            return data;
        }

        return LevelIncrease{};
    }

    template <>
    void serialize(Archive& archive, const SkillTree& value)
    {
        archive.startSerialization("LevelIncrease");

        serialize(archive, "name", value.name);
        serialize(archive, "currentLevel", value.currentLevel);
        serialize(archive, "maxLevel", value.maxLevel);
        serialize(archive, "currentXp", value.currentXp);
        serialize(archive, "requiredMatForNextLevel", value.requiredMatForNextLevel);

        for (size_t i = 0; i < MAXLEVEL + 1; i++)
        {
            serialize(archive, "levelGains" + std::to_string(i), value.levelGains[i]);
        }

        archive.endSerialization();
    }

    template <>
    SkillTree deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing LevelIncrease");

            SkillTree data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "currentLevel", data.currentLevel);
            defaultDeserialize(serializedString, "maxLevel", data.maxLevel);
            defaultDeserialize(serializedString, "currentXp", data.currentXp);
            defaultDeserialize(serializedString, "requiredMatForNextLevel", data.requiredMatForNextLevel);

            for (size_t i = 0; i < MAXLEVEL + 1; i++)
            {
                defaultDeserialize(serializedString, "levelGains" + std::to_string(i), data.levelGains[i]);
            }

            return data;
        }

        return SkillTree{};
    }
}