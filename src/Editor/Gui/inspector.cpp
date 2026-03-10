#include "inspector.h"

#include "Scene/scenemanager.h"

// UI includes moved from header
#include "UI/prefab.h"
#include "UI/namedanchor.h"
#include "UI/sizer.h"
#include "UI/textinput.h"
#include "UI/thememanager.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include "Systems/coresystems.h"

#include "Prefabs/foldablecard.h"

namespace pg
{

    namespace editor
    {

        namespace
        {
            static const char* const DOM = "Inspector";

            static void* getRawComponentPtr(EntitySystem* ecs, _unique_id entityId, const std::string& typeName)
            {
                auto& reg = ComponentSerializerRegistry::instance();

                if (reg.hasSerializer(typeName))
                {
                    auto fn = reg.getRetriever(typeName);
                    if (fn)
                        return fn(ecs, entityId);
                }

                auto* owner = ecs->getComponentRegistry()->retrieveStandardComponent(typeName);

                if (owner)
                    return owner->getComponent(entityId);

                return nullptr;
            }

            std::string toUpper(std::string str)
            {
                std::transform(str.begin(), str.end(), str.begin(), ::toupper);
                return str;
            }

            std::vector<std::string> splitComma(const std::string& str)
            {
                std::vector<std::string> result;
                std::istringstream ss(str);
                std::string item;

                while (std::getline(ss, item, ','))
                {
                    // Trim whitespace from item
                    item.erase(item.begin(), std::find_if(item.begin(), item.end(), [](unsigned char ch) { return not std::isspace(ch); }));
                    item.erase(std::find_if(item.rbegin(), item.rend(), [](unsigned char ch) { return not std::isspace(ch); }).base(), item.end());

                    if (not item.empty())
                        result.push_back(item);
                }

                return result;
            }
        }

        std::map<EditorKeyConfig, DefaultScancode> scancodeMap = {
            {EditorKeyConfig::Undo,   {"Undo", SDL_SCANCODE_Z, KMOD_CTRL}},
            {EditorKeyConfig::Redo,   {"Redo", SDL_SCANCODE_Y, KMOD_CTRL}},
        };

        void InspectorCommandHistory::execute(std::unique_ptr<InspectorCommands> command)
        {
            auto* sys = command->inspectorSys;
            sys->idStack.push_back(sys->currentId);

            command->execute();
            undoStack.push_back(std::move(command));
            redoStack.clear();
        }

        void InspectorCommandHistory::undo()
        {
            if (undoStack.empty())
                return;

            auto cmd = std::move(undoStack.back());
            undoStack.pop_back();
            cmd->undo();

            auto* sys = cmd->inspectorSys;
            sys->idStack.pop_back();

            redoStack.push_back(std::move(cmd));
        }

        void InspectorCommandHistory::redo()
        {
            if (redoStack.empty())
                return;

            auto cmd = std::move(redoStack.back());
            redoStack.pop_back();

            auto* sys = cmd->inspectorSys;
            sys->idStack.push_back(sys->currentId);

            cmd->execute();
            undoStack.push_back(std::move(cmd));
        }

        void DraggingCommand::execute()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);

            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();

