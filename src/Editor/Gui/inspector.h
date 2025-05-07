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
        struct EndDragging
        {
            _unique_id id;
            float startX, startY;
            float endX, endY;
        };

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

        struct InspectorCommands
        {
            virtual ~InspectorCommands() {}
            virtual void execute() = 0;
            virtual void undo() = 0;
        };

        class InspectorCommandHistory
        {
        public:
            InspectorCommandHistory() = default;

            void execute(std::unique_ptr<InspectorCommands> command)
            {
                command->execute();
                undoStack.push_back(std::move(command));
                redoStack.clear();
            }

            void undo()
            {
                if (undoStack.empty())
                    return;

                auto cmd = std::move(undoStack.back());
                undoStack.pop_back();
                cmd->undo();
                redoStack.push_back(std::move(cmd));
            }

            void redo()
            {
                if (redoStack.empty())
                    return;

                auto cmd = std::move(redoStack.back());
                redoStack.pop_back();
                cmd->execute();
                undoStack.push_back(std::move(cmd));
            }

        private:
            std::vector<std::unique_ptr<InspectorCommands>> undoStack;
            std::vector<std::unique_ptr<InspectorCommands>> redoStack;
        };

        struct DraggingCommand : public InspectorCommands
        {
            DraggingCommand(EntitySystem* ecsRef, _unique_id id, float startX, float startY, float endX, float endY) :
                ecsRef(ecsRef), id(id), startX(startX), startY(startY), endX(endX), endY(endY) {}

            virtual void execute() override
            {
                auto ent = ecsRef->getEntity(id);

                if (not ent or not ent->has<PositionComponent>())
                    return;

                auto pos = ent->get<PositionComponent>();

                pos->setX(endX);
                pos->setY(endY);
            }

            virtual void undo() override
            {
                auto ent = ecsRef->getEntity(id);

                if (not ent or not ent->has<PositionComponent>())
                    return;

                auto pos = ent->get<PositionComponent>();

                pos->setX(startX);
                pos->setY(startY);
            }

            EntitySystem* ecsRef; _unique_id id;
            float startX, startY;
            float endX, endY;
        };

        struct InspectorSystem : public System<Listener<InspectEvent>, Listener<StandardEvent>, Listener<NewSceneLoaded>, QueuedListener<EntityChangedEvent>, QueuedListener<EndDragging>, InitSys>
        {
            virtual void onEvent(const StandardEvent& event) override;

            virtual void init() override;

            void addNewText(const std::string& text);

            void addNewAttribute(const std::string& text, const std::string& type, std::string& value);

            void printChildren(SerializedInfoHolder& parent);

            virtual void onProcessEvent(const EntityChangedEvent& event) override;

            virtual void onEvent(const InspectEvent& event) override;

            virtual void onEvent(const NewSceneLoaded& event) override;

            virtual void onProcessEvent(const EndDragging& event) override
            {
                history.execute(std::make_unique<DraggingCommand>(ecsRef, event.id, event.startX, event.startY, event.endX, event.endY));
            }

            template <typename Comp>
            void registerCustomDrawer(std::function<void(InspectorSystem*, SerializedInfoHolder&)> drawer)
            {
                if constexpr(HasStaticName<Comp>::value)
                {
                    customDrawers.emplace(Comp::getType(), drawer);
                }
                else
                {
                    LOG_ERROR("InspectorSystem", "Can't register custom drawer for non-named component: " << typeid(Comp).name());
                }
            }

            void registerCustomDrawer(const std::string& type, std::function<void(InspectorSystem*, SerializedInfoHolder&)> drawer)
            {
                customDrawers.emplace(type, drawer);
            }

            virtual void execute() override;

            void processEntityChanged(const EntityChangedEvent& event);

            void deserializeCurrentEntity();

            InspectorCommandHistory history;

            InspectorArchive archive;

            std::vector<InspectedText> inspectorText;

            CompRef<VerticalLayout> view;

            CompRef<UiComponent> tabUi;

            InspectEvent event;

            bool eventRequested = false;

            bool needDeserialization = false;

            bool needUpdateEntity = false;

            bool needClear = false;

            std::unordered_map<std::string, std::function<void(InspectorSystem*, SerializedInfoHolder&)>> customDrawers;

            _unique_id currentId = 0;
        };

        void defaultInspectWidget(InspectorSystem* sys, SerializedInfoHolder& parent);

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