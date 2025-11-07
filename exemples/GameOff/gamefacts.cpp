#include "gamefacts.h"

namespace pg
{
    template <>
    void serialize(Archive& archive, const AddFact& value)
    {
        archive.startSerialization("AddFact");

        serialize(archive, "name", value.name);
        serialize(archive, "value", value.value);
        serialize(archive, "metadata", value.metadata);

        archive.endSerialization();
    }

    template <>
    AddFact deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("AddFact");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("AddFact", "Element is null");
        }
        else
        {
            LOG_MILE("AddFact", "Deserializing AddFact");

            AddFact data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "value", data.value);
            defaultDeserialize(serializedString, "metadata", data.metadata);

            return data;
        }

        return AddFact{};
    }

    template <>
    void serialize(Archive& archive, const RemoveFact& value)
    {
        archive.startSerialization("RemoveFact");

        serialize(archive, "name", value.name);

        archive.endSerialization();
    }

    template <>
    RemoveFact deserialize(const UnserializedObject& serializedString)
    {
        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("RemoveFact", "Element is null");
        }
        else
        {
            LOG_MILE("RemoveFact", "Deserializing RemoveFact");

            RemoveFact data;

            defaultDeserialize(serializedString, "name", data.name);

            return data;
        }

        return RemoveFact{};
    }

    template <>
    void serialize(Archive& archive, const IncreaseFact& value)
    {
        archive.startSerialization("IncreaseFact");

        serialize(archive, "name", value.name);
        serialize(archive, "value", value.value);
        serialize(archive, "metadata", value.metadata);

        archive.endSerialization();
    }

    template <>
    IncreaseFact deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("IncreaseFact");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("IncreaseFact", "Element is null");
        }
        else
        {
            LOG_MILE("IncreaseFact", "Deserializing IncreaseFact");

            IncreaseFact data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "value", data.value);
            defaultDeserialize(serializedString, "metadata", data.metadata);

            return data;
        }

        return IncreaseFact{};
    }

    template <>
    void serialize(Archive& archive, const FactChecker& value)
    {
        archive.startSerialization("FactChecker");

        serialize(archive, "name", value.name);
        serialize(archive, "value", value.value);
        serialize(archive, "equality", equalityToString.at(value.equality));

        archive.endSerialization();
    }

    template <>
    FactChecker deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("Fact checker");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Fact checker", "Element is null");
        }
        else
        {
            LOG_MILE("Fact checker", "Deserializing FactChecker");

            FactChecker data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "value", data.value);

            std::string equality = "None";
            defaultDeserialize(serializedString, "equality", equality);

            data.equality = stringToEquality.at(equality);

            return data;
        }

        return FactChecker{};
    }
}