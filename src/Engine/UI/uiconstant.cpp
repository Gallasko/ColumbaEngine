#include "uiconstant.h"

#include "uisystem.h"

namespace pg
{
    namespace
	{
		static constexpr char const * const DOM = "Ui Constant";
	}

    float UiSize::UiValue::returnCurrentSize() const
    {
        LOG_THIS_MEMBER(DOM);

        // Todo make an independant ui handler so the size hierachy doens't depend on a working ecs env
        // if (ecsRef and not dirty)
        // {
        //     return currentValue;
        // }

        const float refSizeValue1 = refSize1 == nullptr ? 0.0f : refSize1->returnCurrentSize();
        const float refSizeValue2 = refSize2 == nullptr ? 0.0f : refSize2->returnCurrentSize();

        const float refSize1Result = pixelSize + refSizeValue1 * scaleValue;

        //TODO make exception for division by 0
        switch(opType)
        {
            case UiSizeOpType::ADD:
                currentValue = refSize1Result + refSizeValue2;
                break;
            case UiSizeOpType::SUB:
                currentValue = refSize1Result - refSizeValue2;
                break;
            case UiSizeOpType::MUL:
                currentValue = refSize1Result * refSizeValue2;
                break;
            case UiSizeOpType::DIV:
                if (refSizeValue2 == 0.0f)
                {
                    // TODO
                    LOG_ERROR("Ui Constant", "Division by zero");
                    currentValue = 0.0f;
                }
                else
                {
                    currentValue = refSize1Result / refSizeValue2;
                }
                break;
            case UiSizeOpType::NONE:
            default:
                currentValue = refSize1Result;
        }

        // Todo add this flag
        dirty = false;

        if(ecsRef and areNotAlmostEqual(currentValue, oldValue))
        {
            auto entity = ecsRef->getEntity(entityId);

            if(entity and entity->has<UiComponent>())
            {
                oldValue = currentValue;
                ecsRef->sendEvent(UiSizeChangeEvent{entityId});
            }
        }

        return currentValue;
    }

    /**
     * @brief Specialization of the serialize function for UiSize 
     * 
     * @param archive A references to the archive
     * @param value The ui size value
     */
    template <>
    void serialize(Archive& archive, const UiSize::UiValue& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization("UiValue");

        serialize(archive, "pixelSize", value.pixelSize);
        serialize(archive, "scaleValue", value.scaleValue);

        serialize(archive, "id", value.entityId);

        std::string op = "none";

        switch (value.opType)
        {
            case UiSize::UiValue::UiSizeOpType::ADD:
            {
                op = "add";
                
                break;
            }

            case UiSize::UiValue::UiSizeOpType::SUB:
            {
                op = "sub";
                break;
            }

            case UiSize::UiValue::UiSizeOpType::MUL:
            {
                op = "mul";
                break;
            }

            case UiSize::UiValue::UiSizeOpType::DIV:
            {
                op = "div";
                break;
            }

            default:
                op = "none";
        }

        serialize(archive, "op", op);

        serialize(archive, "dir", value.dir);

        if (value.refSize1)
        {
            serialize(archive, "ref1", *(value.refSize1));
        }

        if (value.refSize2)
        {
            serialize(archive, "ref2", *(value.refSize2));
        }

        archive.endSerialization();
    }

    /**
     * @brief Specialization of the serialize function for UiSize 
     * 
     * @param archive A references to the archive
     * @param value The ui size value
     */
    template <>
    void serialize(Archive& archive, const UiSize& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization("UiSize");

        // Todo be more specific with value ! (! id and dir of ref1 and ref2 !)
        serialize(archive, "floatValue", static_cast<float>(value));

        serialize(archive, "value", *(value.value));

        archive.endSerialization();
    }

}