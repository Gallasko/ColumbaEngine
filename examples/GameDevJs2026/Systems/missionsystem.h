#pragma once

#include "Systems/basicsystems.h"

#include "depotsystem.h"
#include "playerinventory.h"
#include "worldfacts.h"
#include "saveserialization.h"

using namespace pg;

struct MissionReward
{
    ItemId itemId;
    uint16_t count;
};

struct MissionDef
{
    std::string name;
    std::string description;
    uint16_t robotCoreCost;
    size_t durationMs;
    std::vector<MissionReward> rewards;
    std::string unlockFact; // WorldFact required (empty = always available)
    std::string completionFact; // Fact to set on first completion
};

struct ActiveMission
{
    size_t defIndex;         // Index into missionDefs
    int depotX, depotY;     // Linked depot owner coordinates
    size_t elapsedMs = 0;
    bool completed = false;
};

class MissionSystem : public System<Listener<TickEvent>, SaveSys>
{
public:
    static constexpr size_t DEFAULT_MAX_ACTIVE = 2;
    static constexpr ItemId ROBOT_CORE_ID = 33;

    MissionSystem(DepotSystem* depotSystem, WorldFacts* worldFacts)
        : depotSystem(depotSystem), worldFacts(worldFacts)
    {
        buildMissionDefs();
    }

    virtual std::string getSystemName() const override { return "Mission System"; }

    // SaveSys
    virtual void save(Archive& archive) override;
    virtual void load(const UnserializedObject& serializedString) override;

    virtual void onEvent(const TickEvent& event) override
    {
        tickAccumulator += static_cast<size_t>(event.tick);
    }

    void execute() override;

    // --- Public API (called by MissionUI) ---

    const std::vector<MissionDef>& getDefs() const { return missionDefs; }
    const std::vector<ActiveMission>& getActive() const { return activeMissions; }

    bool isMissionUnlocked(size_t defIndex) const;
    bool canStartMission(size_t defIndex) const;
    bool startMission(size_t defIndex, int depotX, int depotY);
    bool claimMission(size_t activeIndex);

    size_t getMaxActive() const { return maxActiveMissions; }
    void purchaseExtraSlot();
    size_t getExtraSlotCost() const { return 10; } // Tickets

private:
    void buildMissionDefs();

    DepotSystem* depotSystem = nullptr;
    WorldFacts* worldFacts = nullptr;

    std::vector<MissionDef> missionDefs;
    std::vector<ActiveMission> activeMissions;
    size_t maxActiveMissions = DEFAULT_MAX_ACTIVE;
    size_t tickAccumulator = 0;
};
