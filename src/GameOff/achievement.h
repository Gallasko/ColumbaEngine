#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "ECS/system.h"

#include "gamefacts.h"

namespace pg
{
    enum class AchievementRewardType : uint8_t
    {
        NoReward = 0,
        Event,
        Add,
        Remove,
        Increase
    };

    const static std::unordered_map<AchievementRewardType, std::string> achievementRewardTypeToString = {
        {AchievementRewardType::NoReward, "NoReward"},
        {AchievementRewardType::Event, "Event"},
        {AchievementRewardType::Add, "Add"},
        {AchievementRewardType::Remove, "Remove"},
        {AchievementRewardType::Increase, "Increase"},
    };

    const static auto stringToAchievementRewardType = invertMap(achievementRewardTypeToString);

    static const std::string AchievementUnlockEventName = "achievementUnlocked";

    struct AchievementReward
    {
        AchievementReward() : type(AchievementRewardType::NoReward), reward(StandardEvent("Noop")) { }
        AchievementReward(const StandardEvent& event) : type(AchievementRewardType::Event), reward(event) {}
        AchievementReward(const AddFact& fact) : type(AchievementRewardType::Add), reward(fact) {}
        AchievementReward(const RemoveFact& fact) : type(AchievementRewardType::Remove), reward(fact) {}
        AchievementReward(const IncreaseFact& fact) : type(AchievementRewardType::Increase), reward(fact) {}

        AchievementReward(const AchievementReward& other) : type(other.type), reward(other.reward) {}

        AchievementReward& operator=(const AchievementReward& other)
        {
            type = other.type;
            reward = other.reward;

            return *this;
        }

        inline static std::string getType() { return "AchievementReward"; }

