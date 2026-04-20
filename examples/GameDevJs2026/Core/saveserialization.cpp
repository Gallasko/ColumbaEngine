#include "saveserialization.h"

#include "craftingsystem.h"
#include "minersystem.h"
#include "insertersystem.h"
#include "storagesystem.h"
#include "depotsystem.h"

namespace pg
{
    // ===== ItemStack =====

    template <>
    void serialize(Archive& archive, const ItemStack& value)
    {
        archive.startSerialization("ItemStack");
        serialize(archive, "id", static_cast<unsigned int>(value.id));
        serialize(archive, "count", static_cast<unsigned int>(value.count));
        archive.endSerialization();
    }

    template <>
    ItemStack deserialize(const UnserializedObject& s)
    {
        ItemStack result;
        if (s.isNull()) return result;

        unsigned int id = 0, count = 0;
        defaultDeserialize(s, "id", id);
        defaultDeserialize(s, "count", count);
        result.id = static_cast<ItemId>(id);
        result.count = static_cast<uint16_t>(count);
        return result;
    }

    // ===== Inventory =====

    template <>
    void serialize(Archive& archive, const Inventory& value)
    {
        archive.startSerialization("Inventory");
        serialize(archive, "slots", value.slots);
        archive.endSerialization();
    }

    template <>
    Inventory deserialize(const UnserializedObject& s)
    {
        Inventory result;
        if (s.isNull()) return result;
        defaultDeserialize(s, "slots", result.slots);
        return result;
    }

    // ===== SavedBuilding =====

    template <>
    void serialize(Archive& archive, const SavedBuilding& value)
    {
        archive.startSerialization(SavedBuilding::getType());
        serialize(archive, "x", value.x);
        serialize(archive, "y", value.y);
        serialize(archive, "tileId", static_cast<unsigned int>(value.tileId));
        serialize(archive, "direction", static_cast<unsigned int>(value.direction));
        serialize(archive, "enterDirection", static_cast<unsigned int>(value.enterDirection));
        serialize(archive, "conveyorTileIndex", value.conveyorTileIndex);
        archive.endSerialization();
    }

    template <>
    SavedBuilding deserialize(const UnserializedObject& s)
    {
        SavedBuilding result;
        if (s.isNull()) return result;

        defaultDeserialize(s, "x", result.x);
        defaultDeserialize(s, "y", result.y);

        unsigned int tileId = 0, dir = 0, enterDir = 0;
        defaultDeserialize(s, "tileId", tileId);
        defaultDeserialize(s, "direction", dir);
        defaultDeserialize(s, "enterDirection", enterDir);
        result.tileId = static_cast<uint16_t>(tileId);
        result.direction = static_cast<uint8_t>(dir);
        result.enterDirection = static_cast<uint8_t>(enterDir);

        defaultDeserialize(s, "conveyorTileIndex", result.conveyorTileIndex);
        return result;
    }

    // ===== MachineData =====

    template <>
    void serialize(Archive& archive, const MachineData& value)
    {
        archive.startSerialization("MachineData");
        serialize(archive, "ownerX", value.ownerX);
        serialize(archive, "ownerY", value.ownerY);
        serialize(archive, "machineType", static_cast<unsigned int>(value.machineType));
        serialize(archive, "inputSlots", value.inputSlots);
        serialize(archive, "outputSlots", value.outputSlots);
        serialize(archive, "craftProgress", value.craftProgress);
        archive.endSerialization();
    }

    template <>
    MachineData deserialize(const UnserializedObject& s)
    {
        MachineData result;
        if (s.isNull()) return result;

        defaultDeserialize(s, "ownerX", result.ownerX);
        defaultDeserialize(s, "ownerY", result.ownerY);

        unsigned int machineType = 0;
        defaultDeserialize(s, "machineType", machineType);
        result.machineType = static_cast<uint16_t>(machineType);

        defaultDeserialize(s, "inputSlots", result.inputSlots);
        defaultDeserialize(s, "outputSlots", result.outputSlots);
        defaultDeserialize(s, "craftProgress", result.craftProgress);

        return result;
    }

