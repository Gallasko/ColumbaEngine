#pragma once

#include "serialization.h"

#include "ECS/system.h"

#include "2D/texture.h"

#include "UI/sizer.h"
#include "UI/textinput.h"

#include "Input/keyconfig.h"

namespace pg
{
    struct NewSceneLoaded;

    namespace editor
    {
        enum class EditorKeyConfig : uint8_t
        {
            Undo,
            Redo,
        };

        extern std::map<EditorKeyConfig, DefaultScancode> scancodeMap;

        struct EndDragging
        {
            _unique_id id;
            float startX, startY;
            float endX, endY;
        };

        struct ToggleInspectorEvent {};

        struct EditorAttachComponent
        {
            EditorAttachComponent(const std::string& name, _unique_id id) : name(name), id(id) {}
            EditorAttachComponent(const EditorAttachComponent& rhs) : name(rhs.name), id(rhs.id) {}

            EditorAttachComponent& operator=(const EditorAttachComponent& rhs)
            {
                name = rhs.name;
                id = rhs.id;

                return *this;
            }

            std::string name;
            _unique_id id;
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

        struct InspectorSystem;

        struct DraggingCommand : public InspectorCommands
        {
            DraggingCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, float startX, float startY, float endX, float endY) :
                inspectorSys(inspectorSys), ecsRef(ecsRef), startX(startX), startY(startY), endX(endX), endY(endY) {}

            virtual void execute() override;

            virtual void undo() override;

            InspectorSystem* inspectorSys; EntitySystem* ecsRef; _unique_id id;
            float startX, startY;
            float endX, endY;
        };

        struct AttachComponentCommand : public InspectorCommands
        {
            AttachComponentCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, _unique_id id, const std::string& name) : inspectorSys(inspectorSys), ecsRef(ecsRef), id(id), name(name) {}

            virtual void execute() override;
            virtual void undo() override;

            InspectorSystem *inspectorSys;
            EntitySystem *ecsRef;
            _unique_id id;
            std::string name;
        };

        struct CreateEntityCommand : public InspectorCommands
        {
            CreateEntityCommand(InspectorSystem *inspectorSys, EntitySystem *ecsRef, std::function<EntityRef(EntitySystem *)> callbackCreated) : inspectorSys(inspectorSys), ecsRef(ecsRef), callback(callbackCreated) { }

            virtual void execute() override;
            virtual void undo() override;

            InspectorSystem *inspectorSys;
            EntitySystem *ecsRef;
            std::function<EntityRef(EntitySystem *)> callback;
            _unique_id id;
            _unique_id lastFocusedId;
        };

        struct ResizeCommand : public InspectorCommands
        {
            ResizeCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, _unique_id entityId, ResizeHandle handle, 
                         float startWidth, float startHeight, float startX, float startY,
                         float endWidth, float endHeight, float endX, float endY) :
                inspectorSys(inspectorSys), ecsRef(ecsRef), entityId(entityId), handle(handle),
                startWidth(startWidth), startHeight(startHeight), startX(startX), startY(startY),
                endWidth(endWidth), endHeight(endHeight), endX(endX), endY(endY) {}

            virtual void execute() override;
            virtual void undo() override;

