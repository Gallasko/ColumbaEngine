#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "ECS/system.h"

#include "gamefacts.h"

namespace pg
{
    struct Achievement
    {
        inline static std::string getType() { return "Achievement"; }

        std::string name;

        bool unlocked = false;

        std::vector<FactChecker> prerequisiteFacts;

        // std::vector<Facts> rewardFacts;

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

        // Todo
        void setUnlocked(EntitySystem *ecsRef)
        {

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

                firstInit = false; 
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

                for (auto it = achievementVec.rbegin(); it != achievementVec.rend();)
                {
                    if (not it->factChecker)
                    {
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
                    achievementUnlocked.push_back(**it);
                    ++it;
                }
                else
                {
                    it = decltype(it)(achievementLocked.erase(std::next(it).base()));
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

        std::vector<AchievementPtr> achievementLocked;

        std::unordered_map<std::string, std::vector<AchievementResolver>> achievementToResolve;

        std::vector<Achievement> pendingAchievement;

        bool achievementPending = false;

        bool firstInit = true;
    };
}
