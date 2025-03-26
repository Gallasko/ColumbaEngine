#include "managenerator.h"

namespace pg
{
    template <>
    void serialize(Archive& archive, const RessourceGenerator& value)
    {
        archive.startSerialization("RessourceGenerator");

        serialize(archive, "id", value.id);
        serialize(archive, "ressource", value.ressource);
        serialize(archive, "currentMana", value.currentMana);
        serialize(archive, "productionRate", value.productionRate);
        serialize(archive, "capacity", value.capacity);

        archive.endSerialization();
    }

    template <>
    RessourceGenerator deserialize(const UnserializedObject& serializedString)
    {
        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Achievement", "Element is null");
        }
        else
        {
            LOG_INFO("Achievement", "Deserializing Achievement");

            RessourceGenerator data;

            defaultDeserialize(serializedString, "id", data.id);
            defaultDeserialize(serializedString, "ressource", data.ressource);
            defaultDeserialize(serializedString, "currentMana", data.currentMana);
            defaultDeserialize(serializedString, "productionRate", data.productionRate);
            defaultDeserialize(serializedString, "capacity", data.capacity);

            return data;
        }

        return RessourceGenerator{};
    }
}