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

        const auto* def = missionRegistry->tryGet(m.defIndex);
        if (not def)
            continue;

        if (def->isDeliveryMission())
        {
            // Check if linked depot has all required items
            if (getDeliveryProgress(m) >= 1.0f)
            {
                if (def->repeatable)
                {
                    // Auto-claim: consume items, give rewards, restart
                    autoClaimAndRestart(m);
                }
                else
                {
                    m.completed = true;
                }
            }
        }
        else
        {
            m.elapsedMs += dt;
            if (m.elapsedMs >= def->durationMs)
            {
                if (def->repeatable)
                {
                    autoClaimAndRestart(m);
                }
                else
                {
                    m.completed = true;
                    m.elapsedMs = def->durationMs;
                }
            }
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

    const auto& def = missionRegistry->get(defIndex);
    // Block non-repeatable missions that have already been completed
    if (not def.repeatable and isMissionCompleted(defIndex))
        return false;

    return true;
}

bool MissionSystem::startMission(size_t defIndex, int depotX, int depotY)
{
    if (not canStartMission(defIndex))
        return false;

    const auto& def = missionRegistry->get(defIndex);

    // Find depot — block if depot already has an active mission
    DepotData* depot = depotSystem->getDepot(depotX, depotY);
    if (not depot)
        return false;
    if (hasActiveMissionAtDepot(depotX, depotY))
        return false;

    // Check and consume robot cores (skip if cost is 0)
    if (def.robotCoreCost > 0)
    {
        uint16_t coreCount = 0;
        for (const auto& slot : depot->inventory.slots)
        {
            if (slot.id == ROBOT_CORE_ID)
                coreCount += slot.count;
        }

        if (coreCount < def.robotCoreCost)
            return false;

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

    // For delivery missions, consume required items from depot
    DepotData* depot = depotSystem->getDepot(m.depotX, m.depotY);
    if (def.isDeliveryMission() and depot)
    {
        for (const auto& req : def.deliveryRequirements)
        {
            uint16_t remaining = req.count;
            for (auto& slot : depot->inventory.slots)
            {
                if (remaining == 0) break;
                if (slot.id == req.itemId)
                {
                    uint16_t take = std::min(slot.count, remaining);
                    slot.count -= take;
                    remaining -= take;
                    if (slot.count == 0)
                        slot.clear();
                }
            }
        }
    }

    // Distribute rewards: tickets go to player, other items to depot output
    for (const auto& reward : def.rewards)
    {
        if (reward.itemId == TICKET_ID)
        {
            // Tickets always go to player inventory as currency
            sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});
            continue;
        }

        if (not depot)
        {
            // Depot was removed — give directly to player
            sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});
            continue;
        }

        // Non-ticket rewards go to depot output slots
        uint16_t left = reward.count;
        for (auto& slot : depot->output.slots)
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

    // Set completion fact for tier gating
    if (not def.completionFact.empty() and worldFacts)
        worldFacts->setFact(def.completionFact, true);

    printf("MissionSystem: claimed '%s'\n", def.name.c_str());

    // Remove from active list
    activeMissions.erase(activeMissions.begin() + static_cast<ptrdiff_t>(activeIndex));
    return true;
}

void MissionSystem::autoClaimAndRestart(ActiveMission& m)
{
    const auto& def = missionRegistry->get(m.defIndex);

    // Consume delivered items from depot
    DepotData* depot = depotSystem->getDepot(m.depotX, m.depotY);
    if (def.isDeliveryMission() and depot)
    {
        for (const auto& req : def.deliveryRequirements)
        {
            uint16_t remaining = req.count;
            for (auto& slot : depot->inventory.slots)
            {
                if (remaining == 0) break;
                if (slot.id == req.itemId)
                {
                    uint16_t take = std::min(slot.count, remaining);
                    slot.count -= take;
                    remaining -= take;
                    if (slot.count == 0)
                        slot.clear();
                }
            }
        }
    }

    // Distribute rewards
    for (const auto& reward : def.rewards)
    {
        if (reward.itemId == TICKET_ID)
        {
            sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});
            continue;
        }

        if (not depot)
        {
            sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});
            continue;
        }

        uint16_t left = reward.count;
        for (auto& slot : depot->output.slots)
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

    // Set completion fact
    if (not def.completionFact.empty() and worldFacts)
        worldFacts->setFact(def.completionFact, true);

    // Reset mission for next cycle
    m.elapsedMs = 0;
    m.completed = false;

    printf("MissionSystem: auto-claimed '%s' (repeatable)\n", def.name.c_str());
}

void MissionSystem::purchaseExtraSlot()
{
    maxActiveMissions++;
    printf("MissionSystem: max active missions increased to %zu\n", maxActiveMissions);
}

uint16_t MissionSystem::getDeliveryCount(const ActiveMission& m, const DeliveryRequirement& req) const
{
    DepotData* depot = depotSystem->getDepot(m.depotX, m.depotY);
    if (not depot)
        return 0;

    uint16_t count = 0;
    for (const auto& slot : depot->inventory.slots)
    {
        if (slot.id == req.itemId)
            count += slot.count;
    }
    return std::min(count, req.count);
}

float MissionSystem::getDeliveryProgress(const ActiveMission& m) const
{
    const auto* def = missionRegistry->tryGet(m.defIndex);
    if (not def or not def->isDeliveryMission())
        return 0.0f;

    uint16_t totalRequired = 0;
    uint16_t totalDelivered = 0;
    for (const auto& req : def->deliveryRequirements)
    {
        totalRequired += req.count;
        totalDelivered += getDeliveryCount(m, req);
    }

    if (totalRequired == 0)
        return 1.0f;

    return static_cast<float>(totalDelivered) / static_cast<float>(totalRequired);
}

bool MissionSystem::isMissionCompleted(size_t defIndex) const
{
    if (defIndex >= missionRegistry->count())
        return false;
    const auto& def = missionRegistry->get(defIndex);
    if (def.completionFact.empty())
        return false;
    return worldFacts and worldFacts->getFact<bool>(def.completionFact);
}

bool MissionSystem::hasActiveMissionAtDepot(int x, int y) const
{
    for (const auto& m : activeMissions)
    {
        if (m.depotX == x and m.depotY == y)
            return true;
    }
    return false;
}

const ActiveMission* MissionSystem::getActiveMissionForDepot(int x, int y) const
{
    for (const auto& m : activeMissions)
    {
        if (m.depotX == x and m.depotY == y)
            return &m;
    }
    return nullptr;
}

size_t MissionSystem::getActiveMissionIndexForDepot(int x, int y) const
{
    for (size_t i = 0; i < activeMissions.size(); ++i)
    {
        if (activeMissions[i].depotX == x and activeMissions[i].depotY == y)
            return i;
    }
    return SIZE_MAX;
}
