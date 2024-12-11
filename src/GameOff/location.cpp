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
    void serialize(Archive& archive, const CharacterList& value)
    {
        archive.startSerialization("ChracterList");

        for (size_t i = 0; i < value.characters.size(); i++)
        {
            serialize(archive, value.characters.at(i));
        }

        archive.endSerialization();
    }

    template <>
    CharacterList deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Character list");

            CharacterList data;

            LOG_INFO("Module", "Deserialising: " << serializedString.getObjectName() << ", type: " << serializedString.getObjectType());

            for (const auto& child : serializedString.children)
            {
                if (child.isNull())
                    continue;

                LOG_INFO("Module", "Deserialising: " << child.getObjectName() << ", type: " << child.getObjectType());
                
                auto chara = deserialize<Character>(child);

                data.characters.push_back(chara);
            }            

            return data;
        }

        return CharacterList{};
    }

    template <>
    void serialize(Archive& archive, const DropChanceList& value)
    {
        archive.startSerialization("DropChance");

        // serialize(archive, "item", value.item);

        archive.endSerialization();
    }

    template <>
    void serialize(Archive& archive, const Encounter& value)
    {
        archive.startSerialization("DropChance");

        // serialize(archive, "item", value.item);

        archive.endSerialization();
    }

    template <>
    void serialize(Archive& archive, const Location& value)
    {
        archive.startSerialization("DropChance");

        // serialize(archive, "item", value.item);

        archive.endSerialization();
    }
}