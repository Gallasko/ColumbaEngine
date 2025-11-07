#pragma once

#include "ECS/system.h"
#include "ECS/entitysystem.h"

#include "Scene/scenemanager.h"

#include "2D/position.h"

#include "Input/inputcomponent.h"

#include "Systems/coresystems.h"

#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/ttftext.h"

#include "inspector.h"
#include "contextmenu.h"

namespace pg
{
    namespace editor
    {
        struct SelectEntityEvent 
        { 
            SelectEntityEvent(_unique_id id = 0) : entityId(id) {}
            _unique_id entityId = 0;
        };

        struct ToggleEntityVisibilityEvent 
        { 
            ToggleEntityVisibilityEvent(_unique_id id = 0) : entityId(id) {}
            _unique_id entityId = 0;
        };

        struct RefreshEntityListEvent {};

        struct ToggleEntityListEvent {};

        struct EntityInfo 
        {
            _unique_id entityId = 0;
            std::string name;
            std::string type;
            bool isVisible = true;
            bool isSelected = false;
        };

        struct EntityListSystem : public System<Ref<SceneElement>, Ref<EntityName>, Ref<PositionComponent>,
            Listener<EntityChangedEvent>, Listener<NewSceneLoaded>, Listener<SelectEntityEvent>,
            Listener<ToggleEntityVisibilityEvent>, Listener<RefreshEntityListEvent>,
            QueuedListener<CreateElement>, Listener<ToggleEntityListEvent>,
            InitSys>
        {
            virtual void init() override;

            virtual void onEvent(const EntityChangedEvent& event) override;
            virtual void onEvent(const NewSceneLoaded& event) override;
            virtual void onEvent(const SelectEntityEvent& event) override;
            virtual void onEvent(const ToggleEntityVisibilityEvent& event) override;
            virtual void onEvent(const RefreshEntityListEvent& event) override;
            virtual void onProcessEvent(const CreateElement& event) override;
            virtual void onEvent(const ToggleEntityListEvent& event) override;

            void refreshEntityList();
            void createEntityListUI();
            void updateEntitySelection(_unique_id selectedId);
            void toggleEntityListVisibility();
            
            EntityInfo createEntityInfo(EntityRef entity);
            CompList<PositionComponent, UiAnchor, HorizontalLayout> createEntityListItem(const EntityInfo& info);

            std::vector<EntityInfo> sceneEntities;
            bool needsRefresh = true;

            // UI Components
            EntityRef entityListPanel;
            CompRef<VerticalLayout> entityListView;
            EntityRef titleText;
            EntityRef countText;
            EntityRef scrollableList;
            
            // Toggle functionality
            bool isEntityListVisible = true;
            EntityRef toggleButton;
            EntityRef toggleButtonText;

            _unique_id selectedEntityId = 0;
        };
    }
}