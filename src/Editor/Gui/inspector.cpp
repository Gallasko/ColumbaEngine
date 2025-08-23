#include "inspector.h"

#include "Scene/scenemanager.h"

#include "UI/prefab.h"
#include "UI/namedanchor.h"
#include "2D/simple2dobject.h"

#include "Systems/coresystems.h"

#include "Prefabs/foldablecard.h"

namespace pg
{

    namespace editor
    {

        namespace
        {
            static const char* const DOM = "Inspector";

            void deserializeCurrentEntityHelper(UnserializedObject& holder, const SerializedInfoHolder& parent)
            {
                for (const auto& child : parent.children)
                {
                    if (child.className == "")
                    {
                        std::string str;

                        if (strcmp(ARCHIVEVERSION, "1.0.0") == 0)
                            str = ATTRIBUTECONST + " " + child.type + " {" + child.value + "}";

                        UnserializedObject attribute(str, child.name, false);

                        holder.children.push_back(attribute);
                    }
                    else
                    {
                        UnserializedObject klass(child.name, child.className, std::string(""));

                        deserializeCurrentEntityHelper(klass, child);

                        holder.children.push_back(klass);
                    }
                }
            }
        }

        std::map<EditorKeyConfig, DefaultScancode> scancodeMap = {
            {EditorKeyConfig::Undo,   {"Undo", SDL_SCANCODE_Z, KMOD_CTRL}},
            {EditorKeyConfig::Redo,   {"Redo", SDL_SCANCODE_Y, KMOD_CTRL}},
            };

        void DraggingCommand::execute()
        {
            id = inspectorSys->currentId;
            auto ent = ecsRef->getEntity(id);

            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();

            pos->setX(endX);
            pos->setY(endY);
        }

