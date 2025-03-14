#include "location.h"

namespace pg
{
    namespace
    {
        static constexpr char const * const DOM = "location";
    }

    template <>
    void serialize(Archive& archive, const DropChance& value)
    {
        archive.startSerialization("DropChance");

        serialize(archive, "item", value.item);
        serialize(archive, "chance", value.dropChance);
        serialize(archive, "quantity", value.quantity);

        archive.endSerialization();
    }

    template <>
    DropChance deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Drop chance");

            DropChance data;

            data.item = deserialize<Item>(serializedString["item"]);

            if (serializedString.find("chance"))
                data.dropChance = deserialize<float>(serializedString["chance"]);

            if (serializedString.find("quantity"))
                data.quantity = deserialize<size_t>(serializedString["quantity"]);

            return data;
        }

        return DropChance{};
    }

    template <>
    void serialize(Archive& archive, const Encounter& value)
    {
        archive.startSerialization("Encouter");

        serialize(archive, "enemies", value.characters);
        serialize(archive, "drops", value.dropTable);
        serialize(archive, "weight", value.weight);

        archive.endSerialization();
    }

    template <>
    Encounter deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_MILE(DOM, "Deserializing Encounter");

            Encounter data;

            data.characters = deserializeVector<Character>(serializedString["enemies"]);
            data.dropTable = deserializeVector<DropChance>(serializedString["drops"]);

            defaultDeserialize(serializedString, "weight", data.weight);

            return data;
        }

        return Encounter{};
    }

    template <>
    void serialize(Archive& archive, const Location& value)
    {
        archive.startSerialization("Location");
        
        serialize(archive, "name", value.name);
        serialize(archive, "nbEncounters", value.nbEncounterBeforeBoss);
        serialize(archive, "random", value.randomEncounter);
        serialize(archive, "encounters", value.possibleEnounters);
        serialize(archive, "boss", value.bossEncounter);
        serialize(archive, "finished", value.finished);
        serialize(archive, "prerequisite", value.prerequisiteFacts);
        
        archive.endSerialization();
    }

    template <>
    Location deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Location");

            Location data;

            data.name = deserialize<std::string>(serializedString["name"]);

            defaultDeserialize(serializedString, "nbEncounters", data.nbEncounterBeforeBoss);
            defaultDeserialize(serializedString, "random", data.randomEncounter);
            defaultDeserialize(serializedString, "boss", data.bossEncounter);
            defaultDeserialize(serializedString, "finished", data.finished);
            defaultDeserialize(serializedString, "prerequisite", data.prerequisiteFacts);

            data.possibleEnounters = deserializeVector<Encounter>(serializedString["encounters"]);
            
            LOG_INFO(DOM, "Location deserialized");

            return data;
        }

        return Location{};
    }

    void Encounter::addEncounter(const Encounter& encounter)
    {
        characters.insert(characters.end(), encounter.characters.begin(), encounter.characters.end());
        dropTable.insert(dropTable.end(), encounter.dropTable.begin(), encounter.dropTable.end());
    }
}