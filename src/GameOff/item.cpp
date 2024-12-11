#include "item.h"

namespace pg
{
    namespace
    {
        static constexpr const char * const DOM = "Item";
    }

    template <>
    void serialize(Archive& archive, const Item& value)
    {
        archive.startSerialization("Item");

        serialize(archive, "name", value.name);
        serialize(archive, "type", itemTypeToString.at(value.type));
        serialize(archive, "stacksize", value.stacksize);
        serialize(archive, "nbItems", value.nbItems);
        serialize(archive, "description", value.description);
        
        // Todo
        // serialize(archive, "rarity", itemR value.nbItems);
        // for (auto att : value.attributes) serialize(archive, att.first, att.second);

        archive.endSerialization();
    }

    template <>
    Item deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Item");

            Item data;

            data.name = deserialize<std::string>(serializedString["name"]);
            data.type = stringToItemType.at(deserialize<std::string>(serializedString["type"]));
            data.stacksize = deserialize<int>(serializedString["stacksize"]);
            data.nbItems = deserialize<size_t>(serializedString["nbItems"]);
            data.description = deserialize<std::string>(serializedString["description"]);

            return data;
        }

        return Item{};
    }
}