    // ===== MinerData =====

    template <>
    void serialize(Archive& archive, const MinerData& value)
    {
        archive.startSerialization("MinerData");
        serialize(archive, "ownerX", value.ownerX);
        serialize(archive, "ownerY", value.ownerY);
        serialize(archive, "outputSlots", value.outputSlots);
        serialize(archive, "mineProgress", value.mineProgress);
        serialize(archive, "isMining", value.isMining);
        archive.endSerialization();
    }

    template <>
    MinerData deserialize(const UnserializedObject& s)
    {
        MinerData result;
        if (s.isNull()) return result;

        defaultDeserialize(s, "ownerX", result.ownerX);
        defaultDeserialize(s, "ownerY", result.ownerY);
        defaultDeserialize(s, "outputSlots", result.outputSlots);
        defaultDeserialize(s, "mineProgress", result.mineProgress);
        defaultDeserialize(s, "isMining", result.isMining);

        return result;
    }

    // ===== InserterData =====

    template <>
    void serialize(Archive& archive, const InserterData& value)
    {
        archive.startSerialization("InserterData");
        serialize(archive, "x", value.x);
        serialize(archive, "y", value.y);
        serialize(archive, "direction", static_cast<unsigned int>(value.direction));
        serialize(archive, "state", static_cast<unsigned int>(value.state));
        serialize(archive, "heldItem", static_cast<unsigned int>(value.heldItem));
        serialize(archive, "animFrame", value.animFrame);
        archive.endSerialization();
    }

    template <>
    InserterData deserialize(const UnserializedObject& s)
    {
        InserterData result;
        if (s.isNull()) return result;

        defaultDeserialize(s, "x", result.x);
        defaultDeserialize(s, "y", result.y);

        unsigned int dir = 0, state = 0, heldItem = 0;
        defaultDeserialize(s, "direction", dir);
        defaultDeserialize(s, "state", state);
        defaultDeserialize(s, "heldItem", heldItem);
        result.direction = static_cast<uint8_t>(dir);
        result.state = static_cast<InserterState>(state);
        result.heldItem = static_cast<ItemId>(heldItem);

        defaultDeserialize(s, "animFrame", result.animFrame);

        return result;
    }

    // ===== StorageData =====

    template <>
    void serialize(Archive& archive, const StorageData& value)
    {
        archive.startSerialization("StorageData");
        serialize(archive, "ownerX", value.ownerX);
        serialize(archive, "ownerY", value.ownerY);
        serialize(archive, "inventory", value.inventory);
        archive.endSerialization();
    }

    template <>
    StorageData deserialize(const UnserializedObject& s)
    {
        StorageData result;
        if (s.isNull()) return result;

        defaultDeserialize(s, "ownerX", result.ownerX);
        defaultDeserialize(s, "ownerY", result.ownerY);
        defaultDeserialize(s, "inventory", result.inventory);

        return result;
    }

    // ===== DepotData =====

    template <>
    void serialize(Archive& archive, const DepotData& value)
    {
        archive.startSerialization("DepotData");
        serialize(archive, "ownerX", value.ownerX);
        serialize(archive, "ownerY", value.ownerY);
        serialize(archive, "inventory", value.inventory);
        archive.endSerialization();
    }

    template <>
    DepotData deserialize(const UnserializedObject& s)
    {
        DepotData result;
        if (s.isNull()) return result;

        defaultDeserialize(s, "ownerX", result.ownerX);
        defaultDeserialize(s, "ownerY", result.ownerY);
        defaultDeserialize(s, "inventory", result.inventory);

        return result;
    }

    // ===== TerrainType =====

    template <>
    void serialize(Archive& archive, const TerrainType& value)
    {
        serialize(archive, static_cast<unsigned int>(value));
    }

    template <>
    TerrainType deserialize(const UnserializedObject& s)
    {
        unsigned int val = 0;
        val = deserialize<unsigned int>(s);
        return static_cast<TerrainType>(val);
    }
}