        void DraggingCommand::undo()
        {
            auto ent = ecsRef->getEntity(id);

            if (not ent or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();

            pos->setX(startX);
            pos->setY(startY);
        }

        void AttachComponentCommand::execute()
        {
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
            auto ent = ecsRef->getEntity(id);

            if (ent)
            {
                ecsRef->detach(name, ent);

                inspectorSys->eventRequested = true;
            }
        }

        void CreateEntityCommand::execute()
        {
            lastFocusedId = inspectorSys->currentId;

            auto ent = callback(ecsRef);

            ecsRef->attach<SceneElement>(ent);

            std::string name = "Entity_" + std::to_string(inspectorSys->nbEntity);

            inspectorSys->nbEntity++;

            ecsRef->attach<EntityName>(ent, name);

            ecsRef->attach<NamedUiAnchor>(ent);

            id = ent.id;

            inspectorSys->currentId = id;

            // Need to redraw the inspector
            inspectorSys->event.entity = ent;
            inspectorSys->eventRequested = true;
        }

        void CreateEntityCommand::undo()
        {
            inspectorSys->currentId = lastFocusedId;

            // inspectorSys->eventRequested = true;

            ecsRef->removeEntity(id);
        }

        void InspectorSystem::onEvent(const StandardEvent& event)
        {
            if (event.name == "InspectorTextChanges")
            {
                LOG_INFO("Inspector", "Received event named: " << event.name << ", return value: " << event.values.at("return"));

                auto id = event.values.at("id").get<size_t>();

                LOG_INFO("Inspector", "Replacing text: " << *inspectorText.at(id).valuePointer << " with: " << event.values.at("return").toString());

                *inspectorText.at(id).valuePointer = event.values.at("return").toString();

                needDeserialization = true;
            }
        };

        void InspectorSystem::init()
        {
            addListenerToStandardEvent("InspectorTextChanges");

            auto windowEnt = ecsRef->getEntity("__MainWindow");

            auto windowUi = windowEnt->get<UiAnchor>();

            auto listView = makeVerticalLayout(ecsRef, 1, 1, 300, 1, true);

            listView.get<PositionComponent>()->setZ(1);
            auto listViewUi = listView.get<UiAnchor>();

            listViewUi->setTopAnchor(windowUi->top);
            listViewUi->setBottomAnchor(windowUi->bottom);
            listViewUi->setRightAnchor(windowUi->right);

            auto listViewBackground = makeUiTexture(ecsRef, 0, 0, "TabTexture");

            auto listViewBackgroundUi = listViewBackground.get<UiAnchor>();
            listViewBackgroundUi->fillIn(listViewUi);

            view = listView.get<VerticalLayout>();

            registerAttachableComponent<PositionComponent>();
            registerAttachableComponent<UiAnchor>();
            registerAttachableComponent<Simple2DObject>(Shape2D::Square);
        }

        CompRef<VerticalLayout> InspectorSystem::addNewText(const std::string& text, CompRef<VerticalLayout> currentView)
        {
            std::string textTemp = text;

            std::transform(textTemp.begin(), textTemp.end(), textTemp.begin(), ::toupper);

            auto fold = makeFoldableCard(ecsRef, textTemp);

            currentView->addEntity(fold);

            return fold.get<VerticalLayout>();
        }

        // Todo to remove type
        void InspectorSystem::addNewAttribute(const std::string& text, std::string& value, CompRef<VerticalLayout> currentView)
        {
            InspectorWidgets::makeLabeledTextInput(ecsRef, currentView, text, value, this);
        }

        void InspectorSystem::printChildren(SerializedInfoHolder& parent, CompRef<VerticalLayout> currentView)
        {
            auto it = customDrawers.find(parent.className);

            if (it != customDrawers.end())
            {
                it->second(this, parent, currentView);
                return;
            }
            else
            {
                defaultInspectWidget(this, parent, currentView);
            }
        }


        void InspectorSystem::processEntityChanged(const EntityChangedEvent& event)
        {
            if (event.id == 0 or currentId == 0 or event.id != currentId)
                return;

            auto pos = ecsRef->getComponent<PositionComponent>(currentId);
            if (not pos) return;

            // now update each field by name
            for (const auto& f : inspectorText)
            {
                if (not (f.name == "x" or f.name == "y" or f.name == "z" or f.name == "width" or f.name == "height"))
                    continue;

                auto comp = ecsRef->getComponent<TextInputComponent>(f.id);

                if (not comp)
                {
                    LOG_ERROR(DOM, "Component not found for id: " << f.id);
                    continue;
                }

                // compute the new value string
                std::string newVal;
                if      (f.name == "x")      newVal = std::to_string(pos->x);
                else if (f.name == "y")      newVal = std::to_string(pos->y);
                else if (f.name == "z")      newVal = std::to_string(pos->z);
                else if (f.name == "width")  newVal = std::to_string(pos->width);
                else /* height */            newVal = std::to_string(pos->height);

                // 1) update the visible text widget
                comp->setText(newVal);

                // 2) **also** write it back into your archive
                *f.valuePointer = newVal;
            }
        }

        void InspectorSystem::onProcessEvent(const EntityChangedEvent& event)
        {
            processEntityChanged(event);
        }

        void InspectorSystem::onEvent(const InspectEvent& event)
        {
            if (currentId != event.entity.id)
            {
                this->event = event;
                eventRequested = true;
            }
        }

        void InspectorSystem::onEvent(const NewSceneLoaded&)
        {
            currentId = 0;
            needClear = true;

            ecsRef->sendEvent(ReRendererAll{});
        }

        void InspectorSystem::execute()
        {
            if (needUpdateEntity)
            {
                ecsRef->sendEvent(EntityChangedEvent{currentId});
                needUpdateEntity = false;
            }

            if (needDeserialization and currentId != 0)
            {
                deserializeCurrentEntity();
                needDeserialization = false;
                needUpdateEntity = true;
            }

            if (eventRequested or needClear)
            {
                view->clear();
                inspectorText.clear();

                archive.mainNode.children.clear();

                ecsRef->sendEvent(SkipRenderPass{3});

                needClear = false;
            }

            if (not eventRequested)
                return;

            currentId = event.entity.id;

            serialize(archive, *event.entity.entity);

            for (auto& child : archive.mainNode.children)
            {
                printChildren(child, view);
            }

            eventRequested = false;

            auto row = makeHorizontalLayout(ecsRef, 0,0, 0,0);
            row.get<HorizontalLayout>()->fitToAxis = true;
            row.get<HorizontalLayout>()->spacing  = 8.f;

            // label
            auto label = makeTTFText(ecsRef, 0,0, 1, "bold", "Add Component", 0.4f);
            view->addEntity(label.entity);
//
            // std::function<void(const OnMouseClick&)> f = [this](const OnMouseClick& ev){
                // if (ev.button == SDL_BUTTON_LEFT) showAttachMenu = not showAttachMenu;
            // };

            // hook its click
            // ecsRef->attach<OnEventComponent>(label.entity, f);

            view->addEntity(row.entity);

            // if (showAttachMenu)
            // {
                // clean up from last frame
                for (auto e : attachMenuItems)
                    ecsRef->removeEntity(e);

                attachMenuItems.clear();

                for (const auto& pair : attachableComponentMap)
                {
                    const auto& name = pair.first;

                    auto item = makeTTFText(ecsRef, 0,0, 1, "light", name, 0.35f);
                    // indent it a bit
                    // item.get<PositionComponent>()->setX(item.get<PositionComponent>()->x + 20.f);

                    // clicking this line attaches that component

                    ecsRef->attach<MouseLeftClickComponent>(item.entity, makeCallable<EditorAttachComponent>(name, currentId));

                    view->addEntity(item.entity);
                    attachMenuItems.push_back(item.entity);
                }
            // }
        }

        void InspectorSystem::deserializeCurrentEntity()
        {
            UnserializedObject obj;

            deserializeCurrentEntityHelper(obj, archive.mainNode);

            if (obj.children.size() < 1)
            {
                LOG_ERROR(DOM, "Entity root node has no children, should never happen !");

                return;
            }

            auto entity = ecsRef->getEntity(currentId);

            // obj.children[0] is the root node of the entity's components
            for (const auto& child : obj.children[0].children)
            {
                if (child.isClassObject())
                {
                    ecsRef->deserializeComponent(entity, child);
                }
            }
        }

        void defaultInspectWidget(InspectorSystem* sys, SerializedInfoHolder& parent, CompRef<VerticalLayout> currentView)
        {
            // If no class name then we got an attribute
            if (parent.className == "")
            {
                sys->addNewAttribute(parent.name, parent.value, currentView);
            }
            // We got a class name then it is a class ! So no type nor value
            else
            {
                auto name = parent.name == "" ? parent.className : parent.name;
                currentView = sys->addNewText(name, currentView);
            }

            for (auto& child : parent.children)
            {
                sys->printChildren(child, currentView);
            }
        }

        void InspectorWidgets::makeLabeledTextInput(EntitySystem* ecs, BaseLayout* parentLayout, const std::string& labelText, std::string& boundValue, InspectorSystem* sys)
        {
            std::string textTemp = labelText;

            std::transform(textTemp.begin(), textTemp.end(), textTemp.begin(), ::toupper);

            // Horizontal row
            auto row = makeHorizontalLayout(ecs, 0, 0, 0, 0);
            auto rowAnchor = row.get<UiAnchor>();
            auto rowView = row.get<HorizontalLayout>();

            rowAnchor->setWidthConstrain(PosConstrain{parentLayout->id, AnchorType::Width});

            rowView->spacing = 10;
            rowView->fitToAxis = true;

            // Label
            auto labelEnt = makeTTFText(ecs, 0, 0, 1, "bold", textTemp, 0.4f);
            // auto labelPos = labelEnt.get<PositionComponent>();
            rowView->addEntity(labelEnt.entity);

            auto prefabEnt = makeAnchoredPrefab(ecs, 0, 0, 1);
            // auto prefabAnchor = prefabEnt.get<UiAnchor>();
            auto prefab = prefabEnt.get<Prefab>();

            // Text input
            auto background = makeUiSimple2DShape(ecs, Shape2D::Square, 140, 0, {55.f, 55.f, 55.f, 255.f});
            // auto backgroundPos = background.get<PositionComponent>();
            auto backgroundAnchor = background.get<UiAnchor>();

            prefab->setMainEntity(background.entity);

            auto inputEnt = makeTTFTextInput(ecs, 0, 0, StandardEvent("InspectorTextChanges", "id", sys->inspectorText.size()), "light", { boundValue }, 0.4f);
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

            sys->inspectorText.emplace_back(labelText, &boundValue, inputEnt.entity.id);

            // Add the row into the parent vertical layout
            parentLayout->addEntity(row.entity);
        }
    }
}
