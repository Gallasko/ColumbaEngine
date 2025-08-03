#include "stdafx.h"

#include "focusable.h"

#include "serialization.h"

namespace pg
{
    namespace
    {
        static constexpr const char * const DOM = "Focusable";
    }

    template<>
    void serialize(Archive& archive, const FocusableComponent& component)
    {
        LOG_THIS(DOM);

        archive.startSerialization(FocusableComponent::getType());

        serialize(archive, "entityId", component.entityId);

        serialize(archive, "alwaysFocused", component.alwaysFocus);
        serialize(archive, "focused", component.focused);

        archive.endSerialization();
    }

    template <>
    FocusableComponent deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        FocusableComponent value;

        for(auto& element : serializedString.children)
        {
            if(element.isNull())
            {
                LOG_ERROR(DOM, "Element is null");
            }
            else if(element.getObjectName() == "")
            {
                LOG_ERROR(DOM, "Element has no name");
            }
            else
            {
                LOG_INFO(DOM, "Deserializing " + element.getObjectName());

                // Todo fix this
                // value.entityId = deserialize<_unique_id>(serializedString["entityId"]);

                value.alwaysFocus = deserialize<bool>(serializedString["alwaysFocused"]);
                value.focused = deserialize<bool>(serializedString["focused"]);
            }
        }

        return value;
    }
}