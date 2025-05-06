#pragma once

#include "serialization.h"

#include "ECS/system.h"

#include "2D/texture.h"

#include "UI/sizer.h"
#include "UI/textinput.h"

namespace pg
{
    struct NewSceneLoaded;

    namespace editor
    {
        struct InspectEvent { EntityRef entity; };

        struct ValueChanged { std::string valueName; std::string value; };

        struct InspectedText
        {
            InspectedText(const std::string& name, std::string *valuePointer, _unique_id id) : name(name), valuePointer(valuePointer), id(id) {}
            InspectedText(const InspectedText& rhs) : name(rhs.name), valuePointer(rhs.valuePointer), id(rhs.id) {}

            std::string name;
            std::string *valuePointer = nullptr;
            _unique_id id;
        };

        struct InspectorSystem : public System<Listener<InspectEvent>, Listener<StandardEvent>, Listener<NewSceneLoaded>, Listener<EntityChangedEvent>, InitSys>
        {
            virtual void onEvent(const StandardEvent& event) override;

            virtual void init() override;

            void addNewText(const std::string& text);

            void addNewAttribute(const std::string& text, const std::string& type, std::string& value);

            void printChildren(SerializedInfoHolder& parent, size_t indentLevel);

            virtual void onEvent(const EntityChangedEvent& event) override { eventQueue.push(event); }

            virtual void onEvent(const InspectEvent& event) override;

            virtual void onEvent(const NewSceneLoaded& event) override;

            virtual void execute() override;

            void processEntityChanged(const EntityChangedEvent& event);

            void deserializeCurrentEntity();

            InspectorArchive archive;

            std::vector<InspectedText> inspectorText;

            CompRef<VerticalLayout> view;

            CompRef<UiComponent> tabUi;

            InspectEvent event;

            bool eventRequested = false;

            bool needDeserialization = false;

            bool needUpdateEntity = false;

            bool needClear = false;

            std::queue<EntityChangedEvent> eventQueue;

            _unique_id currentId = 0;
        };

        /**
         * Helper functions for building common Inspector UI widgets as prefabs.
         */
        class InspectorWidgets
        {
        public:
            /**
             * Creates a labeled text-input row and adds it to the given vertical layout.
             *
             * @param ecs         Pointer to the EntitySystem
             * @param parentLayout   The Inspector's VerticalLayout to which to add this row
             * @param labelText   The label to display on the left
             * @param boundValue  Reference to the underlying std::string that backs the TextInputComponent
             * @param onChange    StandardEvent to fire when the user commits a change
             */
            static void makeLabeledTextInput(EntitySystem* ecs, BaseLayout* parentLayout, const std::string& labelText, std::string& boundValue, InspectorSystem* sys);
        };
    }

}