            pos->setX(endX);
            pos->setY(endY);
        }

        void DraggingCommand::undo()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);

            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();

            pos->setX(startX);
            pos->setY(startY);
        }

        void AttachComponentCommand::execute()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);

            if (ent)
            {
                inspectorSys->attachableComponentMap[name](ent);

                // Todo only add the newly created component inspection at the end of the inspection layout,
                // currently this reload the whole entity information to the view which may not be intuitive for the user
                inspectorSys->eventRequested = true;
            }
        }

        void AttachComponentCommand::undo()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);

            if (ent)
            {
                ecsRef->detach(name, ent);

                inspectorSys->eventRequested = true;
            }
        }

        void CreateEntityCommand::execute()
        {
            auto ent = callback(ecsRef);

            ecsRef->attach<SceneElement>(ent);

            std::string name = "Entity_" + std::to_string(inspectorSys->nbEntity);

            inspectorSys->nbEntity++;

            ecsRef->attach<EntityName>(ent, name);

            ecsRef->attach<NamedUiAnchor>(ent);

            auto id = ent.id;

            inspectorSys->idStack.push_back(id);
            inspectorSys->currentId = id;

            // Need to redraw the inspector
            inspectorSys->eventRequested = true;
        }

        void CreateEntityCommand::undo()
        {
            ecsRef->removeEntity(inspectorSys->idStack.back());
            inspectorSys->idStack.pop_back();

            // If idstack.size == 1 then there was no previous entity
            if (inspectorSys->idStack.size() > 1)
                inspectorSys->onEvent(InspectEvent{inspectorSys->idStack.back()});
        }

        void ResizeCommand::execute()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);

            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();

            pos->setX(endX);
            pos->setY(endY);
            pos->setWidth(endWidth);
            pos->setHeight(endHeight);
        }

        void ResizeCommand::undo()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);

            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();

            pos->setX(startX);
            pos->setY(startY);
            pos->setWidth(startWidth);
            pos->setHeight(startHeight);
        }

        void RotationCommand::execute()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);
            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();
            pos->setRotation(endRotation);
        }

        void RotationCommand::undo()
        {
            auto id = inspectorSys->idStack.back();
            auto ent = ecsRef->getEntity(id);
            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();
            pos->setRotation(startRotation);
        }

        void InspectorSystem::onEvent(const StandardEvent& event)
        {
            if (event.name == "InspectorTextChanges")
            {
                LOG_INFO("Inspector", "Received event named: " << event.name << ", return value: " << event.values.at("return"));
            }

            auto key    = event.values.at("key").toString();     // "Type:prop:slot"
            auto newVal = event.values.at("return").toString();

            // Parse key
            auto sep1     = key.find(':');
            auto sep2     = key.rfind(':');
            std::string compType = key.substr(0, sep1);
            std::string propName = key.substr(sep1 + 1, sep2 - sep1 - 1);
            int slot             = std::stoi(key.substr(sep2 + 1));

            for (auto& binding : activeBindings)
            {
                if (binding.componentType != compType)
                    continue;

                if (binding.propertyName  != propName)
                    continue;

                auto& propMeta = ComponentProxyRegistry::instance().getMetadata(compType).properties.at(propName);
                if (not propMeta.sSetter)
                    break;

                if (binding.inputIds.size() == 1)
                {
                    // Scalar: just call with the new value directly
                    propMeta.sSetter(binding.componentPtr, newVal);
                }
                else
                {
                    // Vec3/Vec4: collect all current values, combine, call once
                    std::string combined;
                    for (size_t i = 0; i < binding.inputIds.size(); i++)
                    {
                        auto tc = ecsRef->getComponent<TextInputComponent>(binding.inputIds[i]);

                        std::string v = tc ? tc->text : "0";

                        if (static_cast<int>(i) == slot)
                            v = newVal;  // use the just-typed value

                        if (i > 0)
                            combined += ",";
                        combined += v;
                    }

                    propMeta.sSetter(binding.componentPtr, combined);
                }
                break;
            }
        };

        void InspectorSystem::init()
        {
            addListenerToStandardEvent("InspectorTextChanges");

            auto windowEnt = ecsRef->getEntity("__MainWindow");

            auto windowUi = windowEnt->get<UiAnchor>();

            auto actionEnt = ecsRef->getEntity("__ActionTab");

            auto actionUi = actionEnt->get<UiAnchor>();

            auto listView = makeVerticalLayout(ecsRef, 1, 1, 300, 1, true);

            listView.get<PositionComponent>()->setZ(1);
            auto listViewUi = listView.get<UiAnchor>();

            listViewUi->setTopAnchor(actionUi->bottom);
            listViewUi->setBottomAnchor(windowUi->bottom);
            listViewUi->setRightAnchor(windowUi->right);

            auto themeManager = ecsRef->getSystem<ThemeManager>();
            auto listViewBackground = makeEditorPanel(ecsRef, themeManager, 0, 0);

            auto listViewBackgroundUi = listViewBackground.get<UiAnchor>();
            listViewBackgroundUi->fillIn(listViewUi);

            mainView = listView.get<VerticalLayout>();

            auto compViewLayout = makeVerticalLayout(ecsRef, 1, 1, 300, 1);
            compView = compViewLayout.get<VerticalLayout>();
            compViewLayout.get<PositionComponent>()->setZ(2);

            auto compViewUi = compViewLayout.get<UiAnchor>();

            compViewUi->setLeftAnchor(listViewUi->left);
            compViewUi->setRightAnchor(listViewUi->right);

            mainView->addEntity(compViewLayout.entity);

            auto addViewLayout = makeVerticalLayout(ecsRef, 1, 1, 300, 1);
            addView = addViewLayout.get<VerticalLayout>();
            addViewLayout.get<PositionComponent>()->setZ(2);

            auto addViewUi = addViewLayout.get<UiAnchor>();

            addViewUi->setLeftAnchor(listViewUi->left);
            addViewUi->setRightAnchor(listViewUi->right);

            mainView->addEntity(addViewLayout.entity);

            // Store reference to the inspector panel for visibility toggling
            inspectorPanel = listView.entity;

            // Create toggle button on left side of inspector, vertically centered
            auto toggleButtonShape = makeEditorButton(ecsRef, themeManager, 20, 20);
            toggleButton = toggleButtonShape.entity;

            auto toggleButtonPos = toggleButtonShape.get<PositionComponent>();
            toggleButtonPos->setZ(3); // Above background and content

            auto toggleButtonUi = toggleButtonShape.get<UiAnchor>();
            toggleButtonUi->setLeftAnchor(listViewUi->left);
            toggleButtonUi->setVerticalCenter(listViewUi->verticalCenter);
            toggleButtonUi->setLeftMargin(-25); // Position outside the inspector panel

            // Create toggle button text
            auto toggleText = makeEditorText(ecsRef, themeManager, 0, 0, 12.0f, "light", "<", 0.5);
            toggleButtonText = toggleText.entity;

            auto toggleTextPos = toggleText.get<PositionComponent>();
            toggleTextPos->setZ(4); // Above button

            auto toggleTextUi = toggleText.get<UiAnchor>();
            toggleTextUi->centeredIn(toggleButtonUi);

            // Add click handler to toggle button
            ecsRef->attach<MouseLeftClickComponent>(toggleButton, makeCallable<ToggleInspectorEvent>());

            registerAttachableComponent<PositionComponent>();
            registerAttachableComponent<UiAnchor>();
            registerAttachableComponent<Simple2DObject>(Shape2D::Square);

            // Add Component

            auto row = makeHorizontalLayout(ecsRef, 0, 0, 300, 30, true);
            row.get<HorizontalLayout>()->fitToAxis = true;
            row.get<HorizontalLayout>()->spacing  = 8.f;

            // label
            auto label = makeEditorHeaderText(ecsRef, themeManager, 0, 0, 1, "bold", "Add Component", 0.4f);
            addView->addEntity(label.entity);
//
            // std::function<void(const OnMouseClick&)> f = [this](const OnMouseClick& ev){
                // if (ev.button == SDL_BUTTON_LEFT) showAttachMenu = not showAttachMenu;
            // };

            // hook its click
            // ecsRef->attach<OnEventComponent>(label.entity, f);

            addView->addEntity(row.entity);

            // if (showAttachMenu)
            // {
                // clean up from last frame
            for (auto e : attachMenuItems)
                ecsRef->removeEntity(e);

            attachMenuItems.clear();

            for (const auto& pair : attachableComponentMap)
            {
                const auto& name = pair.first;

                auto item = makeEditorText(ecsRef, themeManager, 0, 0, 1, "light", name, 0.35f);
                // indent it a bit
                // item.get<PositionComponent>()->setX(item.get<PositionComponent>()->x + 20.f);

                // clicking this line attaches that component

                ecsRef->attach<MouseLeftClickComponent>(item.entity, makeCallable<EditorAttachComponent>(name, currentId));

                addView->addEntity(item.entity);
                attachMenuItems.push_back(item.entity);
            }
            // }
        }

        CompRef<VerticalLayout> InspectorSystem::addNewText(const std::string& text, CompRef<VerticalLayout> currentView)
        {
            auto fold = makeFoldableCard(ecsRef, toUpper(text));

            currentView->addEntity(fold);

            return fold.get<VerticalLayout>();
        }

        void InspectorSystem::processEntityChanged(const EntityChangedEvent& event)
        {
            if (event.id == 0 or currentId == 0 or event.id != currentId)
                return;
        }

        void InspectorSystem::onProcessEvent(const EntityChangedEvent& event)
        {
            processEntityChanged(event);
        }

        void InspectorSystem::onEvent(const InspectEvent& event)
        {
            if (currentId != event.id)
            {
                currentId = event.id;
                eventRequested = true;
            }
        }

        void InspectorSystem::onEvent(const NewSceneLoaded&)
        {
            idStack = {};
            needClear = true;

            ecsRef->sendEvent(ReRendererAll{});
        }

        void InspectorSystem::execute()
        {
            if (needClear)
            {
                hideAllPanels();
                activeBindings.clear();

                needClear = false;
            }
            else if (not eventRequested)
            {
                return;
            }

            hideAllPanels();
            activeBindings.clear();

            currentEnt = ecsRef->getEntity(currentId);

            LOG_INFO("Inspector", "Working on entity: " << currentId);

            for (const auto& compRef : currentEnt->componentList)
            {
                auto typeName = ecsRef->getComponentRegistry()->getComponentTypeName(compRef.getId());
                if (not ComponentProxyRegistry::instance().hasMetadata(typeName))
                    continue;

                void* ptr = getRawComponentPtr(ecsRef, currentId, typeName);
                if (not ptr)
                    continue;

                showPanel(typeName, ptr);
            }

            eventRequested = false;
        }

        _unique_id InspectorWidgets::makeScalarInput(EntitySystem* ecs, BaseLayout* parentLayout, const std::string& labelText, const std::string& key, const std::string& baseValue)
        {
            // Horizontal row
            auto row = makeHorizontalLayout(ecs, 0, 0, 300, 30, true);
            auto rowAnchor = row.get<UiAnchor>();
            auto rowView = row.get<HorizontalLayout>();

            rowAnchor->setWidthConstrain(PosConstrain{parentLayout->id, AnchorType::Width});

            rowView->spacing = 10;
            rowView->fitToAxis = true;

            // Label
            auto themeManager = ecs->getSystem<ThemeManager>();
            auto labelEnt = makeEditorSecondaryText(ecs, themeManager, 0, 0, 1, "bold", toUpper(labelText), 0.4f);
            // auto labelPos = labelEnt.get<PositionComponent>();
            rowView->addEntity(labelEnt.entity);

            auto prefabEnt = makeAnchoredPrefab(ecs, 0, 0, 1);
            // auto prefabAnchor = prefabEnt.get<UiAnchor>();
            auto prefab = prefabEnt.get<Prefab>();

            // Text input
            auto background = makeEditorInputBackground(ecs, themeManager, 140, 0);
            // auto backgroundPos = background.get<PositionComponent>();
            auto backgroundAnchor = background.get<UiAnchor>();

            prefab->setMainEntity(background.entity);

            auto inputEnt = makeTTFTextInput(ecs, 0, 0, StandardEvent("InspectorTextChanges", "key", key), "light", { baseValue }, 0.4f);
            auto input = inputEnt.get<TextInputComponent>();
            auto inputAnchor = inputEnt.get<UiAnchor>();

            input->clearTextAfterEnter = false;

            backgroundAnchor->setHeightConstrain(PosConstrain{inputEnt.entity.id, AnchorType::Height, PosOpType::Add, 4.f});

            inputAnchor->setTopAnchor(backgroundAnchor->top);
            inputAnchor->setTopMargin(2.f);
            inputAnchor->setLeftAnchor(backgroundAnchor->left);
            inputAnchor->setLeftMargin(2.f);
            inputAnchor->setRightAnchor(backgroundAnchor->right);
            inputAnchor->setRightMargin(2.f);
            inputAnchor->setZConstrain(PosConstrain{background.entity.id, AnchorType::Z, PosOpType::Add, 1.f});

            prefab->addToPrefab(inputEnt.entity);
            rowView->addEntity(prefabEnt.entity);

            // Add the row into the parent vertical layout
            parentLayout->addEntity(row.entity);

            return inputEnt.entity.id;
        }

        std::array<_unique_id, 3> InspectorWidgets::makeVec3Input(EntitySystem* ecs, BaseLayout* parentLayout, const std::string& labelText, const std::string& baseKey, const std::string& baseValue)
        {
            auto themeManager = ecs->getSystem<ThemeManager>();

            // Top label: property name
            auto nameLabel = makeEditorSecondaryText(ecs, themeManager, 0, 0, 1, "bold", toUpper(labelText), 0.4f);
            parentLayout->addEntity(nameLabel.entity);

            // Row of X / Y / Z axis labels
            auto labelRow = makeHorizontalLayout(ecs, 0, 0, 300, 20, true);
            auto labelRowAnchor = labelRow.get<UiAnchor>();
            auto labelRowView = labelRow.get<HorizontalLayout>();
            labelRowAnchor->setWidthConstrain(PosConstrain{parentLayout->id, AnchorType::Width});
            labelRowView->spaced = true;

            const char* axisNames[3] = {"X", "Y", "Z"};
            for (int i = 0; i < 3; i++)
            {
                auto axisLabel = makeEditorSecondaryText(ecs, themeManager, 0, 0, 1, "bold", axisNames[i], 0.4f);
                labelRowView->addEntity(axisLabel.entity);
            }
            parentLayout->addEntity(labelRow.entity);

            // Row of 3 text inputs
            auto inputRow = makeHorizontalLayout(ecs, 0, 0, 300, 30, true);
            auto inputRowAnchor = inputRow.get<UiAnchor>();
            auto inputRowView = inputRow.get<HorizontalLayout>();
            inputRowAnchor->setWidthConstrain(PosConstrain{parentLayout->id, AnchorType::Width});
            inputRowView->spacing = 10;
            inputRowView->fitToAxis = true;

            std::array<_unique_id, 3> ids{};

            for (int i = 0; i < 3; i++)
            {
                std::string key = baseKey + ":" + std::to_string(i);

                auto prefabEnt = makeAnchoredPrefab(ecs, 0, 0, 1);
                auto prefab = prefabEnt.get<Prefab>();

                auto background = makeEditorInputBackground(ecs, themeManager, 90, 0);
                auto backgroundAnchor = background.get<UiAnchor>();
                prefab->setMainEntity(background.entity);

                auto inputEnt = makeTTFTextInput(ecs, 0, 0, StandardEvent("InspectorTextChanges", "key", key), "light", { baseValue }, 0.4f);
                auto input = inputEnt.get<TextInputComponent>();
                auto inputAnchor = inputEnt.get<UiAnchor>();

                input->clearTextAfterEnter = false;

                backgroundAnchor->setHeightConstrain(PosConstrain{inputEnt.entity.id, AnchorType::Height, PosOpType::Add, 4.f});

                inputAnchor->setTopAnchor(backgroundAnchor->top);
                inputAnchor->setTopMargin(2.f);
                inputAnchor->setLeftAnchor(backgroundAnchor->left);
                inputAnchor->setLeftMargin(2.f);
                inputAnchor->setRightAnchor(backgroundAnchor->right);
                inputAnchor->setRightMargin(2.f);
                inputAnchor->setZConstrain(PosConstrain{background.entity.id, AnchorType::Z, PosOpType::Add, 1.f});

                prefab->addToPrefab(inputEnt.entity);
                inputRowView->addEntity(prefabEnt.entity);

                ids[i] = inputEnt.entity.id;
            }

            parentLayout->addEntity(inputRow.entity);

            return ids;
        }

        void InspectorSystem::toggleInspectorVisibility()
        {
            if (inspectorPanel.empty() or toggleButtonText.empty())
                return;

            isInspectorVisible = not isInspectorVisible;

            auto panelPos = inspectorPanel.get<PositionComponent>();
            auto buttonTextComp = ecsRef->getComponent<TTFText>(toggleButtonText.id);
            auto toggleButtonUi = toggleButton.get<UiAnchor>();

            if (not panelPos or not buttonTextComp or not toggleButtonUi)
                return;

            auto windowEnt = ecsRef->getEntity("__MainWindow");
            auto windowUi = windowEnt->get<UiAnchor>();
            auto inspectorUi = inspectorPanel.get<UiAnchor>();

            if (not windowUi or not inspectorUi)
                return;

            if (isInspectorVisible)
            {
                // Show: restore inspector panel and reposition button to inspector left
                panelPos->setVisibility(true);
                buttonTextComp->text = "<";

                // Reposition button to left of inspector panel
                toggleButtonUi->clearRightAnchor();
                toggleButtonUi->setLeftAnchor(inspectorUi->left);
                toggleButtonUi->setVerticalCenter(inspectorUi->verticalCenter);
                toggleButtonUi->setLeftMargin(-25);

                LOG_INFO("Inspector", "Inspector panel shown");
            }
            else
            {
                // Hide: make panel invisible and reposition button to main window right edge
                panelPos->setVisibility(false);
                buttonTextComp->text = ">";

                // Reposition button to right edge of main window
                toggleButtonUi->clearLeftAnchor();
                toggleButtonUi->setRightAnchor(windowUi->right);
                toggleButtonUi->setVerticalCenter(windowUi->verticalCenter);
                toggleButtonUi->setRightMargin(5);

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

                LOG_INFO("Inspector", "Inspector panel hidden, button moved to window edge");
            }
        }

        CompList<PositionComponent, Prefab, UiAnchor, VerticalLayout> InspectorSystem::getOrBuildPanel(const std::string& typeName)
        {
            auto& panel = componentPanels[typeName];

            // If already initialized, return the existing panel prefab
            if (panel.initialized)
            {
                return CompList<PositionComponent, Prefab, UiAnchor, VerticalLayout>(
                    panel.foldCard,
                    panel.foldCard.get<PositionComponent>(),
                    panel.foldCard.get<Prefab>(),
                    panel.foldCard.get<UiAnchor>(),
                    panel.foldCard.get<VerticalLayout>()
                );
            }

            panel.typeName = typeName;

            // foldable card, added to view once, starts hidden
            auto fold = makeFoldableCard(ecsRef, toUpper(typeName));
            compView->addEntity(fold);
            panel.foldCard = fold.entity;
            panel.layout   = fold.get<VerticalLayout>();
            fold.get<PositionComponent>()->setVisibility(false);

            // check for custom drawer (takes over whole panel)
            // auto customIt = customDrawers.find(typeName);
            // if (customIt != customDrawers.end())
            // {
            //     customIt->second(this, ComponentProxyRegistry::instance().getMetadata(typeName), panel.layout);
            //     panel.initialized = true;
            //     return fold;
            // }

            auto& meta = ComponentProxyRegistry::instance().getMetadata(typeName);

            for (auto& [propName, propMeta] : meta.properties)
            {
                InspectorPropertyWidget widget;
                widget.propertyName = propName;
                widget.type = propMeta.type;

                auto name = propName;

                auto key = [&](int slot) {
                    return typeName + ":" + name + ":" + std::to_string(slot);
                };

                switch (propMeta.type)
                {
                case PropertyType::Vector3D:
                {
                    auto table = InspectorWidgets::makeVec3Input(ecsRef, panel.layout, propName, typeName + ":" + name, "0");

                    for (int i = 0; i < 3; i++)
                        widget.inputIds.push_back(table[i]);

                    break;
                }

                case PropertyType::Vector4D:
                    for (int i = 0; i < 4; i++)
                        widget.inputIds.push_back(InspectorWidgets::makeScalarInput(ecsRef, panel.layout,
                            propName + ( i == 0 ? " X" : ( i == 1 ? " Y" : ( i == 2 ? " Z" : " W" ) ) ), key(i), "0"));
                    break;

                default:   // Float, Double, Int, Bool, String, UnsignedInt, UniqueId
                    widget.inputIds.push_back(InspectorWidgets::makeScalarInput(ecsRef, panel.layout,
                        propName, key(0), "0"));
                    break;
                }

                panel.widgets.push_back(std::move(widget));
            }

            panel.initialized = true;

            return fold;
        }

        void InspectorSystem::showPanel(const std::string& typeName, void* componentPtr)
        {
            // Get or build the panel, ensuring it's fully initialized
            auto fold = getOrBuildPanel(typeName);

            auto& panel = componentPanels[typeName];
            fold.get<PositionComponent>()->setVisibility(true);

            auto& meta = ComponentProxyRegistry::instance().getMetadata(typeName);

            for (const auto& widget : panel.widgets)
            {
                ActiveBinding binding;
                binding.componentType = typeName;
                binding.propertyName  = widget.propertyName;
                binding.type          = widget.type;
                binding.inputIds      = widget.inputIds;
                binding.componentPtr  = componentPtr;
                activeBindings.push_back(binding);

                // Refresh displayed value(s)
                auto& propMeta = meta.properties.at(widget.propertyName);
                if (propMeta.sGetter)
                {
                    std::string val = propMeta.sGetter(componentPtr);
                    // For scalar: fill inputIds[0]
                    // For Vec3/Vec4: val is "x,y,z" — split by comma
                    auto parts = splitComma(val);
                    for (size_t i = 0; i < widget.inputIds.size(); i++)
                    {
                        auto textInput = ecsRef->getComponent<TextInputComponent>(widget.inputIds[i]);
                        if (textInput)
                            textInput->setText(i < parts.size() ? parts[i] : val);
                    }
                }
            }
        }

        void InspectorSystem::hideAllPanels()
        {
            for (auto& [name, panel] : componentPanels)
                if (panel.initialized)
                    panel.foldCard.get<PositionComponent>()->setVisibility(false);
        }

    }
}
