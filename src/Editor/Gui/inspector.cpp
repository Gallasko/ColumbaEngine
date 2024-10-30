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

            auto windowUi = windowEnt->get<UiComponent>();

            auto listView = makeListView(ecsRef, 1, 1, 300, 1);
            
            ecsRef->attach<Texture2DComponent>(listView.entity, "TabTexture");

            auto listViewUi = listView.get<UiComponent>();

            listViewUi->setTopAnchor(windowUi->top);
            listViewUi->setBottomAnchor(windowUi->bottom);
            listViewUi->setRightAnchor(windowUi->right);

            view = listView.get<ListView>();
        }

        void InspectorSystem::addNewText(const std::string& text)
        {
            std::string textTemp = text;

            std::transform(textTemp.begin(), textTemp.end(), textTemp.begin(), ::toupper);

            auto sentence = makeSentence(ecsRef, 1, 1, {textTemp});

            auto sentUi = sentence.get<UiComponent>();

            sentUi->setVisibility(false);
            sentUi->setZ(1);

            view->addEntity(sentUi);
        }

        void InspectorSystem::addNewAttribute(const std::string& text, const std::string& type, std::string& value)
        {
            std::string textTemp = text;

            std::transform(textTemp.begin(), textTemp.end(), textTemp.begin(), ::toupper);

            auto sentence = makeSentence(ecsRef, 1, 1, {textTemp});

            auto sentUi = sentence.get<UiComponent>();

            sentUi->setVisibility(false);
            sentUi->setZ(1);

            auto nbElements = inspectorText.size();

            auto valueInput = makeTextInput(ecsRef, 0, 0, StandardEvent("InspectorChanges", "id", nbElements), {value});

            valueInput.get<TextInputComponent>()->clearTextAfterEnter = false;

            auto valueInputUi = valueInput.get<UiComponent>();

            valueInputUi->setVisibility(false);
            valueInputUi->setZ(1);

            valueInputUi->setLeftAnchor(sentUi->right);

            inspectorText.emplace_back(&value, valueInputUi);

            view->addEntity(sentUi);

            view->addEntity(valueInputUi);
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

        void InspectorSystem::onEvent(const InspectEvent& event)
        {
            this->event = event;
            eventRequested = true;
        }

        void InspectorSystem::onEvent(const NewSceneLoaded&)
        {
            currentId = 0;
            needClear = true;
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
