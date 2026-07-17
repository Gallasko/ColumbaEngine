#include "missionsystem.h"

using namespace pg;

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

    LOG_INFO("MissionSystem", "saved " << activeMissions.size() << " active missions");
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

    LOG_INFO("MissionSystem", "loaded " << activeMissions.size() << " active missions");
}

namespace
{
    // Distribute `count` of `id` into the depot's output slots (or to player on overflow).
    // Mirrors the old "stack into existing, then fill empty, then PlayerGainItemEvent on overflow"
    // behavior, but routed through Inventory::insert so the per-item maxStack is respected
    // instead of the hardcoded 999 cap.
    template <typename SendEventFn>
    void awardReward(DepotData* depot, ItemId id, uint16_t count,
                     const ItemRegistry& itemReg, SendEventFn&& send)
    {
        if (count == 0)
            return;
        if (not depot)
        {
            send(PlayerGainItemEvent{id, count});
            return;
        }
        uint16_t overflow = depot->output.insert(id, count, itemReg);
        if (overflow > 0)
            send(PlayerGainItemEvent{id, overflow});
    }
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

    auto* worldFacts = ecsRef->getSystem<WorldFacts>();
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
    DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(depotX, depotY);
    if (not depot)
        return false;
    if (hasActiveMissionAtDepot(depotX, depotY))
        return false;

    // Check and consume robot cores (skip if cost is 0)
    if (def.robotCoreCost > 0)
    {
        if (not depot->inventory.hasAtLeast(ROBOT_CORE_ID, def.robotCoreCost))
            return false;
        depot->inventory.remove(ROBOT_CORE_ID, def.robotCoreCost);
    }

    ActiveMission m;
    m.defIndex = defIndex;
    m.depotX = depotX;
    m.depotY = depotY;
    activeMissions.push_back(m);

    LOG_INFO("MissionSystem", "started '" << def.name << "' linked to depot at ("
            << depotX << ", " << depotY << ")");
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

    // For delivery missions, consume required items from depot (unless flagged to keep)
    DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(m.depotX, m.depotY);
    if (def.isDeliveryMission() and def.consumeItems and depot)
    {
        for (const auto& req : def.deliveryRequirements)
            depot->inventory.remove(req.itemId, req.count);
    }

    // Distribute rewards: tickets always go to player; other items to depot output (overflow → player)
    for (const auto& reward : def.rewards)
    {
        if (reward.itemId == TICKET_ID or not itemRegistry)
        {
            sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});
            continue;
        }
        awardReward(depot, reward.itemId, reward.count, *itemRegistry,
                    [this](const PlayerGainItemEvent& e) { sendEvent(e); });
    }

    // Set completion fact for tier gating
    auto* worldFacts = ecsRef->getSystem<WorldFacts>();
    if (not def.completionFact.empty() and worldFacts)
        worldFacts->setFact(def.completionFact, true);

    LOG_INFO("MissionSystem", "claimed '" << def.name << "'");

    // Remove from active list
    activeMissions.erase(activeMissions.begin() + static_cast<ptrdiff_t>(activeIndex));
    return true;
}

void MissionSystem::autoClaimAndRestart(ActiveMission& m)
{
    const auto& def = missionRegistry->get(m.defIndex);

    // Consume delivered items from depot
    DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(m.depotX, m.depotY);
    if (def.isDeliveryMission() and depot)
    {
        for (const auto& req : def.deliveryRequirements)
            depot->inventory.remove(req.itemId, req.count);
    }

    // Distribute rewards
    for (const auto& reward : def.rewards)
    {
        if (reward.itemId == TICKET_ID or not itemRegistry)
        {
            sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});
            continue;
        }
        awardReward(depot, reward.itemId, reward.count, *itemRegistry,
                    [this](const PlayerGainItemEvent& e) { sendEvent(e); });
    }

    // Set completion fact
    auto* worldFacts = ecsRef->getSystem<WorldFacts>();
    if (not def.completionFact.empty() and worldFacts)
        worldFacts->setFact(def.completionFact, true);

    // Reset mission for next cycle
    m.elapsedMs = 0;
    m.completed = false;

    LOG_INFO("MissionSystem", "auto-claimed '" << def.name << "' (repeatable)");
}

void MissionSystem::purchaseExtraSlot()
{
    maxActiveMissions++;
    LOG_INFO("MissionSystem", "max active missions increased to " << maxActiveMissions);
}

uint16_t MissionSystem::getDeliveryCount(const ActiveMission& m, const DeliveryRequirement& req) const
{
    DepotData* depot = ecsRef->getSystem<DepotSystem>()->getDepot(m.depotX, m.depotY);
    if (not depot)
        return 0;
    return std::min(depot->inventory.countItem(req.itemId), req.count);
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

bool MissionSystem::canValidateMainMission(size_t defIndex) const
{
    auto* playerInv = ecsRef->getSystem<PlayerInventorySystem>();
    if (defIndex >= missionRegistry->count() or not playerInv)
        return false;

    const auto& def = missionRegistry->get(defIndex);
    if (def.category != MissionCategory::Main)
        return false;
    if (not isMissionUnlocked(defIndex))
        return false;
    if (not def.repeatable and isMissionCompleted(defIndex))
        return false;

    for (const auto& req : def.deliveryRequirements)
    {
        if (not playerInv->hasItem(req.itemId, req.count))
            return false;
    }
    return true;
}

bool MissionSystem::validateMainMission(size_t defIndex)
{
    if (not canValidateMainMission(defIndex))
        return false;

    const auto& def = missionRegistry->get(defIndex);

    // Consume items from player inventory (unless flagged to keep)
    if (def.consumeItems)
    {
        auto* playerInv = ecsRef->getSystem<PlayerInventorySystem>();
        for (const auto& req : def.deliveryRequirements)
            playerInv->getInventory().remove(req.itemId, req.count);
    }

    // Distribute rewards
    for (const auto& reward : def.rewards)
        sendEvent(PlayerGainItemEvent{reward.itemId, reward.count});

    // Set completion fact
    auto* worldFacts = ecsRef->getSystem<WorldFacts>();
    if (not def.completionFact.empty() and worldFacts)
        worldFacts->setFact(def.completionFact, true);

    LOG_INFO("MissionSystem", "validated main mission '" << def.name << "' from player inventory");
    return true;
}

bool MissionSystem::isMissionCompleted(size_t defIndex) const
{
    if (defIndex >= missionRegistry->count())
        return false;
    const auto& def = missionRegistry->get(defIndex);
    if (def.completionFact.empty())
        return false;
    auto* worldFacts = ecsRef->getSystem<WorldFacts>();
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
