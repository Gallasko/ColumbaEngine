#pragma once

#include "Systems/basicsystems.h"

#include "missionregistry.h"
#include "depotsystem.h"
#include "playerinventory.h"
#include "worldfacts.h"
#include "saveserialization.h"

using namespace pg;

struct ActiveMission
{
    size_t defIndex;         // Index into MissionRegistry
    int depotX, depotY;     // Linked depot owner coordinates
    size_t elapsedMs = 0;
    bool completed = false;
};

class MissionSystem : public System<Listener<TickEvent>, SaveSys>
{
public:
    static constexpr size_t DEFAULT_MAX_ACTIVE = 2;
    static constexpr ItemId ROBOT_CORE_ID = 33;
    static constexpr ItemId TICKET_ID = 35;

    MissionSystem(MissionRegistry* missionRegistry, DepotSystem* depotSystem, WorldFacts* worldFacts)
        : missionRegistry(missionRegistry), depotSystem(depotSystem), worldFacts(worldFacts)
    {
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

    const std::vector<MissionDef>& getDefs() const { return missionRegistry->all(); }
    const std::vector<ActiveMission>& getActive() const { return activeMissions; }

    bool isMissionUnlocked(size_t defIndex) const;
    bool canStartMission(size_t defIndex) const;
    bool startMission(size_t defIndex, int depotX, int depotY);
    bool claimMission(size_t activeIndex);

    size_t getMaxActive() const { return maxActiveMissions; }
    void purchaseExtraSlot();
    size_t getExtraSlotCost() const { return 10; } // Tickets

    // Delivery mission helpers
    uint16_t getDeliveryCount(const ActiveMission& m, const DeliveryRequirement& req) const;
    float getDeliveryProgress(const ActiveMission& m) const;

private:
    MissionRegistry* missionRegistry = nullptr;
    DepotSystem* depotSystem = nullptr;
    WorldFacts* worldFacts = nullptr;

    std::vector<ActiveMission> activeMissions;
    size_t maxActiveMissions = DEFAULT_MAX_ACTIVE;
    size_t tickAccumulator = 0;
};
