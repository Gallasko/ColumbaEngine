#include "inspector.h"

#include "Scene/scenemanager.h"

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

        void InspectorSystem::onEvent(const StandardEvent& event)
        {
            LOG_INFO("Inspector", "Received event named: " << event.name << ", return value: " << event.values.at("return"));

            auto id = event.values.at("id").get<size_t>();

            LOG_INFO("Inspector", "Replacing text: " << *inspectorText.at(id).valuePointer << " with: " << event.values.at("return").toString());

            *inspectorText.at(id).valuePointer = event.values.at("return").toString();

            needDeserialization = true;
        };

        void InspectorSystem::init()
        {
            addListenerToStandardEvent("InspectorChanges");

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
        }

        void InspectorSystem::addNewText(const std::string& text)
        {
            std::string textTemp = text;

            std::transform(textTemp.begin(), textTemp.end(), textTemp.begin(), ::toupper);

            auto sentence = makeTTFText(ecsRef, 1, 1, 1, "res/font/Inter/static/Inter_28pt-Bold.ttf", textTemp, 0.4);

            auto sentUi = sentence.get<PositionComponent>();

            view->addEntity(sentence.entity);
        }

        void InspectorSystem::addNewAttribute(const std::string& text, const std::string& type, std::string& value)
        {
            std::string textTemp = text;

            std::transform(textTemp.begin(), textTemp.end(), textTemp.begin(), ::toupper);

            auto sentence = makeTTFText(ecsRef, 1, 1, 1, "res/font/Inter/static/Inter_28pt-Bold.ttf", textTemp, 0.4);

            auto sentUi = sentence.get<PositionComponent>();
            auto sentAnchor = sentence.get<UiAnchor>();

            auto nbElements = inspectorText.size();

            auto valueInput = makeTTFTextInput(ecsRef, 0, 0, StandardEvent("InspectorChanges", "id", nbElements), "res/font/Inter/static/Inter_28pt-Light.ttf", {value}, 0.4);

            auto input = valueInput.get<TextInputComponent>();

            input->clearTextAfterEnter = false;

            auto valueInputUi = valueInput.get<PositionComponent>();
            auto valueInputAnchor = valueInput.get<UiAnchor>();

            // valueInputAnchor->setLeftAnchor(sentAnchor->right);

            inspectorText.emplace_back(text, &value, valueInput.entity.id);

            view->addEntity(sentence.entity);

            view->addEntity(valueInput.entity);
        }

        void InspectorSystem::printChildren(SerializedInfoHolder& parent, size_t indentLevel)
        {
            // If no class name then we got an attribute
            if (parent.className == "" and indentLevel > 2)
            {
                addNewAttribute(parent.name, parent.type, parent.value);
            }
            // We got a class name then it is a class ! So no type nor value
            else
            {
                if (indentLevel > 1)
                {
                    addNewText(parent.className);
                }
            }

            for (auto& child : parent.children)
            {
                printChildren(child, indentLevel + 1);
            }
        }


        void InspectorSystem::processEntityChanged(const EntityChangedEvent& event)
        {
            if (currentId == 0 or event.id != currentId)
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

                if (f.name == "x")
                    comp->setText(std::to_string(pos->x));
                else if (f.name == "y")
                    comp->setText(std::to_string(pos->y));
                else if (f.name == "z")
                    comp->setText(std::to_string(pos->z));
                else if (f.name == "width")
                    comp->setText(std::to_string(pos->width));
                else if (f.name == "height")
                    comp->setText(std::to_string(pos->height));
            }
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
        }

        void InspectorSystem::execute()
        {
            while (not eventQueue.empty())
            {
                const auto& event = eventQueue.front();
                processEntityChanged(event);

                eventQueue.pop();
            }

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

            printChildren(archive.mainNode, 0);

            eventRequested = false;
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

    }
}
