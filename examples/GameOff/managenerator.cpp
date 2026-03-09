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
        serialize(archive, "active", value.active);
        serialize(archive, "prestigeTags", value.prestigeTags);

        archive.endSerialization();
    }

    template <>
    RessourceGenerator deserialize(const UnserializedObject& serializedString)
    {
        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("RessourceGenerator", "Element is null");
        }
        else
        {
            RessourceGenerator data;

            defaultDeserialize(serializedString, "id", data.id);
            defaultDeserialize(serializedString, "ressource", data.ressource);
            defaultDeserialize(serializedString, "currentMana", data.currentMana);
            defaultDeserialize(serializedString, "productionRate", data.productionRate);
            defaultDeserialize(serializedString, "capacity", data.capacity);
            defaultDeserialize(serializedString, "active", data.active);
            defaultDeserialize(serializedString, "prestigeTags", data.prestigeTags);

            return data;
        }

        return RessourceGenerator{};
    }

    template <>
    void serialize(Archive& archive, const ConverterComponent& value)
    {
        archive.startSerialization("ConverterComponent");

        serialize(archive, "id", value.id);
        serialize(archive, "input", value.input);
        serialize(archive, "output", value.output);
        serialize(archive, "cost", value.cost);
        serialize(archive, "yield", value.yield);
        serialize(archive, "active", value.active);
        serialize(archive, "prestigeTags", value.prestigeTags);

        archive.endSerialization();
    }

    template <>
    ConverterComponent deserialize(const UnserializedObject& serializedString)
    {
        if (serializedString.isNull())
        {
            LOG_ERROR("ConverterComponent", "Element is null");
            return ConverterComponent{};
        }

        ConverterComponent data;

        defaultDeserialize(serializedString, "id", data.id);
        defaultDeserialize(serializedString, "input", data.input);
        defaultDeserialize(serializedString, "output", data.output);
        defaultDeserialize(serializedString, "cost", data.cost);
        defaultDeserialize(serializedString, "yield", data.yield);
        defaultDeserialize(serializedString, "active", data.active);
        defaultDeserialize(serializedString, "prestigeTags", data.prestigeTags);

        return data;
    }
}