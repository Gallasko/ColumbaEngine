#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Helpers/registry.h"

#include "itemregistry.h"

struct MissionReward
{
    ItemId itemId;
    uint16_t count;
};

struct DeliveryRequirement
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
    std::string unlockFact;     // WorldFact required (empty = always available)
    std::string completionFact; // Fact to set on first completion
    std::vector<DeliveryRequirement> deliveryRequirements; // Non-empty = delivery mission

    bool isDeliveryMission() const { return !deliveryRequirements.empty(); }
};

struct MissionRegistry : public pg::Registry<MissionDef>
{
    void addMission(const MissionDef& def) { add(def); }
};

MissionRegistry createDefaultMissionRegistry();
