#include "achievement.h"

namespace pg
{
    template <>
    void serialize(Archive& archive, const AchievementReward& value)
    {
        archive.startSerialization(AchievementReward::getType());

        serialize(archive, "type", achievementRewardTypeToString.at(value.type));

        switch (value.type)
        {
        case AchievementRewardType::Event:
            serialize(archive, "reward", std::get<StandardEvent>(value.reward));
            break;
        
        case AchievementRewardType::Add:
            serialize(archive, "reward", std::get<AddFact>(value.reward));
            break;

        case AchievementRewardType::Remove:
            serialize(archive, "reward", std::get<RemoveFact>(value.reward));
            break;

        case AchievementRewardType::Increase:
            serialize(archive, "reward", std::get<IncreaseFact>(value.reward));
            break;

        default:
            LOG_ERROR("AchivementReward", "Trying to serialize an empty Achievement Reward");
            break;
        }

        archive.endSerialization();
    }

    template <>
    AchievementReward deserialize(const UnserializedObject& serializedString)
    {
        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("AchievementReward", "Element is null");
        }
        else
        {
            LOG_INFO("AchievementReward", "Deserializing AchievementReward");

            AchievementReward data;

            data.type = stringToAchievementRewardType.at(deserialize<std::string>(serializedString["type"]));

            switch (data.type)
            {
            case AchievementRewardType::Event:
                data.reward = deserialize<StandardEvent>(serializedString["reward"]);
                break;
            
            case AchievementRewardType::Add:
                data.reward = deserialize<AddFact>(serializedString["reward"]);
                break;

            case AchievementRewardType::Remove:
                data.reward = deserialize<RemoveFact>(serializedString["reward"]);
                break;

            case AchievementRewardType::Increase:
                data.reward = deserialize<IncreaseFact>(serializedString["reward"]);
                break;

            default:
                LOG_ERROR("AchivementReward", "Trying to serialize an empty Achievement Reward");
                break;
            }

            return data;
        }

        return AchievementReward{};
    }

    template <>
    void serialize(Archive& archive, const Achievement& value)
    {
        archive.startSerialization(Achievement::getType());

        serialize(archive, "name", value.name);
        serialize(archive, "unlocked", value.unlocked);
        serialize(archive, "prerequisiteFacts", value.prerequisiteFacts);
        serialize(archive, "rewards", value.rewardFacts);

        archive.endSerialization();
    }

    template <>
    Achievement deserialize(const UnserializedObject& serializedString)
    {
        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Achievement", "Element is null");
        }
        else
        {
            LOG_INFO("Achievement", "Deserializing Achievement");

            Achievement data;

            data.name = deserialize<std::string>(serializedString["name"]);

            defaultDeserialize(serializedString, "unlocked", data.unlocked);
            defaultDeserialize(serializedString, "prerequisiteFacts", data.prerequisiteFacts);
            defaultDeserialize(serializedString, "rewards", data.rewardFacts);

            return data;
        }

        return Achievement{};
    }
}