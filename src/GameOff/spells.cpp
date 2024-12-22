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
        serialize(archive, "elements", value.elementType);

        serialize(archive, "targetPassives", value.applyToTarget);
        serialize(archive, "selfPassives", value.applyToSelf);

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

            defaultDeserialize(data.name, "name", serializedString);
            // serialize(serializedString, "dmg", value.baseDmg);
            // serialize(archive, "manaCost", value.baseManaCost);
            // serialize(archive, "cooldown", value.baseCooldown);
            // serialize(archive, "adMult", value.physicalMultipler);
            // serialize(archive, "apMult", value.magicalMultipler);
            // serialize(archive, "elements", value.elementType);

            // serialize(archive, "targetPassives", value.applyToTarget);
            // serialize(archive, "selfPassives", value.applyToSelf);

            // serialize(archive, "selfOnly", value.selfOnly);
            // serialize(archive, "nbTargets", value.nbTargets);
            // serialize(archive, "canHitMultipleTime", value.canTargetSameCharacterMultipleTimes);

            return data;
        }

        return Spell{};
    }
}