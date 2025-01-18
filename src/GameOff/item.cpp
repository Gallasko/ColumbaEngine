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
            LOG_MILE(DOM, "Deserializing Item");

            Item data;

            defaultDeserialize(serializedString, "name", data.name);

            std::string type = itemTypeToString.at(data.type);
            defaultDeserialize(serializedString, "type", type);
            data.type = stringToItemType.at(type);

            defaultDeserialize(serializedString, "stacksize", data.stacksize);
            defaultDeserialize(serializedString, "nbItems", data.nbItems);
            defaultDeserialize(serializedString, "description", data.description);

            return data;
        }

        return Item{};
    }
}