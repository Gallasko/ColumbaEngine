#include "achievement.h"

namespace pg
{
    template <>
    void serialize(Archive& archive, const Achievement& value)
    {
        archive.startSerialization(Achievement::getType());

        serialize(archive, "name", value.name);
        serialize(archive, "unlocked", value.unlocked);
        serialize(archive, "prerequisiteFacts", value.prerequisiteFacts);

        archive.endSerialization();
    }

    template <>
    Achievement deserialize(const UnserializedObject& serializedString)
    {
        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Achievement", "Element is null");
        }
        else
        {
            LOG_INFO("Achievement", "Deserializing Achievement");

            Achievement data;

            data.name = deserialize<std::string>(serializedString["name"]);

            defaultDeserialize(serializedString, "unlocked", data.unlocked);
            defaultDeserialize(serializedString, "prerequisiteFacts", data.prerequisiteFacts);

            return data;
        }

        return Achievement{};
    }
}