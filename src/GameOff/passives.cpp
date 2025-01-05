#include "passives.h"

namespace pg
{
    

    void CharacterApplicable::deleteOldType()
    {

    }

    void CharacterApplicable::apply(Character& chara)
    {
        if (type == ApplicableFunctionType::Noop)
        {
            // Nothing to do here.
        }
        else if (type == ApplicableFunctionType::Functional)
        {

        }
        else if (type == ApplicableFunctionType::Script)
        {

        }
        else
        {
            LOG_ERROR("Passives", "Passive type is unknown !");
        }
    }
}