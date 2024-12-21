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
            data.dropChance = deserialize<float>(serializedString["chance"]);
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
            LOG_INFO(DOM, "Deserializing Encounter");

            LOG_INFO(DOM, "Deserializing: " << serializedString.getObjectType() << ", " << serializedString.children.size());

            for (auto child : serializedString.children)
            {
                LOG_INFO(DOM, "Child: " << child.getObjectName());
            }

            Encounter data;

            data.characters = deserializeVector<Character>(serializedString["enemies"]);
            data.dropTable = deserializeVector<DropChance>(serializedString["drops"]);

            return data;
        }

        return Encounter{};
    }

    template <>
    void serialize(Archive& archive, const Location& value)
    {
        archive.startSerialization("Location");
        
        serialize(archive, "name", value.name);
        serialize(archive, "name", value.name);
        serialize(archive, "name", value.name);
        serialize(archive, "name", value.name);
        serialize(archive, "name", value.name);
        serialize(archive, "name", value.name);
        

        // serialize(archive, "item", value.item);

        archive.endSerialization();
    }
}