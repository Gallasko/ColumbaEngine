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
    void serialize(Archive& archive, const PassiveCall& value)
    {
        archive.startSerialization("PassiveCall");

        serialize(archive, "passiveName", value.passiveName);

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
            LOG_INFO("Passive Call", "Deserializing PassiveCall");

            PassiveCall data;

            defaultDeserialize(serializedString, "passiveName", data.passiveName);

            defaultDeserialize(serializedString, "args", data.args);

            return data;
        }

        return PassiveCall{};
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

        fail.info.name = NOOPPASSIVE;

        return fail;
    }
}