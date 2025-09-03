#include "nexusscene.h"

#include "Systems/logmodule.h"
#include <iomanip>
#include <sstream>

namespace pg
{
    template <>
    void serialize(Archive& archive, const NexusButtonCost& value)
    {
        archive.startSerialization("NexusButtonCost");

        serialize(archive, "resourceId", value.resourceId);
        serialize(archive, "value", value.value);
        serialize(archive, "valueId", value.valueId);
        serialize(archive, "consumed", value.consumed);

        archive.endSerialization();
    }

    template <>
    NexusButtonCost deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("NexusButtonCost");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("NexusButtonCost", "Element is null");
        }
        else
        {
            LOG_MILE("NexusButtonCost", "Deserializing NexusButtonCost");

            NexusButtonCost data;

            defaultDeserialize(serializedString, "resourceId", data.resourceId);
            defaultDeserialize(serializedString, "value", data.value);
            defaultDeserialize(serializedString, "valueId", data.valueId);
            defaultDeserialize(serializedString, "consumed", data.consumed);

            return data;
        }

        return NexusButtonCost{};
    }

    template <>
    void serialize(Archive& archive, const DynamicNexusButton& value)
    {
        archive.startSerialization("DynamicNexusButton");

        serialize(archive, "id", value.id);
        serialize(archive, "label", value.label);
        serialize(archive, "conditions", value.conditions);
        serialize(archive, "outcome", value.outcome);
        serialize(archive, "category", value.category);
        serialize(archive, "description", value.description);
        serialize(archive, "nbClickBeforeArchive", value.nbClickBeforeArchive);
        serialize(archive, "costs", value.costs);
        serialize(archive, "costIncrease", value.costIncrease);
        serialize(archive, "neededConditionsForVisibility", value.neededConditionsForVisibility);
        serialize(archive, "nbClick", value.nbClick);
        serialize(archive, "archived", value.archived);
        serialize(archive, "activable", value.activable);
        serialize(archive, "active", value.active);
        serialize(archive, "activeTime", value.activeTime);
        serialize(archive, "activationTime", value.activationTime);

        archive.endSerialization();
    }

    template <>
    DynamicNexusButton deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("DynamicNexusButton");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("DynamicNexusButton", "Element is null");
        }
        else
        {
            LOG_MILE("DynamicNexusButton", "Deserializing DynamicNexusButton");

            DynamicNexusButton data;

            defaultDeserialize(serializedString, "id", data.id);
            defaultDeserialize(serializedString, "label", data.label);
            defaultDeserialize(serializedString, "conditions", data.conditions);
            defaultDeserialize(serializedString, "outcome", data.outcome);
            defaultDeserialize(serializedString, "category", data.category);
            defaultDeserialize(serializedString, "description", data.description);
            defaultDeserialize(serializedString, "nbClickBeforeArchive", data.nbClickBeforeArchive);
            defaultDeserialize(serializedString, "costs", data.costs);
            defaultDeserialize(serializedString, "costIncrease", data.costIncrease);
            defaultDeserialize(serializedString, "neededConditionsForVisibility", data.neededConditionsForVisibility);
            defaultDeserialize(serializedString, "nbClick", data.nbClick);
            defaultDeserialize(serializedString, "archived", data.archived);
            defaultDeserialize(serializedString, "activable", data.activable);
            defaultDeserialize(serializedString, "active", data.active);
            defaultDeserialize(serializedString, "activeTime", data.activeTime);
            defaultDeserialize(serializedString, "activationTime", data.activationTime);

            return data;
        }

        return DynamicNexusButton{};
    }

    // Expose floatToString for other files
    std::string floatToString(float value, int decimalPlaces)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(decimalPlaces) << value;
        return out.str();
    }
}