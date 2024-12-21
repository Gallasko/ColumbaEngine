#ifndef SERIALIZATION_H
#define SERIALIZATION_H

/**
 * @file serialization.h
 * @author Pigeon Codeur
 * @brief Definition of the serialization class
 * @version 0.1
 * @date 2022-04-28
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <string>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "Files/filemanager.h"

#include "logger.h"

namespace pg
{
    namespace constant
    {
        struct Vector2D;
        struct Vector3D;
        struct Vector4D;
        struct ModelInfo;
    }

    namespace
    {
        constexpr const char * const ARCHIVEVERSION = "1.0.0";

        /** Name of the constant string indicating an attribute */
        const std::string ATTRIBUTECONST = "__PGSA"; // Stand for PGSERIALISEDATTRIBUTE
    }

    // TODO correctly ban ":" from class name "{" for attribute name and ATTRIBUTECONST from any litteral !
    // check and remove any \t not correctly placed
    // Indent always go up one by one so if we skip an increment their is a problem 
    // Test all those case !

    /**
     * @brief Archive class responsible for creating the serialized string to be written to file
     */
    class Archive
    {
        // Helper struct

        /**
         * @brief Helper struct for end of line in the serialized string.
         * 
         * This struct is used to put an end of line in the serialized string.
         * A specialized template catches this struct and can then put an end of line
         * and the matching indent thanks to it.
         */
        struct EndOfLine
        {
            size_t* indentLevel = nullptr; //< References to the current indentation level of the related Archive
        };

        // Public methods
    public:
        /**
         * @brief Construct a new Archive object
         * 
         * The archive object is an advanced container for a serialized string
         * It manages all the operation related to the creation of the serialized string from user defined data
         * The underlying stringstream hold the serialized string
         */
        Archive() { endOfLine.indentLevel = &indentLevel; }

        virtual ~Archive() { }
        
        /**
         * @brief Function used to put an end of line in the serialized string
         * 
         * @return const EndOfLine& A reference to the end of line object of the Archive
         */
        const EndOfLine& endl() const { return endOfLine; }

        /** Start the serialization process of a class */
        virtual void startSerialization(const std::string& className);

        /** Start the serialization process of a class */
        virtual void endSerialization();

        /** Put an Attribute in the serialization process*/
        virtual void setAttribute(const std::string& value, const std::string& type = "");

        /** Set the name of the current value */
        virtual void setValueName(const std::string& name)
        {
            *this << name << ": ";
        }

        // Archive require the usage of EndOfLine otherwise it can break the whole serialization file !!!
        Archive& operator<<(const EndOfLine&)
        {
            requestComma = true;
            requestNewline = true;
            return *this;
        }

        Archive& operator<<(std::ostream & (*)(std::ostream &))
        {
            static_assert(true, "Can't use any std:: manipulator on this class !");

            return *this;
        }

        /**
         * @brief Operator used to put data into the serialized string
         * 
         * @tparam Type The type of data to be serialized
         * 
         * @param rhs A value to be serialized
         * @return Archive& A reference to the current Archive object
         * 
         * This operator put the data passed as the first parameter into the underlying container
         * It is responsible for putting new lines and setting the appropriate indent level,
         * to make it valid data for the parser !
         */
        template <typename Type>
        Archive& operator<<(const Type& rhs)
        {
            if (requestNewline)
            {
                if (requestComma)
                    container << ",";
                
                container << std::endl;
                container << std::string(*endOfLine.indentLevel, '\t');
                requestNewline = false;
            }

            container << rhs;
            return *this;
        }

        /** The underlying container of the Archive. */
        std::stringstream container;

        /** Flag indicating whether a new line should be inserted */
        bool requestNewline = false;
        /** Flag indicating whether a comma should be inserted */
        bool requestComma = false;
        /** The indent level of the current data */
        size_t indentLevel = 0;
        /** The custom end of line object */
        EndOfLine endOfLine;
    };

    // TODO make a specialized renderer for std::nullptr_t to catch nullptr error ?; 

    template <typename Type>
    void serialize(Archive&, const Type&) { LOG_ERROR("Serializer", "No serialize function exist for " << typeid(Type).name()); }

    // Todo make a static_assert to check if ": " is present in the name and reject it at compile time
    template <typename Type>
    void serialize(Archive& archive, const std::string& name, const Type& value)
    {
        archive.setValueName(name);

        serialize(archive, value);
    }

    template <>
    void serialize(Archive& archive, const bool& value);

    template <>
    void serialize(Archive& archive, const int& value);

    template <>
    void serialize(Archive& archive, const unsigned int& value);

    template <>
    void serialize(Archive& archive, const float& value);

    template <>
    void serialize(Archive& archive, const double& value);

    template <>
    void serialize(Archive& archive, const size_t& value);

    template <>
    void serialize(Archive& archive, const std::string& value);

    template <typename Type>
    void serialize(Archive& archive, const char* value)
    {
        // Todo store the number of characters in the string
        serialize(archive, std::string(value));
    }

    template <>
    void serialize(Archive& archive, const constant::Vector2D& vec2D);

    template <>
    void serialize(Archive& archive, const constant::Vector3D& vec3D);

    template <>
    void serialize(Archive& archive, const constant::Vector4D& vec4D);

    template <>
    void serialize(Archive& archive, const constant::ModelInfo& modelInfo);

    template <typename Type>
    void serialize(Archive& archive, const std::vector<Type>& vec)
    {
        archive.startSerialization("Vector");

        for (size_t i = 0; i < vec.size(); i++)
        {
            serialize(archive, vec.at(i));
        }

        archive.endSerialization();
    }


    class UnserializedObject
    {
        struct Attribute
        {
            std::string name = "";
            std::string value = "";
        };
    public:
        UnserializedObject() : isNullObject(true), isClass(false) { }
        UnserializedObject(const std::string& serializedString, const std::string& objectName = "", bool isClass = true) : objectName(objectName), serializedString(serializedString), isNullObject(false), isClass(isClass) { if (isClass) parseString(); }
        UnserializedObject(const std::string& objectName, const std::string& objectType, const std::string& serializedString) : objectName(objectName), objectType(objectType), serializedString(serializedString), isNullObject(false), isClass(true) {}

        const std::string& getObjectName() const { return objectName; }

        // Todo add
        const std::string& getObjectType() const { return objectType; }

        /**
         * @brief Return the object as an Attribute object.
         * Can only be called if isClass is set to false.
         * 
         * @return The representation of the string in an attribute form
         */
        UnserializedObject::Attribute getAsAttribute() const;

        inline bool isNull() const { return isNullObject; }

        inline bool isClassObject() const { return isClass; }

        const UnserializedObject& operator[](const std::string& key);
        const UnserializedObject& operator[](const std::string& key) const;

        const UnserializedObject& operator[](size_t id);
        const UnserializedObject& operator[](size_t id) const;

        size_t getNbChildren() const { return children.size(); }

        std::string getString() const { return serializedString; }

    public:
        std::vector<UnserializedObject> children;

    private:
        void parseString();

        std::string objectName;
        std::string objectType;
        std::string serializedString;
        
        bool isNullObject = false;
        bool isClass = true;
    };

    template <typename Type>
    Type deserialize(const UnserializedObject& serializedString);

    template <typename Type>
    std::vector<Type> deserializeVector(const UnserializedObject& serializedString)
    {
        LOG_THIS("Serializer");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("Serializer", "Element is null");
        }
        else
        {
            LOG_INFO("Serializer", "Deserializing Vector of " << typeid(Type).name());

            std::vector<Type> data;

            for (const auto& child : serializedString.children)
            {
                if (child.isNull())
                    continue;
                
                auto element = deserialize<Type>(child);

                data.push_back(element);
            }            

            return data;
        }

        return std::vector<Type>{};
    }

    // Todo add a version header for serialization

    class Serializer
    {
        class ClassSerializer
        {
        friend class Serializer;
            ClassSerializer(Serializer *ser, const std::string& objectName) : serializer(ser), objectName(objectName) {}
            ~ClassSerializer() { archive.container << std::endl; serializer->registerSerialized(objectName, archive.container); }
        
        public:
            Archive archive;

        private:
            Serializer *serializer;
            std::string objectName;
        };

    public:
        Serializer(const TextFile& file, bool autoSave = true);
        Serializer(bool autoSave = true) : autoSave(autoSave) {}
        ~Serializer();

        void setFile(const TextFile& file);
        void setFile(const std::string& path);

        void clear() { serializedMap.clear(); }

        // Todo remove baseIndent when removing indent need from serializer
        static std::unordered_map<std::string, std::string> readData(const std::string& vers, const std::string& stringData, size_t baseIndent = 0);

        static std::unique_ptr<Serializer>& getSerializer(const std::string& filename = "serialize.sz")
            {static std::unique_ptr<Serializer> serializer = std::unique_ptr<Serializer>(new Serializer(filename)); return serializer; }

        // Todo make a static_assert to check if ": " is present in the objectName and reject it at compile time
        template <typename Type>
        void serializeObject(const std::string& objectName, const Type& type) { ClassSerializer ar(this, objectName); serialize(ar.archive, type); }

        template <typename Type>
        Type deserializeObject(const std::string& objectName) const
        { 
            const auto& it = serializedMap.find(objectName);

            if (it != serializedMap.end())
                return deserialize<Type>(UnserializedObject(it->second, objectName)); 
            else 
                return deserialize<Type>(UnserializedObject());
        }

        const std::unordered_map<std::string, std::string>& getSerializedMap() const { return serializedMap; }

        inline const std::string& getVersion() { return version; }

    private:
        Serializer(const std::string& filename);
        void readFile(const std::string& data);

        void registerSerialized(const std::string& objectName, const std::stringstream& serializedString);
        void registerToFile() const;

        bool autoSave = true;

        TextFile file;
        std::mutex mutex;

        std::string version = ARCHIVEVERSION;

        std::unordered_map<std::string, std::string> serializedMap;
    };

    struct SerializedInfoHolder
    {
        SerializedInfoHolder() {}
        SerializedInfoHolder(const std::string& className) : className(className) {}
        SerializedInfoHolder(const std::string& name, const std::string& type, const std::string& value) : name(name), type(type), value(value) {}
        SerializedInfoHolder(const SerializedInfoHolder& other) = delete;
        SerializedInfoHolder(SerializedInfoHolder&& other) : className(std::move(other.className)), name(std::move(other.name)), type(std::move(other.type)), value(std::move(other.value)), parent(std::move(other.parent)), children(std::move(other.children)) {}

        std::string className;
        std::string name;
        std::string type;
        std::string value;

        SerializedInfoHolder* parent;
        std::vector<SerializedInfoHolder> children;
    };

    struct InspectorArchive : public Archive
    {
        /** Start the serialization process of a class */
        virtual void startSerialization(const std::string& className) override
        {
            auto& node = currentNode->children.emplace_back(className);

            node.name = lastAttributeName;
            lastAttributeName = "";

            node.parent = currentNode;

            currentNode = &node;
        }

        /** Start the serialization process of a class */
        virtual void endSerialization() override
        {
            currentNode = currentNode->parent;
        }

        /** Put an Attribute in the serialization process*/
        virtual void setAttribute(const std::string& value, const std::string& type = "") override
        {
            auto& attributeNode = currentNode->children.emplace_back(lastAttributeName, type, value);

            attributeNode.parent = currentNode;

            lastAttributeName = "";
        }

        virtual void setValueName(const std::string& name) override
        {
            lastAttributeName = name;
        }

        std::string lastAttributeName = "";

        SerializedInfoHolder mainNode;

        SerializedInfoHolder* currentNode = &mainNode;
    };
}

#endif // SERIALIZATION_H