        void call(EntitySystem* ecsRef) const
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
                LOG_ERROR("AchivementReward", "Trying to call an empty reward");
                break;
            }
        }

        AchievementRewardType type;
        std::variant<StandardEvent, AddFact, RemoveFact, IncreaseFact> reward;
    };

    template <>
    void serialize(Archive& archive, const AchievementReward& value);

    template <>
    AchievementReward deserialize(const UnserializedObject& serializedString);

    struct Achievement
    {
        inline static std::string getType() { return "Achievement"; }

        std::string name;

        bool unlocked = false;

        std::vector<FactChecker> prerequisiteFacts;

        std::vector<AchievementReward> rewardFacts;

        bool isComplete(const std::unordered_map<std::string, ElementType>& facts)
        {
            bool allPrerequisiteDone = true;

            for (const auto& fact : prerequisiteFacts)
            {
                if (not fact.check(facts))
                {
                    allPrerequisiteDone = false;
                    break;
                }
            }

            return allPrerequisiteDone;
        }

        void setUnlocked(EntitySystem *ecsRef)
        {
            unlocked = true;

            for (const auto& reward : rewardFacts)
            {
                reward.call(ecsRef);
            }

            ecsRef->sendEvent(StandardEvent{AchievementUnlockEventName, "name", name});
            ecsRef->sendEvent(AddFact{name + "_unlocked", ElementType{true}});
        }
    };

    template <>
    void serialize(Archive& archive, const Achievement& value);

    template <>
    Achievement deserialize(const UnserializedObject& serializedString);

    typedef std::shared_ptr<Achievement> AchievementPtr;

    struct AchievementSys : public System<Listener<WorldFactsUpdate>, InitSys, SaveSys>
    {
        virtual std::string getSystemName() const override { return "AchievementSystem"; }

        // Do not add new achievement this way at app startup, prefer use the setDefaultAchievement method instead
        void addNewAchivement(const Achievement& achievement)
        {
            pendingAchievement.push_back(achievement);

            achievementPending = true;
        }

        void setDefaultAchievement(const Achievement& achievement)
        {
            auto ptr = std::make_shared<Achievement>(achievement);

            achievementLocked.push_back(ptr);
        }

        virtual void init() override
        {
            firstInit = true;
        }

        // Only save and load unlocked achievement !
        // Saving locked achievement is useless as they are only controled by the current world facts
        virtual void save(Archive& archive) override
        {
            serialize(archive, "unlockedAchievement", achievementUnlocked);
        }

        virtual void load(const UnserializedObject& serializedString) override
        {
            defaultDeserialize(serializedString, "unlockedAchievement", achievementUnlocked);
        }

        virtual void execute() override
        {
            if (not achievementPending and not firstInit)
                return;

            if (firstInit)
            {
                firstInit = false;

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
                auto achi = achievementToUnlock.front();
                
                achi->setUnlocked(ecsRef);
                achievementUnlocked.push_back(*achi);
                
                achievementToUnlock.pop();
            }

            auto worldFacts = ecsRef->getSystem<WorldFacts>();

            for (const auto& achievement : pendingAchievement)
            {
                auto ptr = std::make_shared<Achievement>(achievement);

                bool unlocked = checkAchievementForResolve(ptr, worldFacts);
                
                if (unlocked)
                {
                    achievementUnlocked.push_back(achievement);
                }
                else
                {
                    achievementLocked.push_back(ptr);
                }
            }

            pendingAchievement.clear();
            firstInit = false;
        }

        virtual void onEvent(const WorldFactsUpdate& event)
        {
            for (const auto& changedFact : event.changedFacts)
            {
                auto& achievementVec = achievementToResolve[changedFact];

                auto end = achievementVec.rend();

                for (auto it = achievementVec.rbegin(); it != end;)
                {

                    if (not it->factChecker)
                    {
                        LOG_ERROR("Achievement", "Fact checker not correctly initialized for the achievement: " << it->achievement->name);
                        it = decltype(it)(achievementVec.erase(std::next(it).base()));
                        continue;
                    }

                    if (it->factChecker->check(*event.factMap))
                    {
                        // As this fact is done we try to see if the achievement is completed
                        if (it->achievement->isComplete(*event.factMap))
                        {
                            // If the achievement is completed we set it as completed and push it to the unlocked list
                            it->achievement->setUnlocked(ecsRef);

                            achievementUnlocked.push_back(*(it->achievement));

                            // Erase achievement from the locked list
                            const auto& it2 = std::find(achievementLocked.begin(), achievementLocked.end(), it->achievement);

                            if (it2 != achievementLocked.end())
                            {
                                achievementLocked.erase(it2);
                            }
                        }

                        // We erase the it from the achievement to resolve
                        it = decltype(it)(achievementVec.erase(std::next(it).base()));
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }

        void checkAllLockedAchievement()
        {
            auto worldFacts = ecsRef->getSystem<WorldFacts>();

            const auto& end = achievementLocked.rend();

            for (auto it = achievementLocked.rbegin(); it != end;)
            {
                bool unlocked = checkAchievementForResolve(*it, worldFacts);
                
                if (unlocked)
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

        bool checkAchievementForResolve(AchievementPtr ptr, WorldFacts* facts)
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

        struct AchievementResolver
        {
            AchievementResolver(const std::string& name, AchievementPtr achievement) : achievement(achievement)
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
                    LOG_ERROR("Achievement Resolver", "Could find fact: " << name << " in achivement: " << achievement->name);
                    factChecker = nullptr;
                }
            }

            AchievementResolver(const AchievementResolver& other) : achievement(other.achievement), factChecker(other.factChecker)
            {

            }

            AchievementResolver& operator=(const AchievementResolver& other)
            {
                achievement = other.achievement;
                factChecker = other.factChecker;

                return *this;
            }

            AchievementPtr achievement;

            const FactChecker* factChecker;
        };

        std::vector<Achievement> achievementUnlocked;
        std::queue<AchievementPtr> achievementToUnlock;

        std::vector<AchievementPtr> achievementLocked;

        std::unordered_map<std::string, std::vector<AchievementResolver>> achievementToResolve;

        std::vector<Achievement> pendingAchievement;

        bool achievementPending = false;

        bool firstInit = true;
    };
}
