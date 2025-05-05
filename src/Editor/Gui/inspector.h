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
        // struct SerializedInfoHolder
        // {
        //     SerializedInfoHolder() {}
        //     SerializedInfoHolder(const std::string& className) : className(className) {}
        //     SerializedInfoHolder(const std::string& name, const std::string& type, const std::string& value) : name(name), type(type), value(value) {}
        //     SerializedInfoHolder(const SerializedInfoHolder& other) = delete;
        //     SerializedInfoHolder(SerializedInfoHolder&& other) : className(std::move(other.className)), name(std::move(other.name)), type(std::move(other.type)), value(std::move(other.value)), parent(std::move(other.parent)), children(std::move(other.children)) {}

        //     std::string className;
        //     std::string name;
        //     std::string type;
        //     std::string value;

        //     SerializedInfoHolder* parent;
        //     std::vector<SerializedInfoHolder> children;
        // };

        // struct InspectorArchive : public Archive
        // {
        //     /** Start the serialization process of a class */
        //     virtual void startSerialization(const std::string& className) override
        //     {
        //         auto& node = currentNode->children.emplace_back(className);

        //         node.name = lastAttributeName;
        //         lastAttributeName = "";

        //         node.parent = currentNode;

        //         currentNode = &node;
        //     }

        //     /** Start the serialization process of a class */
        //     virtual void endSerialization() override
        //     {
        //         currentNode = currentNode->parent;
        //     }

        //     /** Put an Attribute in the serialization process*/
        //     virtual void setAttribute(const std::string& value, const std::string& type = "") override
        //     {
        //         auto& attributeNode = currentNode->children.emplace_back(lastAttributeName, type, value);

        //         attributeNode.parent = currentNode;

        //         lastAttributeName = "";
        //     }

        //     virtual void setValueName(const std::string& name) override
        //     {
        //         lastAttributeName = name;
        //     }

        //     std::string lastAttributeName = "";

        //     SerializedInfoHolder mainNode;

        //     SerializedInfoHolder* currentNode = &mainNode;
        // };

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
    }

}