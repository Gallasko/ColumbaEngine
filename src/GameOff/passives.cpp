#include "passives.h"

namespace pg
{
    // void CharacterApplicable::apply(Character& chara)
    // {
    //     if (type == ApplicableFunctionType::Noop)
    //     {
    //         // Nothing to do here.
    //     }
    //     else if (type == ApplicableFunctionType::Functional)
    //     {

    //     }
    //     else if (type == ApplicableFunctionType::Script)
    //     {

    //     }
    //     else
    //     {
    //         LOG_ERROR("Passives", "Passive type is unknown !");
    //     }
    // }

    template <>
    void serialize(Archive& archive, const PassiveInfo& value)
    {
        archive.startSerialization("PassiveInfo");

        serialize(archive, "type", passiveTypeToString.at(value.type));

        serialize(archive, "trigger", triggerTypeToString.at(value.trigger));

        // serialize(archive, "name", value.name);

        serialize(archive, "remainingTurns", value.remainingTurns);

        serialize(archive, "numberOfTriggerBeforeActivation", value.numberOfTriggerBeforeActivation);
        serialize(archive, "currentNbOfTriggerSinceLastActivation", value.currentNbOfTriggerSinceLastActivation);

        serialize(archive, "hidden", value.hidden);

        serialize(archive, "nbSuccesfulActivation", value.nbSuccesfulActivation);

        serialize(archive, "activationChance", value.activationChance);

        serialize(archive, "activationMaxChance", value.activationMaxChance);

        archive.endSerialization();
    }

    template <>
    PassiveInfo deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Passive Info", "Element is null");
        }
        else
        {
            LOG_MILE("Passive Info", "Deserializing PassiveCall");

            PassiveInfo data;

            std::string type = NOOPPASSIVE;

            defaultDeserialize(serializedString, "type", type);
            data.type = stringToPassiveType.at(type);

            std::string trigger = NOOPPASSIVE;

            defaultDeserialize(serializedString, "trigger", trigger);
            data.trigger = stringToTriggerType.at(trigger);

            // defaultDeserialize(serializedString, "name", data.name);

            defaultDeserialize(serializedString, "remainingTurns", data.remainingTurns);

            defaultDeserialize(serializedString, "numberOfTriggerBeforeActivation", data.numberOfTriggerBeforeActivation);
            defaultDeserialize(serializedString, "currentNbOfTriggerSinceLastActivation", data.currentNbOfTriggerSinceLastActivation);

            defaultDeserialize(serializedString, "hidden", data.hidden);

            defaultDeserialize(serializedString, "nbSuccesfulActivation", data.nbSuccesfulActivation);

            defaultDeserialize(serializedString, "activationChance", data.activationChance);
            defaultDeserialize(serializedString, "activationMaxChance", data.activationMaxChance);

            return data;
        }

        return PassiveInfo{};
    }

    template <>
    void serialize(Archive& archive, const PassiveCall& value)
    {
        archive.startSerialization("PassiveCall");

        serialize(archive, "passiveName", value.passiveName);

        serialize(archive, "passiveInfo", value.info);

        serialize(archive, "args", value.args);

        archive.endSerialization();
    }

    template <>
    PassiveCall deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Passive Call", "Element is null");
        }
        else
        {
            LOG_MILE("Passive Call", "Deserializing PassiveCall");

            PassiveCall data;

            defaultDeserialize(serializedString, "passiveName", data.passiveName);

            defaultDeserialize(serializedString, "passiveInfo", data.info);

            defaultDeserialize(serializedString, "args", data.args);

            return data;
        }

        return PassiveCall{};
    }

    void PassiveDatabase::storePassive(const Passive& passive)
    {
        database[passive.name] = passive;
    }

    Passive PassiveDatabase::resolvePassive(const PassiveCall& call) const
    {
        const auto& it = database.find(call.passiveName);

        if (it != database.end())
        {
            auto passive = it->second;

            for (const auto& arg : call.args)
            {
                passive.applyOnCharacter.args[arg.first] = arg.second;
                passive.removeFromCharacter.args[arg.first] = arg.second;
            }

            return passive;
        }

        Passive fail;

        fail.name = NOOPPASSIVE;

        return fail;
    }
}