#include "entitylist.h"
#include "UI/utils.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "Input/inputcomponent.h"

namespace pg
{
    namespace editor
    {
        void EntityListSystem::init()
        {
            registerGroup<EntityName>();

            auto windowEnt = ecsRef->getEntity("__MainWindow");
            auto windowUi = windowEnt->get<UiAnchor>();

            // Create entity list panel (200px wide, left-anchored)
            auto listPanel = makeVerticalLayout(ecsRef, 1, 1, 200, 1, true);
            listPanel.get<PositionComponent>()->setZ(1);
            
            auto listPanelUi = listPanel.get<UiAnchor>();
            listPanelUi->setTopAnchor(windowUi->top);
            listPanelUi->setBottomAnchor(windowUi->bottom);
            listPanelUi->setLeftAnchor(windowUi->left);

            // Add background texture
            auto listPanelBackground = makeUiTexture(ecsRef, 0, 0, "TabTexture");
            auto listPanelBackgroundUi = listPanelBackground.get<UiAnchor>();
            listPanelBackgroundUi->fillIn(listPanelUi);

            entityListPanel = listPanel.entity;
            entityListView = listPanel.get<VerticalLayout>();

            // Create toggle button on right side of entity list panel, vertically centered
            auto toggleButtonShape = makeUiSimple2DShape(ecsRef, Shape2D::Square, 20, 20, {200, 200, 200, 255});
            toggleButton = toggleButtonShape.entity;
            
            auto toggleButtonPos = toggleButtonShape.get<PositionComponent>();
            toggleButtonPos->setZ(3);
            
            auto toggleButtonUi = toggleButtonShape.get<UiAnchor>();
            toggleButtonUi->setRightAnchor(listPanelUi->right);
            toggleButtonUi->setVerticalCenter(listPanelUi->verticalCenter);
            toggleButtonUi->setRightMargin(-25); // Position outside the panel

            // Create toggle button text
            auto toggleText = makeTTFText(ecsRef, 0, 0, 12.0f, "light", "▶", 0.5);
            toggleButtonText = toggleText.entity;
            
            auto toggleTextPos = toggleText.get<PositionComponent>();
            toggleTextPos->setZ(4);
            
            auto toggleTextUi = toggleText.get<UiAnchor>();
            toggleTextUi->centeredIn(toggleButtonUi);

            // Add click handler to toggle button
            ecsRef->attach<MouseLeftClickComponent>(toggleButton, makeCallable<ToggleEntityListEvent>());

            createEntityListUI();
            refreshEntityList();
        }

        void EntityListSystem::createEntityListUI()
        {
            if (entityListView.empty())
                return;

            // Clear existing content
            entityListView->clear();

            // Title section
            auto titleEntity = makeTTFText(ecsRef, 5, 5, 14.0f, "bold", "Scene Entities", 0.6);
            titleText = titleEntity.entity;
            entityListView->addEntity(titleEntity.entity);

            // Entity count
            auto countEntity = makeTTFText(ecsRef, 5, 5, 12.0f, "light", "0 entities", 0.4);
            countText = countEntity.entity;
            entityListView->addEntity(countEntity.entity);

            // Spacer
            auto spacer = makeUiSimple2DShape(ecsRef, Shape2D::Square, 190, 5, {0, 0, 0, 0});
            entityListView->addEntity(spacer.entity);

            // Create scrollable list container for entities
            auto scrollContainer = makeVerticalLayout(ecsRef, 0, 0, 190, 400, true);
            scrollableList = scrollContainer.entity;
            entityListView->addEntity(scrollContainer.entity);
        }

