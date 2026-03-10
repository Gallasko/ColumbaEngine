#pragma once

#include <array>

#include "serialization.h"
#include "ECS/system.h"
#include "UI/sizer.h"
#include "UI/textinput.h"
#include "UI/prefab.h"
#include "Input/keyconfig.h"

#include "Compiler/ecsserialization.h"

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

        struct InspectEvent { _unique_id id; };

        struct ValueChanged { std::string valueName; std::string value; };

        struct InspectedText
        {
            InspectedText(const std::string& name, std::string *valuePointer, _unique_id id) : name(name), valuePointer(valuePointer), id(id) {}
            InspectedText(const InspectedText& rhs) : name(rhs.name), valuePointer(rhs.valuePointer), id(rhs.id) {}

            std::string name;
            std::string *valuePointer = nullptr;
            _unique_id id;
        };

        struct InspectorSystem;

        struct InspectorCommands
        {
            InspectorCommands(InspectorSystem* inspectorSys, EntitySystem* ecsRef) : inspectorSys(inspectorSys), ecsRef(ecsRef) {}

            InspectorSystem* inspectorSys;
            EntitySystem* ecsRef;

            virtual ~InspectorCommands() {}
            virtual void execute() = 0;
            virtual void undo() = 0;
        };

        class InspectorCommandHistory
        {
        public:
            InspectorCommandHistory() = default;

            void execute(std::unique_ptr<InspectorCommands> command);

            void undo();

            void redo();

        private:
            std::vector<std::unique_ptr<InspectorCommands>> undoStack;
            std::vector<std::unique_ptr<InspectorCommands>> redoStack;
        };

        struct DraggingCommand : public InspectorCommands
        {
            DraggingCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, float startX, float startY, float endX, float endY) :
                InspectorCommands(inspectorSys, ecsRef),
                startX(startX), startY(startY), endX(endX), endY(endY) {}

            virtual void execute() override;

            virtual void undo() override;

            float startX, startY;
            float endX, endY;
        };

        struct AttachComponentCommand : public InspectorCommands
        {
            AttachComponentCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, const std::string& name) :
                InspectorCommands(inspectorSys, ecsRef), name(name) {}

            virtual void execute() override;
            virtual void undo() override;

            std::string name;
        };

        struct CreateEntityCommand : public InspectorCommands
        {
            CreateEntityCommand(InspectorSystem *inspectorSys, EntitySystem *ecsRef, std::function<EntityRef(EntitySystem *)> callbackCreated) :
                InspectorCommands(inspectorSys, ecsRef), callback(callbackCreated) { }

            virtual void execute() override;
            virtual void undo() override;

            std::function<EntityRef(EntitySystem *)> callback;
        };

        struct ResizeCommand : public InspectorCommands
        {
            ResizeCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, ResizeHandle handle,
                          float startWidth, float startHeight, float startX, float startY,
                          float endWidth, float endHeight, float endX, float endY) :
                InspectorCommands(inspectorSys, ecsRef),
                handle(handle),
                startWidth(startWidth), startHeight(startHeight), startX(startX), startY(startY),
                endWidth(endWidth), endHeight(endHeight), endX(endX), endY(endY) {}

            virtual void execute() override;
            virtual void undo() override;

            ResizeHandle handle;
            float startWidth, startHeight, startX, startY;
            float endWidth, endHeight, endX, endY;
        };

        struct RotationCommand : public InspectorCommands
        {
            RotationCommand(InspectorSystem *inspectorSys, EntitySystem* ecsRef, RotationHandle handle,
                            float startRotation, float endRotation) :
                InspectorCommands(inspectorSys, ecsRef),
                handle(handle),startRotation(startRotation), endRotation(endRotation) {}

            virtual void execute() override;
            virtual void undo() override;

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

        struct InspectorPropertyWidget
        {
            std::string propertyName;
            PropertyType type;

            std::vector<_unique_id> inputIds;
        };

        struct InspectorComponentPanel
        {
            std::string typeName;

            EntityRef foldCard;                        // foldable card root
            CompRef<VerticalLayout> layout;            // inner layout

            std::vector<InspectorPropertyWidget> widgets;
            bool initialized = false;
        };

        struct ActiveBinding
        {
            std::string componentType;
            std::string propertyName;

            PropertyType type;

            std::vector<_unique_id> inputIds;

            void* componentPtr;
        };

        struct InspectorSystem : public System<Listener<InspectEvent>, Listener<StandardEvent>, Listener<NewSceneLoaded>, QueuedListener<EntityChangedEvent>, QueuedListener<EndDragging>, QueuedListener<EndResize>, QueuedListener<EndRotation>, Listener<ConfiguredKeyEvent<EditorKeyConfig>>, Listener<EditorAttachComponent>, Listener<CreateInspectorEntityEvent>, Listener<ToggleInspectorEvent>, InitSys>
        {
            virtual void onEvent(const StandardEvent& event) override;

            virtual void init() override;

            CompRef<VerticalLayout> addNewText(const std::string& text, CompRef<VerticalLayout> currentView);

            virtual void onProcessEvent(const EntityChangedEvent& event) override;

            virtual void onEvent(const InspectEvent& event) override;

            virtual void onEvent(const NewSceneLoaded& event) override;

            virtual void onProcessEvent(const EndDragging& event) override
            {
                history.execute(std::make_unique<DraggingCommand>(this, ecsRef, event.startX, event.startY, event.endX, event.endY));
            }

            virtual void onProcessEvent(const EndResize& event) override
            {
                history.execute(std::make_unique<ResizeCommand>(this, ecsRef, event.handle,
                    event.startWidth, event.startHeight, event.startX, event.startY,
                    event.endWidth, event.endHeight, event.endX, event.endY));
            }

            virtual void onProcessEvent(const EndRotation& event) override
            {
                history.execute(std::make_unique<RotationCommand>(this, ecsRef, event.handle,
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
                history.execute(std::make_unique<AttachComponentCommand>(this, ecsRef, event.name));
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

            void toggleInspectorVisibility();

            CompList<PositionComponent, Prefab, UiAnchor, VerticalLayout> getOrBuildPanel(const std::string& typeName);
            void showPanel (const std::string& typeName, void* componentPtr);
            void hideAllPanels();

            InspectorCommandHistory history;

            std::map<std::string, InspectorComponentPanel> componentPanels;
            std::vector<ActiveBinding> activeBindings;

            CompRef<VerticalLayout> mainView;
            CompRef<VerticalLayout> compView;
            CompRef<VerticalLayout> addView;

            bool eventRequested = false;

            bool needClear = false;

            std::map<std::string, std::function<void(InspectorSystem*, SerializedInfoHolder&, CompRef<VerticalLayout>)>> customDrawers;

            std::map<std::string, std::function<void(EntityRef)>> attachableComponentMap;
            bool showAttachMenu = false;
            std::vector<EntityRef> attachMenuItems;

            _unique_id currentId = 0;
            std::vector<_unique_id> idStack = {};
            EntityRef currentEnt;

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
            static _unique_id makeScalarInput(EntitySystem* ecs, BaseLayout* parentLayout, const std::string& labelText, const std::string& key, const std::string& baseValue);
            static std::array<_unique_id, 3> makeVec3Input(EntitySystem* ecs, BaseLayout* parentLayout, const std::string& labelText, const std::string& baseKey, const std::string& baseValue);
        };
    }

}