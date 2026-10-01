#pragma once

#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ECS/system.h"

#include "Systems/gamefacts.h"

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

    // What an achievement gives once unlocked: an event to send, or a change to a fact
    struct AchievementReward
    {
        AchievementReward() : type(AchievementRewardType::NoReward), reward(StandardEvent("Noop")), visible(false) {}
        AchievementReward(const StandardEvent& event) : type(AchievementRewardType::Event), reward(event) {}
        AchievementReward(const AddFact& fact) : type(AchievementRewardType::Add), reward(fact) {}
        AchievementReward(const RemoveFact& fact) : type(AchievementRewardType::Remove), reward(fact) {}
        AchievementReward(const IncreaseFact& fact) : type(AchievementRewardType::Increase), reward(fact) {}

        AchievementReward(const AchievementReward& other) : type(other.type), reward(other.reward), visible(other.visible) {}

        AchievementReward& operator=(const AchievementReward& other)
        {
            type = other.type;
            reward = other.reward;
            visible = other.visible;

            return *this;
        }

        inline static std::string getType() { return "AchievementReward"; }

        void call(EntitySystem* ecsRef) const;

        AchievementRewardType type;

        std::variant<StandardEvent, AddFact, RemoveFact, IncreaseFact> reward;

        bool visible = true;
    };

    template <>
    void serialize(Archive& archive, const AchievementReward& value);

    template <>
    AchievementReward deserialize(const UnserializedObject& serializedString);

    struct Achievement
    {
        inline static std::string getType() { return "Achievement"; }

        bool isComplete(const std::unordered_map<std::string, ElementType>& facts) const;

        // Sends the rewards, then the unlock event and the <name>_unlocked fact
        void setUnlocked(EntitySystem *ecsRef);

        std::string name;

        bool unlocked = false;

        bool visible = true;

        std::vector<FactChecker> prerequisiteFacts;

        std::vector<AchievementReward> rewardFacts;
    };

    template <>
    void serialize(Archive& archive, const Achievement& value);

    template <>
    Achievement deserialize(const UnserializedObject& serializedString);

    typedef std::shared_ptr<Achievement> AchievementPtr;

    /**
     * @brief Unlocks achievements as the world facts change.
     *
     * An achievement is a name, a list of FactChecker that must all hold, and a list of rewards.
     * Each locked achievement waits on the facts it still misses, so a fact update only looks at
     * the achievements that asked for that fact.
     *
     * On unlock the rewards are sent, then StandardEvent "achievementUnlocked" (value "name") and
     * the fact "<name>_unlocked".
     *
     * Only the unlocked achievements are saved: the locked ones follow from the current facts.
     */
    struct AchievementSys : public System<Listener<WorldFactsUpdate>, InitSys, SaveSys>
    {
        virtual std::string getSystemName() const override { return "AchievementSystem"; }

        virtual void init() override;

        virtual void save(Archive& archive) override;

        virtual void load(const UnserializedObject& serializedString) override;

        virtual void onEvent(const WorldFactsUpdate& event) override;

        virtual void execute() override;

        // Do not add new achievement this way at app startup, prefer use the setDefaultAchievement method instead
        void addNewAchivement(const Achievement& achievement);

        void setDefaultAchievement(const Achievement& achievement);

        // Forgets every achievement, locked or not
        void clear();

        void checkAllLockedAchievement();

        bool checkAchievementForResolve(AchievementPtr ptr, WorldFacts* facts);

        struct AchievementResolver
        {
            AchievementResolver(const std::string& name, AchievementPtr achievement);

            AchievementPtr achievement;

            const FactChecker* factChecker = nullptr;
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