        // Todo list refresh at the wrong time 
        void EntityListSystem::refreshEntityList()
        {
            sceneEntities.clear();

            // Collect all entities with SceneElement component
            for (const auto& elem : viewGroup<EntityName>())
            {
                auto info = createEntityInfo(elem->entity);
                sceneEntities.push_back(info);
            }

            // Update entity count display
            if (!countText.empty())
            {
                auto countComp = ecsRef->getComponent<TTFText>(countText.id);
                if (countComp)
                {
                    countComp->text = std::to_string(sceneEntities.size()) + " entities";
                    ecsRef->sendEvent(EntityChangedEvent{countText.id});
                }
            }

            // Rebuild entity list UI
            if (!scrollableList.empty())
            { 
                auto scrollView = scrollableList.get<VerticalLayout>();
                if (scrollView)
                {
                    scrollView->clear();

                    for (const auto& entityInfo : sceneEntities)
                    {
                        auto listItem = createEntityListItem(entityInfo);
                        scrollView->addEntity(listItem.entity);
                    }
                }
            }

            needsRefresh = false;
        }

        EntityInfo EntityListSystem::createEntityInfo(EntityRef entity)
        {
            EntityInfo info;
            info.entityId = entity.id;

            // Get entity name
            auto nameComp = entity.get<EntityName>();
            if (nameComp)
            {
                info.name = nameComp->name;
            }
            else
            {
                info.name = "Entity_" + std::to_string(entity.id);
            }

            // Determine entity type based on components
            if (entity.has<Simple2DObject>())
            {
                info.type = "Shape";
            }
            else if (entity.has<Texture2DComponent>())
            {
                info.type = "Texture";
            }
            else if (entity.has<TTFText>())
            {
                info.type = "Text";
            }
            else if (entity.has<VerticalLayout>())
            {
                info.type = "VLayout";
            }
            else if (entity.has<HorizontalLayout>())
            {
                info.type = "HLayout";
            }
            else
            {
                info.type = "Entity";
            }

            // Check visibility
            auto posComp = entity.get<PositionComponent>();
            if (posComp)
            {
                info.isVisible = posComp->isVisible();
            }

            // Check if selected
            info.isSelected = (selectedEntityId == entity.id);

            return info;
        }

        CompList<PositionComponent, UiAnchor, HorizontalLayout> EntityListSystem::createEntityListItem(const EntityInfo& info)
        {
            auto row = makeHorizontalLayout(ecsRef, 0, 0, 180, 20, false);
            
            // Set background color based on selection state
            constant::Vector4D backgroundColor = info.isSelected ? 
                constant::Vector4D{100, 150, 255, 100} :  // Blue for selected
                constant::Vector4D{0, 0, 0, 0};           // Transparent for normal

            if (backgroundColor.w > 0)
            {
                auto background = makeUiSimple2DShape(ecsRef, Shape2D::Square, 180, 20, backgroundColor);
                auto backgroundUi = background.get<UiAnchor>();
                backgroundUi->fillIn(row.get<UiAnchor>());
                background.get<PositionComponent>()->setZ(-1); // Behind other elements
            }

            auto rowLayout = row.get<HorizontalLayout>();
            rowLayout->spacing = 5;
            rowLayout->fitToAxis = false;

            // Visibility icon (clickable)
            auto visibilityIcon = makeTTFText(ecsRef, 0, 0, 10.0f, "light", info.isVisible ? "👁" : "◯", 0.4);
            ecsRef->attach<MouseLeftClickComponent>(visibilityIcon.entity, makeCallable<ToggleEntityVisibilityEvent>(info.entityId));
            rowLayout->addEntity(visibilityIcon.entity);

            // Entity name (clickable for selection)
            auto nameText = makeTTFText(ecsRef, 0, 0, 10.0f, "light", info.name, 0.4);
            ecsRef->attach<MouseLeftClickComponent>(nameText.entity, makeCallable<SelectEntityEvent>(info.entityId));
            rowLayout->addEntity(nameText.entity);

            // Entity type badge
            auto typeText = makeTTFText(ecsRef, 0, 0, 8.0f, "light", info.type, 0.3);
            constant::Vector4D typeColor = {150, 150, 150, 200};
            if (auto ttfComp = typeText.get<TTFText>())
            {
                ttfComp->colors = typeColor;
            }
            rowLayout->addEntity(typeText.entity);

            return row;
        }

        void EntityListSystem::onEvent(const EntityChangedEvent&)
        {
            needsRefresh = true;
        }

