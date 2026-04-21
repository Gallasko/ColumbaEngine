#include "missionsystem.h"

#include <cstdio>

void MissionSystem::save(Archive& archive)
{
    serialize(archive, "maxActiveMissions", maxActiveMissions);
    serialize(archive, "activeMissionCount", activeMissions.size());

    for (size_t i = 0; i < activeMissions.size(); ++i)
    {
        std::string prefix = "mission_" + std::to_string(i) + "_";
        serialize(archive, prefix + "defIndex", activeMissions[i].defIndex);
        serialize(archive, prefix + "depotX", activeMissions[i].depotX);
        serialize(archive, prefix + "depotY", activeMissions[i].depotY);
        serialize(archive, prefix + "elapsedMs", activeMissions[i].elapsedMs);
        serialize(archive, prefix + "completed", activeMissions[i].completed);
    }

    printf("MissionSystem: saved %zu active missions\n", activeMissions.size());
}

void MissionSystem::load(const UnserializedObject& serializedString)
{
    defaultDeserialize(serializedString, "maxActiveMissions", maxActiveMissions);

    size_t count = 0;
    defaultDeserialize(serializedString, "activeMissionCount", count);

    activeMissions.clear();
    for (size_t i = 0; i < count; ++i)
    {
        std::string prefix = "mission_" + std::to_string(i) + "_";
        ActiveMission m;
        defaultDeserialize(serializedString, prefix + "defIndex", m.defIndex);
        defaultDeserialize(serializedString, prefix + "depotX", m.depotX);
        defaultDeserialize(serializedString, prefix + "depotY", m.depotY);
        defaultDeserialize(serializedString, prefix + "elapsedMs", m.elapsedMs);
        defaultDeserialize(serializedString, prefix + "completed", m.completed);

        if (m.defIndex < missionRegistry->count())
            activeMissions.push_back(m);
    }

    printf("MissionSystem: loaded %zu active missions\n", activeMissions.size());
}

void MissionSystem::execute()
{
    if (tickAccumulator == 0)
        return;

    size_t dt = tickAccumulator;
    tickAccumulator = 0;

    for (auto& m : activeMissions)
    {
        if (m.completed)
            continue;

        m.elapsedMs += dt;

        const auto* def = missionRegistry->tryGet(m.defIndex);
        if (def and m.elapsedMs >= def->durationMs)
        {
            m.completed = true;
            m.elapsedMs = def->durationMs;
        }
    }
}

bool MissionSystem::isMissionUnlocked(size_t defIndex) const
{
    if (defIndex >= missionRegistry->count())
        return false;

    const auto& def = missionRegistry->get(defIndex);
    if (def.unlockFact.empty())
        return true;

    return worldFacts and worldFacts->getFact<bool>(def.unlockFact);
}

bool MissionSystem::canStartMission(size_t defIndex) const
{
    if (not isMissionUnlocked(defIndex))
        return false;
    if (activeMissions.size() >= maxActiveMissions)
        return false;
    return true;
}

bool MissionSystem::startMission(size_t defIndex, int depotX, int depotY)
{
    if (not canStartMission(defIndex))
        return false;

    const auto& def = missionRegistry->get(defIndex);

    // Find depot and check for robot cores
    DepotData* depot = depotSystem->getDepot(depotX, depotY);
    if (not depot)
        return false;

    // Count robot cores in depot
    uint16_t coreCount = 0;
    for (const auto& slot : depot->inventory.slots)
    {
        if (slot.id == ROBOT_CORE_ID)
            coreCount += slot.count;
    }

    if (coreCount < def.robotCoreCost)
        return false;

    // Consume robot cores
    uint16_t remaining = def.robotCoreCost;
    for (auto& slot : depot->inventory.slots)
    {
        if (slot.id == ROBOT_CORE_ID and remaining > 0)
        {
            uint16_t take = std::min(slot.count, remaining);
            slot.count -= take;
            remaining -= take;
            if (slot.count == 0)
                slot.clear();
        }
    }

    ActiveMission m;
    m.defIndex = defIndex;
    m.depotX = depotX;
    m.depotY = depotY;
    activeMissions.push_back(m);

    printf("MissionSystem: started '%s' linked to depot at (%d, %d)\n",
           def.name.c_str(), depotX, depotY);
    return true;
}

bool MissionSystem::claimMission(size_t activeIndex)
{
    if (activeIndex >= activeMissions.size())
        return false;

    auto& m = activeMissions[activeIndex];
    if (not m.completed)
        return false;

    const auto& def = missionRegistry->get(m.defIndex);

    // Deposit rewards into linked depot
    DepotData* depot = depotSystem->getDepot(m.depotX, m.depotY);
    if (not depot)
    {
        // Depot was removed — give rewards directly to player
        for (const auto& reward : def.rewards)
            sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});
    }
    else
    {
        for (const auto& reward : def.rewards)
        {
            // Try to insert into depot; overflow goes to player
            uint16_t left = reward.count;
            for (auto& slot : depot->inventory.slots)
            {
                if (left == 0) break;
                if (slot.isEmpty())
                {
                    slot.id = reward.itemId;
                    slot.count = left;
                    left = 0;
                }
                else if (slot.id == reward.itemId and slot.count < 999)
                {
                    uint16_t space = 999 - slot.count;
                    uint16_t add = std::min(left, space);
                    slot.count += add;
                    left -= add;
                }
            }
            if (left > 0)
                sendEvent(PlayerGainItemEvent{reward.itemId, left});
        }
    }

    // Set completion fact for tier gating
    if (not def.completionFact.empty() and worldFacts)
        worldFacts->setFact(def.completionFact, true);

    printf("MissionSystem: claimed '%s'\n", def.name.c_str());

    // Remove from active list
    activeMissions.erase(activeMissions.begin() + static_cast<ptrdiff_t>(activeIndex));
    return true;
}

void MissionSystem::purchaseExtraSlot()
{
    maxActiveMissions++;
    printf("MissionSystem: max active missions increased to %zu\n", maxActiveMissions);
}
