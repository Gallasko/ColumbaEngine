#include "spells.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        static const char * const DOM = "Spell";
    }

    template <>
    void serialize(Archive& archive, const Spell& value)
    {
        archive.startSerialization("Spell");

        serialize(archive, "name", value.name);
        serialize(archive, "dmg", value.baseDmg);
        serialize(archive, "manaCost", value.baseManaCost);
        serialize(archive, "cooldown", value.baseCooldown);
        serialize(archive, "adMult", value.physicalMultipler);
        serialize(archive, "apMult", value.magicalMultipler);
        
        // Todo
        // serialize(archive, "elements", value.elementType);

        // serialize(archive, "targetPassives", value.applyToTarget);
        // serialize(archive, "selfPassives", value.applyToSelf);

        serialize(archive, "selfOnly", value.selfOnly);
        serialize(archive, "nbTargets", value.nbTargets);
        serialize(archive, "canHitMultipleTime", value.canTargetSameCharacterMultipleTimes);

        archive.endSerialization();
    }

    template <>
    Spell deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Spell");

            Spell data;

            defaultDeserialize(serializedString, "name", data.name);
            defaultDeserialize(serializedString, "dmg", data.baseDmg);
            defaultDeserialize(serializedString, "manaCost", data.baseManaCost);
            defaultDeserialize(serializedString, "cooldown", data.baseCooldown);
            defaultDeserialize(serializedString, "adMult", data.physicalMultipler);
            defaultDeserialize(serializedString, "apMult", data.magicalMultipler);

            // Todo
            // defaultDeserialize(serializedString, "elements", data.elementType);

            // defaultDeserialize(serializedString, "targetPassives", data.applyToTarget);
            // defaultDeserialize(serializedString, "selfPassives", data.applyToSelf);

            defaultDeserialize(serializedString, "selfOnly", data.selfOnly);
            defaultDeserialize(serializedString, "nbTargets", data.nbTargets);
            defaultDeserialize(serializedString, "canHitMultipleTime", data.canTargetSameCharacterMultipleTimes);

            return data;
        }

        return Spell{};
    }
}