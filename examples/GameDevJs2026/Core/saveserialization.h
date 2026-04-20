#pragma once

#include "serialization.h"
#include "itemregistry.h"
#include "inventory.h"
#include "terrain.h"
#include "grid.h"

#include <array>

using namespace pg;

// --- Saved building entry (for GridSystem save/load) ---

struct SavedBuilding
{
    int x = 0, y = 0;
    uint16_t tileId = 0;
    uint8_t direction = 0;
    uint8_t enterDirection = 0;
    size_t conveyorTileIndex = 0;

    inline static std::string getType() { return "SavedBuilding"; }
};

// --- Forward declarations for MachineData, MinerData, InserterData ---
// These are declared in their respective system headers; we only need
// serialize/deserialize declarations here so the engine can find them.

struct MachineData;
struct MinerData;
struct InserterData;
struct StorageData;
enum class InserterState : uint8_t;

namespace pg
{
    // ItemStack
    template <> void serialize(Archive& archive, const ItemStack& value);
    template <> ItemStack deserialize(const UnserializedObject& s);

    // Inventory
    template <> void serialize(Archive& archive, const Inventory& value);
    template <> Inventory deserialize(const UnserializedObject& s);

    // SavedBuilding
    template <> void serialize(Archive& archive, const SavedBuilding& value);
    template <> SavedBuilding deserialize(const UnserializedObject& s);

    // MachineData
    template <> void serialize(Archive& archive, const MachineData& value);
    template <> MachineData deserialize(const UnserializedObject& s);

    // MinerData (save-only subset)
    template <> void serialize(Archive& archive, const MinerData& value);
    template <> MinerData deserialize(const UnserializedObject& s);

    // InserterData (save-only subset)
    template <> void serialize(Archive& archive, const InserterData& value);
    template <> InserterData deserialize(const UnserializedObject& s);

    // StorageData
    template <> void serialize(Archive& archive, const StorageData& value);
    template <> StorageData deserialize(const UnserializedObject& s);

    // TerrainType (as uint8_t)
    template <> void serialize(Archive& archive, const TerrainType& value);
    template <> TerrainType deserialize(const UnserializedObject& s);
}
