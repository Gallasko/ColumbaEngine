#include "stdafx.h"

#include "achievement.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Achievement";
    }

    const std::unordered_map<std::string, AchievementRewardType> stringToAchievementRewardType = invertMap(achievementRewardTypeToString);

    void AchievementReward::call(EntitySystem* ecsRef) const
    {
        switch (type)
        {
        case AchievementRewardType::Event:
            ecsRef->sendEvent(std::get<StandardEvent>(reward));
            break;

        case AchievementRewardType::Add:
            ecsRef->sendEvent(std::get<AddFact>(reward));
            break;

        case AchievementRewardType::Remove:
            ecsRef->sendEvent(std::get<RemoveFact>(reward));
            break;

        case AchievementRewardType::Increase:
            ecsRef->sendEvent(std::get<IncreaseFact>(reward));
            break;

        default:
            LOG_ERROR(DOM, "Trying to call an empty reward");
            break;
        }
    }

    bool Achievement::isComplete(const std::unordered_map<std::string, ElementType>& facts) const
    {
        for (const auto& fact : prerequisiteFacts)
        {
            if (not fact.check(facts))
                return false;
        }

        return true;
    }

    void Achievement::setUnlocked(EntitySystem *ecsRef)
    {
        unlocked = true;

        for (const auto& reward : rewardFacts)
        {
            reward.call(ecsRef);
        }

        ecsRef->sendEvent(StandardEvent{AchievementUnlockEventName, "name", name});
        ecsRef->sendEvent(AddFact{name + "_unlocked", ElementType{true}});
    }

    AchievementSys::AchievementResolver::AchievementResolver(const std::string& name, AchievementPtr achievement) : achievement(achievement)
    {
        const auto& it = std::find_if(achievement->prerequisiteFacts.begin(), achievement->prerequisiteFacts.end(), [name](const FactChecker& fact) {
            return fact.name == name;
        });

        if (it != achievement->prerequisiteFacts.end())
        {
            factChecker = &*it;
        }
        else
        {
            LOG_ERROR(DOM, "Could not find fact: " << name << " in achievement: " << achievement->name);
        }
    }

    void AchievementSys::init()
    {
        firstInit = true;
    }

    void AchievementSys::save(Archive& archive)
    {
        serialize(archive, "unlockedAchievement", achievementUnlocked);
    }

    void AchievementSys::load(const UnserializedObject& serializedString)
    {
        defaultDeserialize(serializedString, "unlockedAchievement", achievementUnlocked);
    }

    void AchievementSys::addNewAchivement(const Achievement& achievement)
    {
        pendingAchievement.push_back(achievement);

        achievementPending = true;
    }

    void AchievementSys::setDefaultAchievement(const Achievement& achievement)
    {
        auto ptr = std::make_shared<Achievement>(achievement);

        achievementLocked.push_back(ptr);
    }

    void AchievementSys::clear()
    {
        achievementUnlocked.clear();
        achievementLocked.clear();
        achievementToResolve.clear();
        pendingAchievement.clear();

        while (not achievementToUnlock.empty())
        {
            achievementToUnlock.pop();
        }

        achievementPending = false;
    }

    void AchievementSys::execute()
    {
        if (not achievementPending and not firstInit)
            return;

        if (firstInit)
        {
            firstInit = false;

            // An achievement loaded as unlocked is not waited on again
            for (const auto& achievement : achievementUnlocked)
            {
                const auto& end = achievementLocked.end();
                const auto& it = std::find_if(achievementLocked.begin(), end, [&achievement](AchievementPtr ptr) {
                    return achievement.name == ptr->name;
                });

                if (it != end)
                {
                    achievementLocked.erase(it);
                }
            }

            checkAllLockedAchievement();
        }

        while (not achievementToUnlock.empty())
        {
            auto achievement = achievementToUnlock.front();

            achievement->setUnlocked(ecsRef);
            achievementUnlocked.push_back(*achievement);

            achievementToUnlock.pop();
        }

        auto worldFacts = ecsRef->getSystem<WorldFacts>();

        // Taken out first: unlocking sends events, and a handler may add achievements
        std::vector<Achievement> pending;
        pending.swap(pendingAchievement);

        achievementPending = false;

        for (const auto& achievement : pending)
        {
            auto ptr = std::make_shared<Achievement>(achievement);

            if (checkAchievementForResolve(ptr, worldFacts))
            {
                ptr->setUnlocked(ecsRef);
                achievementUnlocked.push_back(*ptr);
            }
            else
            {
                achievementLocked.push_back(ptr);
            }
        }
    }

    void AchievementSys::onEvent(const WorldFactsUpdate& event)
    {
        for (const auto& changedFact : event.changedFacts)
        {
            auto found = achievementToResolve.find(changedFact);

            if (found == achievementToResolve.end())
                continue;

            auto& achievementVec = found->second;

            for (auto it = achievementVec.rbegin(); it != achievementVec.rend();)
            {
                if (not it->factChecker)
                {
                    LOG_ERROR(DOM, "Fact checker not correctly initialized for the achievement: " << it->achievement->name);
                    it = decltype(it)(achievementVec.erase(std::next(it).base()));
                    continue;
                }

                if (not it->factChecker->check(*event.factMap))
                {
                    ++it;
                    continue;
                }

                // As this fact is done we try to see if the achievement is completed
                if (it->achievement->isComplete(*event.factMap))
                {
                    it->achievement->setUnlocked(ecsRef);

                    achievementUnlocked.push_back(*(it->achievement));

                    const auto& it2 = std::find(achievementLocked.begin(), achievementLocked.end(), it->achievement);

                    if (it2 != achievementLocked.end())
                    {
                        achievementLocked.erase(it2);
                    }
                }

                // This fact is no longer waited on
                it = decltype(it)(achievementVec.erase(std::next(it).base()));
            }
        }
    }

    void AchievementSys::checkAllLockedAchievement()
    {
        auto worldFacts = ecsRef->getSystem<WorldFacts>();

        for (auto it = achievementLocked.rbegin(); it != achievementLocked.rend();)
        {
            if (checkAchievementForResolve(*it, worldFacts))
            {
                achievementToUnlock.push(*it);

                it = decltype(it)(achievementLocked.erase(std::next(it).base()));
            }
            else
            {
                ++it;
            }
        }
    }

    bool AchievementSys::checkAchievementForResolve(AchievementPtr ptr, WorldFacts* facts)
    {
        bool unlocked = true;

        for (const auto& fact : ptr->prerequisiteFacts)
        {
            if (not fact.check(facts->factMap))
            {
                unlocked = false;

                achievementToResolve[fact.name].emplace_back(fact.name, ptr);
            }
        }

        return unlocked;
    }

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

        serialize(archive, "visible", value.visible);

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

            defaultDeserialize(serializedString, "visible", data.visible);

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
        serialize(archive, "visible", value.visible);
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
            defaultDeserialize(serializedString, "visible", data.visible);
            defaultDeserialize(serializedString, "prerequisiteFacts", data.prerequisiteFacts);
            defaultDeserialize(serializedString, "rewards", data.rewardFacts);

            return data;
        }

        return Achievement{};
    }
}