#include "coresystems.h"

#include "UI/textinput.h"

namespace pg
{
    static constexpr char const * DOM = "Core System";

    template <>
    void serialize(Archive& archive, const EntityName& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(EntityName::getType());

        serialize(archive, "entityName", value.name);

        archive.endSerialization();
    }

    template <>
    EntityName deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an EntityName");

            auto entityName = deserialize<std::string>(serializedString["entityName"]);

            return EntityName{entityName};
        }

        return EntityName{""};
    }

    void RunScriptFromTextInputSystem::onEvent(const TextInputTriggeredEvent& event)
    {
        LOG_THIS_MEMBER("Run Script From Text Input System");

        if (not event.entity.has<TextInputComponent>())
        {
            LOG_ERROR("Run Script From Text Input System", "Entity has no Text Input Component");
        }

        auto textComp = event.entity.get<TextInputComponent>();

        LOG_INFO("Run Script From Text Input System", "Trying to execute script: " << textComp->returnText);

        ecsRef->sendEvent(ExecuteFileScriptEvent{textComp->returnText});
    }
}