            InspectorSystem *inspectorSys;
            EntitySystem *ecsRef;
            _unique_id entityId;
            ResizeHandle handle;
            float startWidth, startHeight, startX, startY;
            float endWidth, endHeight, endX, endY;
        };

        struct RotationCommand : public InspectorCommands
        {
            RotationCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, _unique_id entityId, RotationHandle handle, 
                           float startRotation, float endRotation) :
                inspectorSys(inspectorSys), ecsRef(ecsRef), entityId(entityId), handle(handle),
                startRotation(startRotation), endRotation(endRotation) {}

            virtual void execute() override;
            virtual void undo() override;

            InspectorSystem *inspectorSys;
            EntitySystem *ecsRef;
            _unique_id entityId;
            RotationHandle handle;
            float startRotation, endRotation;
        };

        struct CreateInspectorEntityEvent
        {
            template <typename Func>
            CreateInspectorEntityEvent(Func callback)
            {
                this->callback = callback;
            }

            CreateInspectorEntityEvent(std::function<EntityRef(EntitySystem *)> callback) : callback(callback) {}
            CreateInspectorEntityEvent(const CreateInspectorEntityEvent& other) : callback(other.callback) {}

            CreateInspectorEntityEvent& operator=(const CreateInspectorEntityEvent& other)
            {
                callback = other.callback;

                return *this;
            }

            std::function<EntityRef(EntitySystem *)> callback;
        };

        struct InspectorSystem : public System<Listener<InspectEvent>, Listener<StandardEvent>, Listener<NewSceneLoaded>, QueuedListener<EntityChangedEvent>, QueuedListener<EndDragging>, QueuedListener<EndResize>, QueuedListener<EndRotation>, Listener<ConfiguredKeyEvent<EditorKeyConfig>>, Listener<EditorAttachComponent>, Listener<CreateInspectorEntityEvent>, Listener<ToggleInspectorEvent>, InitSys>
        {
            virtual void onEvent(const StandardEvent& event) override;

            virtual void init() override;

            CompRef<VerticalLayout> addNewText(const std::string& text, CompRef<VerticalLayout> currentView);

            void addNewAttribute(const std::string& text, std::string& value, CompRef<VerticalLayout> currentView);

            void printChildren(SerializedInfoHolder& parent, CompRef<VerticalLayout> currentView);

            virtual void onProcessEvent(const EntityChangedEvent& event) override;

            virtual void onEvent(const InspectEvent& event) override;

            virtual void onEvent(const NewSceneLoaded& event) override;

            virtual void onProcessEvent(const EndDragging& event) override
            {
                history.execute(std::make_unique<DraggingCommand>(this, ecsRef, event.startX, event.startY, event.endX, event.endY));
            }

            virtual void onProcessEvent(const EndResize& event) override
            {
                history.execute(std::make_unique<ResizeCommand>(this, ecsRef, event.entityId, event.handle, 
                    event.startWidth, event.startHeight, event.startX, event.startY,
                    event.endWidth, event.endHeight, event.endX, event.endY));
            }

            virtual void onProcessEvent(const EndRotation& event) override
            {
                history.execute(std::make_unique<RotationCommand>(this, ecsRef, event.entityId, event.handle, 
                    event.startRotation, event.endRotation));
            }

            virtual void onEvent(const ConfiguredKeyEvent<EditorKeyConfig>& e) override
            {
                if (e.value == EditorKeyConfig::Undo)
                {
                    LOG_INFO("Inspector", "Undo");

                    history.undo();
                }
                else if (e.value == EditorKeyConfig::Redo)
                {
                    LOG_INFO("Inspector", "Redo");

                    history.redo();
                }
            }

            virtual void onEvent(const EditorAttachComponent& event) override
            {
                history.execute(std::make_unique<AttachComponentCommand>(this, ecsRef, event.id, event.name));
            }

            virtual void onEvent(const CreateInspectorEntityEvent& event) override
            {
                history.execute(std::make_unique<CreateEntityCommand>(this, ecsRef, event.callback));
            }

            virtual void onEvent(const ToggleInspectorEvent&) override
            {
                toggleInspectorVisibility();
            }

            template <typename Comp>
            void registerCustomDrawer(std::function<void(InspectorSystem*, SerializedInfoHolder&, CompRef<VerticalLayout>)> drawer)
            {
                if constexpr(HasStaticName<Comp>::value)
                {
                    registerCustomDrawer(Comp::getType(), drawer);
                }
                else
                {
                    LOG_ERROR("InspectorSystem", "Can't register custom drawer for non-named component: " << typeid(Comp).name());
                }
            }

            void registerCustomDrawer(const std::string& type, std::function<void(InspectorSystem*, SerializedInfoHolder&, CompRef<VerticalLayout>)> drawer)
            {
                customDrawers.emplace(type, drawer);
            }

            // Todo maybe add a function to add special detach function for certain type of components
            // Or maybe move this to the component registry
            template <typename Comp, typename... Args>
            void registerAttachableComponent(const std::string& name, Args&&... args)
            {
                attachableComponentMap.emplace(name, [this, args...](EntityRef ent) {
                    ecsRef->template attach<Comp>(ent, args...);

                    ecsRef->sendEvent(EntityChangedEvent{ent->id});
                });
            }

            template <typename Comp, typename... Args>
            void registerAttachableComponent(Args&&... args)
            {
                const std::string& name = Comp::getType();

                registerAttachableComponent<Comp>(name, args...);
            }

            virtual void execute() override;

            void processEntityChanged(const EntityChangedEvent& event);

            void deserializeCurrentEntity();

            void toggleInspectorVisibility();

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

            std::map<std::string, std::function<void(InspectorSystem*, SerializedInfoHolder&, CompRef<VerticalLayout>)>> customDrawers;

            std::map<std::string, std::function<void(EntityRef)>> attachableComponentMap;
            bool showAttachMenu = false;
            std::vector<EntityRef> attachMenuItems;

            _unique_id currentId = 0;

            size_t nbEntity = 0;

            // Inspector visibility state
            bool isInspectorVisible = true;
            EntityRef inspectorPanel;
            EntityRef toggleButton;
            EntityRef toggleButtonText;
        };

        void defaultInspectWidget(InspectorSystem* sys, SerializedInfoHolder& parent, CompRef<VerticalLayout> currentView);

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