        void EntityListSystem::onEvent(const NewSceneLoaded&)
        {
            needsRefresh = true;
        }

        void EntityListSystem::onEvent(const SelectEntityEvent& event)
        {
            selectedEntityId = event.entityId;
            
            // Send inspect event to show in inspector panel
            ecsRef->sendEvent(InspectEvent{ecsRef->getEntity(event.entityId)});
            
            // Refresh list to show selection
            needsRefresh = true;

            LOG_INFO("EntityList", "Selected entity: " << event.entityId);
        }

        void EntityListSystem::onEvent(const ToggleEntityVisibilityEvent& event)
        {
            auto entity = ecsRef->getEntity(event.entityId);
            if (not entity)
                return;

            auto posComp = entity->get<PositionComponent>();
            if (posComp)
            {
                posComp->setVisibility(!posComp->isVisible());
                ecsRef->sendEvent(EntityChangedEvent{event.entityId});
                needsRefresh = true;

                LOG_INFO("EntityList", "Toggled visibility for entity: " << event.entityId);
            }
        }

        void EntityListSystem::onEvent(const RefreshEntityListEvent&)
        {
            refreshEntityList();
        }

        void EntityListSystem::onProcessEvent(const CreateElement&)
        {
            refreshEntityList();
        }

        void EntityListSystem::onEvent(const ToggleEntityListEvent&)
        {
            toggleEntityListVisibility();
        }

        void EntityListSystem::toggleEntityListVisibility()
        {
            if (entityListPanel.empty() || toggleButtonText.empty())
                return;

            isEntityListVisible = !isEntityListVisible;

            auto panelPos = entityListPanel.get<PositionComponent>();
            auto buttonTextComp = ecsRef->getComponent<TTFText>(toggleButtonText.id);
            auto toggleButtonUi = toggleButton.get<UiAnchor>();

            if (!panelPos || !buttonTextComp || !toggleButtonUi)
                return;

            auto windowEnt = ecsRef->getEntity("__MainWindow");
            auto windowUi = windowEnt->get<UiAnchor>();
            auto entityListUi = entityListPanel.get<UiAnchor>();

            if (!windowUi || !entityListUi)
                return;

            if (isEntityListVisible)
            {
                // Show: restore entity list panel and reposition button to panel right
                panelPos->setVisibility(true);
                buttonTextComp->text = "▶";
                
                // Reposition button to right of entity list panel
                toggleButtonUi->clearLeftAnchor();
                toggleButtonUi->setRightAnchor(entityListUi->right);
                toggleButtonUi->setVerticalCenter(entityListUi->verticalCenter);
                toggleButtonUi->setRightMargin(-25);
                
                LOG_INFO("EntityList", "Entity list panel shown");
            }
            else
            {
                // Hide: make panel invisible and reposition button to main window left edge
                panelPos->setVisibility(false);
                buttonTextComp->text = "◀";
                
                // Reposition button to left edge of main window
                toggleButtonUi->clearRightAnchor();
                toggleButtonUi->setLeftAnchor(windowUi->left);
                toggleButtonUi->setVerticalCenter(windowUi->verticalCenter);
                toggleButtonUi->setLeftMargin(5);
                
                // Keep toggle button visible
                auto buttonPos = toggleButton.get<PositionComponent>();
                if (buttonPos)
                {
                    buttonPos->setVisibility(true);
                }
                
                auto buttonTextPos = toggleButtonText.get<PositionComponent>();
                if (buttonTextPos)
                {
                    buttonTextPos->setVisibility(true);
                }
                
                LOG_INFO("EntityList", "Entity list panel hidden, button moved to window edge");
            }

            // Send update events to refresh display
            ecsRef->sendEvent(EntityChangedEvent{toggleButtonText.id});
            ecsRef->sendEvent(EntityChangedEvent{toggleButton.id});
            ecsRef->sendEvent(EntityChangedEvent{entityListPanel.id});
        }

        void EntityListSystem::updateEntitySelection(_unique_id selectedId)
        {
            selectedEntityId = selectedId;
            needsRefresh = true;
        }
    }
}