#pragma once

#include <unordered_map>

#include "ECS/system.h"

#include "Memory/elementtype.h"

namespace pg
{
    struct SaveData
    {
        SaveData() {}
        SaveData(const SaveData& other) : elements(other.elements) {}

        SaveData& operator=(const SaveData& other)
        {
            elements = other.elements;

            return *this;
        }

        inline static std::string getType() { return "SaveData"; }

        std::unordered_map<std::string, ElementType> elements;
    };

    template<>
    void serialize(Archive& archive, const SaveData& config);

    template <>
    SaveData deserialize(const UnserializedObject& serializedString);

    struct SaveFile
    {
        SaveFile() {}
        SaveFile(const SaveFile& other) : file(other.file), data(other.data) {}

        SaveFile& operator=(const SaveFile& other)
        {
            file = other.file;
            data = other.data;

            return *this;
        }

        TextFile file;

        SaveData data;
    };

    struct SaveElementEvent
    {
        template<typename Type>
        SaveElementEvent(const std::string& name, const Type& element) : SaveElementEvent(name, ElementType{element}) {}

        SaveElementEvent(const std::string& name, const ElementType& element) : name(name), element(element) {}
        SaveElementEvent(const SaveElementEvent& other) : name(other.name), element(other.element) {}

        SaveElementEvent& operator=(const SaveElementEvent& other)
        {
            name = other.name;
            element = other.element;

            return *this;
        }

        std::string name;
        ElementType element;
    };

    class SaveManager : public System<QueuedListener<SaveElementEvent>, StoragePolicy>
    {
    public:
        SaveManager(const std::string& savePath);

        virtual std::string getSystemName() const override { return "Save System"; }

        virtual void execute() override;

        virtual void onProcessEvent(const SaveElementEvent& event) override;

        ElementType getValue(const std::string& id) const;

    private:
        void loadSave(const std::string& savePath);

    private:
        void save();

    private:
        SaveFile currentSave;

        bool needSave = false;

        Serializer serializer;
    };
}