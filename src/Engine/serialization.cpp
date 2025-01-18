#include "serialization.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <cstring>

#include "logger.h"
#include "constant.h"

namespace pg
{
    namespace
    {
        /** Name of the current domain (for logging purposes)*/
        static constexpr char const * DOM = "Serializer";

        /**
         * @brief A function that returns the number of leading whitespace characters in a string
         * 
         * @param str The string to check
         * @param whitespace Characters used as whitespace characters
         * 
         * @return size_t The number of whitespace characters at the beginning of the string
         */
        size_t nbLeadingSpaces(const std::string& str, const std::string& whitespace = " \t")
        {
            LOG_THIS(DOM);
            
            const auto strBegin = str.find_first_not_of(whitespace);
            if (strBegin == std::string::npos)
                return -1; // no content

            return strBegin;
        }

        /**
         * @brief A function that trim all whitespace characters from the beginning of a string
         * 
         * @param str The string to trim
         * @param whitespace Characters used as whitespace characters
         * 
         * @return std::string The resulting string trimmed of the whitespace characters
         */
        std::string trim(const std::string& str, const std::string& whitespace = " \t")
        {
            LOG_THIS(DOM);

            const auto strBegin = str.find_first_not_of(whitespace);
            if (strBegin == std::string::npos)
                return ""; // no content

            const auto strEnd = str.find_last_not_of(whitespace);
            const auto strRange = strEnd - strBegin + 1;

            return str.substr(strBegin, strRange);
        }

        /**
         * @brief Return a string corresponding to all the data left in a stream
         * 
         * @param in A String stream
         * @return std::string String with all the data left in the stream
         */
        std::string gulp(std::istream &in)
        {
            std::string ret;
            char buffer[4096];
            while (in.read(buffer, sizeof(buffer)))
                ret.append(buffer, sizeof(buffer));
            ret.append(buffer, in.gcount());
            return ret;
        }
    }

    // Serialisation of base type

    template <>
    void serialize(Archive& archive, const bool& value)
    {
        LOG_THIS(DOM);

        std::string res = value ? "true" : "false";

        archive.setAttribute(res, "bool");
    }

    template <>
    void serialize(Archive& archive, const int& value)
    {
        LOG_THIS(DOM);

        archive.setAttribute(std::to_string(value), "int");
    }

    template <>
    void serialize(Archive& archive, const unsigned int& value)
    {
        LOG_THIS(DOM);

        archive.setAttribute(std::to_string(value), "unsigned int");
    }

    template <>
    void serialize(Archive& archive, const float& value)
    {
        LOG_THIS(DOM);

        archive.setAttribute(std::to_string(value), "float");
    }

    template <>
    void serialize(Archive& archive, const double& value)
    {
        LOG_THIS(DOM);

        archive.setAttribute(std::to_string(value), "double");
    }

    template <>
    void serialize(Archive& archive, const size_t& value)
    {
        LOG_THIS(DOM);

        archive.setAttribute(std::to_string(value), "size_t");
    }

    template <>
    void serialize(Archive& archive, const std::string& value)
    {
        LOG_THIS(DOM);

        archive.setAttribute(value, "string");
    }

    template <>
    void serialize(Archive& archive, const constant::Vector2D& vec2D)
    {
        LOG_THIS(DOM);

        archive.startSerialization("Vector 2D");

        serialize(archive, "x", vec2D.x);
        serialize(archive, "y", vec2D.y);

        archive.endSerialization();
    }

    template <>
    void serialize(Archive& archive, const constant::Vector3D& vec3D)
    {
        LOG_THIS(DOM);

        archive.startSerialization("Vector 3D");

        serialize(archive, "x", vec3D.x);
        serialize(archive, "y", vec3D.y);
        serialize(archive, "z", vec3D.z);

        archive.endSerialization();
    }

    template <>
    void serialize(Archive& archive, const constant::Vector4D& vec4D)
    {
        LOG_THIS(DOM);

        archive.startSerialization("Vector 4D");

        serialize(archive, "x", vec4D.x);
        serialize(archive, "y", vec4D.y);
        serialize(archive, "z", vec4D.z);
        serialize(archive, "w", vec4D.w);

        archive.endSerialization();
    }

    template <>
    void serialize(Archive& archive, const constant::ModelInfo& modelInfo)
    {
        LOG_THIS(DOM);

        archive.startSerialization("Model Info");

        // Todo: create a serializer for a vector/list/map/arrays (serializeList)
        std::string attribute = "[ ";
        
        for (unsigned int i = 0; i < modelInfo.nbVertices; i++)
            attribute += std::to_string(modelInfo.vertices[i]) + " ";
        
        attribute += "]";
        archive.setAttribute(attribute, "Verticies");

        attribute = "[ ";
        
        for (unsigned int i = 0; i < modelInfo.nbIndices; i++)
            attribute += std::to_string(modelInfo.indices[i]) + " ";
        
        attribute += "]";
        archive.setAttribute(attribute, "Indicies");
        
        archive.endSerialization();
    }

    template <>
    bool deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        auto attribute = serializedString.getAsAttribute();
        if (attribute.name != "bool")
        {
            LOG_ERROR(DOM, "Serialized string [" << serializedString.getObjectName() << "] is not a bool (" << attribute.name << ")");
            return false;
        }

        if (attribute.value == "true")
            return true;
        else if (attribute.value == "false")
            return false;

        LOG_ERROR(DOM, "Serialized string [" << serializedString.getObjectName() << "] of bool is neither true or false");
        return false;
    }

    template <>
    int deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        int value = 0;

        auto attribute = serializedString.getAsAttribute();

        // Todo check this
        if (attribute.name != "size_t" and attribute.name != "unsigned int" and attribute.name != "int")
        {
            LOG_ERROR(DOM, "Serialized string [" << serializedString.getObjectName() << "] is not an integer (" << attribute.name << ")");
            return value;
        }

        std::stringstream sstream(attribute.value);
        sstream >> value;

        return value;
    }

    template <>
    unsigned int deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        unsigned int value = 0;

        auto attribute = serializedString.getAsAttribute();

        // Todo check this
        if (attribute.name != "size_t" and attribute.name != "unsigned int" and attribute.name != "int")
        {
            LOG_ERROR(DOM, "Serialized string [" << serializedString.getObjectName() << "] is not an integer (" << attribute.name << ")");
            return value;
        }

        std::stringstream sstream(attribute.value);
        sstream >> value;

        return value;
    }

    template <>
    float deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        float value = 0;

        auto attribute = serializedString.getAsAttribute();
        if (attribute.name != "float")
        {
            LOG_ERROR(DOM, "Serialized string [" << serializedString.getObjectName() << "] is not a float (" << attribute.name << ")");
            return value;
        }

        std::stringstream sstream(attribute.value);
        sstream >> value;

        return value;
    }

    template <>
    double deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        double value = 0;

        auto attribute = serializedString.getAsAttribute();
        if (attribute.name != "double")
        {
            LOG_ERROR(DOM, "Serialized string [" << serializedString.getObjectName() << "] is not a double (" << attribute.name << ")");
            return value;
        }

        std::stringstream sstream(attribute.value);
        sstream >> value;

        return value;
    }

    template <>
    size_t deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        size_t value = 0;

        auto attribute = serializedString.getAsAttribute();

        // Todo check this
        if (attribute.name != "size_t" and attribute.name != "unsigned int" and attribute.name != "int")
        {
            LOG_ERROR(DOM, "Serialized string [" << serializedString.getObjectName() << "] is not an integer (" << attribute.name << ")");
            return value;
        }

        std::stringstream sstream(attribute.value);
        sstream >> value;

        return value;
    }

    template <>
    std::string deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string value;

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            auto stringAttribute = serializedString.getAsAttribute();

            if (stringAttribute.name != "string")
            {
                LOG_ERROR(DOM, "String [" << serializedString.getObjectName() << "] attribute name is not 'string' (" << stringAttribute.name << ")");
                
                return value;
            }

            return stringAttribute.value;
        }

        return value;
    }
    
    template <>
    constant::Vector2D deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an Vector2D");

            auto x = deserialize<float>(serializedString["x"]);
            auto y = deserialize<float>(serializedString["y"]);

            return constant::Vector2D{x, y};
        }

        return constant::Vector2D{0, 0};
    }

    template <>
    constant::Vector3D deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an Vector3D");

            auto x = deserialize<float>(serializedString["x"]);
            auto y = deserialize<float>(serializedString["y"]);
            auto z = deserialize<float>(serializedString["z"]);

            return constant::Vector3D{x, y, z};
        }

        return constant::Vector3D{0, 0, 0};
    }

    template <>
    constant::Vector4D deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an Vector4D");

            auto x = deserialize<float>(serializedString["x"]);
            auto y = deserialize<float>(serializedString["y"]);
            auto z = deserialize<float>(serializedString["z"]);
            auto w = deserialize<float>(serializedString["w"]);

            return constant::Vector4D{x, y, z, w};
        }

        return constant::Vector4D{0, 0, 0, 0};
    }

    void Archive::startSerialization(const std::string& className)
    {
        LOG_THIS_MEMBER(DOM);

        *this << className << " {" << endOfLine;
        indentLevel++;

        requestComma = false;
    }

    void Archive::endSerialization()
    {
        LOG_THIS_MEMBER(DOM);

        requestComma = false;

        indentLevel--;
        *this << "}" << endOfLine;
    }

    void Archive::setAttribute(const std::string& value, const std::string& type)
    {
        if (type.find(ATTRIBUTECONST) != std::string::npos)
        {
            LOG_ERROR(DOM, "Invalid attribute, " + std::string(ATTRIBUTECONST) + " is present in the string. Archive will not register it !");

            return;
        }

        // Set the prefix ATTRIBUTECONST inside the attribute to mark it so the parser can handle multiline attribute
        *this << ATTRIBUTECONST << " " << type << " {";
        
        std::istringstream iss(value);
        std::string line;

        bool firstLine = true;

        // Remove any std::endl and replace them with the archive.endl so the parser doesn't mix up classes and attribute !
        while (std::getline(iss, line))
        {
            if (firstLine)
                firstLine = false;
            else
                *this << endl();
            *this << line;
        }
            
        *this << "}" << endl();
    }

    UnserializedObject::Attribute UnserializedObject::getAsAttribute() const
    {
        UnserializedObject::Attribute attribute;

        // Check if the isClass is set to true, then it is a class object and not a attribute object so we return a blank attribute and raise an error.
        if (isClass)
        {
            LOG_ERROR(DOM, "Object: " << objectName << " ( " << objectType << ") is not an attribute");

            return UnserializedObject::Attribute();
        }

        // Lockup for the ATTRIBUTECONST
        auto ATTRIBUTECONSTPos = serializedString.find(ATTRIBUTECONST);

        if (ATTRIBUTECONSTPos == std::string::npos)
        {
            LOG_ERROR(DOM, "The serializedString is missing the 'ATTRIBUTECONST' !");

            return UnserializedObject::Attribute();
        }

        ATTRIBUTECONSTPos += ATTRIBUTECONST.size();

        auto startAttributeValue = serializedString.find("{", ATTRIBUTECONSTPos);

        if (startAttributeValue == std::string::npos)
        {
            LOG_ERROR(DOM, "The serializedString is missing the '{' that start the attribute !");

            return UnserializedObject::Attribute();
        }

        if (startAttributeValue - ATTRIBUTECONSTPos < 2)
        {
            LOG_ERROR(DOM, "The serializedString is ill formated 'ATTRIBUTECONST' and '{' should at least be separeted by two characters !");

            return UnserializedObject::Attribute();
        }
        else if (startAttributeValue - ATTRIBUTECONSTPos == 2)
        {
            attribute.name = "";
        }
        else
        {
            attribute.name = serializedString.substr(ATTRIBUTECONSTPos + 1, startAttributeValue - ATTRIBUTECONSTPos - 2);
        }

        auto endAttributeValue = serializedString.rfind("}");

        if (endAttributeValue == std::string::npos)
        {
            LOG_ERROR(DOM, "The serializedString is missing the '}' that end the attribute !");

            return UnserializedObject::Attribute();
        }

        attribute.value = serializedString.substr(startAttributeValue + 1, endAttributeValue - (startAttributeValue + 1));

        return attribute;
    }

    const UnserializedObject& UnserializedObject::operator[](const std::string& key) noexcept
    {
        auto isObjectName = [=](UnserializedObject obj) { return obj.objectName == key; }; 
        auto it = std::find_if(children.begin(), children.end(), isObjectName);

        if (it != children.end())
            return *it;
        else
        {
            LOG_ERROR(DOM, "Requested the child: '" + key + "' not present inside the object");
            return nullUnserializedObject;
        }
    }

    const UnserializedObject& UnserializedObject::operator[](const std::string& key) const noexcept
    {
        auto isObjectName = [=](const UnserializedObject& obj) { return obj.objectName == key; }; 
        const auto& it = std::find_if(children.begin(), children.end(), isObjectName);

        if (it != children.end())
            return *it;
        else
        {
            LOG_ERROR(DOM, "Requested the child: '" + key + "' not present inside the object");
            return nullUnserializedObject;
        }
    }

    bool UnserializedObject::find(const std::string& key) const
    {
        auto isObjectName = [=](const UnserializedObject& obj) { return obj.objectName == key; }; 
        const auto& it = std::find_if(children.begin(), children.end(), isObjectName);

        return it != children.end();
    }

    const UnserializedObject& UnserializedObject::operator[](size_t id) noexcept
    {
        if (id < children.size())
            return children.at(id);
        else
        {
            LOG_ERROR(DOM, "Requested the child: '" + std::to_string(id) + "' not present inside the object");
            return nullUnserializedObject;
        }
    }

    const UnserializedObject& UnserializedObject::operator[](size_t id) const noexcept
    {
        if (id < children.size())
            return children.at(id);
        else
        {
            LOG_ERROR(DOM, "Requested the child: '" + std::to_string(id)  + "' not present inside the object");
            return nullUnserializedObject;
        }
    }

    // Todo need to fix when a serialized class is empty
    void UnserializedObject::parseString()
    {
        LOG_THIS_MEMBER(DOM);

        size_t lineNumber = 0;

        std::string currentLine;
        size_t classChildIndent = 0;

        std::string nextLine;
        size_t currentIndent = 0;
        size_t nextIndent = 0;

        std::istringstream iss(serializedString);

        std::string tempObjectName = ""; 
        std::string tempAttributeName = ""; 
        std::string delimiter = ": ";
        std::string tempSerializedString = "";
        size_t pos;
        
        // First line should always be the beginning of a class declaration
        bool startOfClass = false;
        bool lockupClass = false;

        bool lockupAttribute = false;

        // First class of the serialized string is for the current class
        //bool firstClass = true;

        bool errorHappened = false;

        // The first child is a empty one
        children.emplace_back();

        // Check if their is at least one line in the serialized string
        if (not std::getline(iss, currentLine))
        {
            LOG_ERROR(DOM, "Serialized string is empty for object: '" + objectName + "'");

            isNullObject = true;
            return;
        }
        else
        {
            // Then calculate the indent of the children
            classChildIndent = nbLeadingSpaces("\t" + currentLine);

            // This can happen if the class is actually a basic type which is only an attribute !
            if (currentLine.find(ATTRIBUTECONST) != std::string::npos)
            {
                isClass = false;
                return;
            }

            // Check if the name given to the constructor match to the class in the serialized string
            pos = currentLine.find(delimiter);

            // If ":" is not found for the class declaration then it is a unnamed one
            if (pos == std::string::npos)
            {
                tempObjectName = "";

                // This should always be true
                if (classChildIndent > 0)
                    pos = classChildIndent - 1;
                else
                    pos = 0;
            }
            else // Else the name of the object is the first part of the string
            {
                tempObjectName = trim(currentLine.substr(0, pos));
                pos += 2;
            }

            auto endPos = currentLine.find(" {");

            if (endPos == std::string::npos)
            {
                LOG_ERROR(DOM, "Error happened when parsing class '" + objectName + "' missing { after class type");
                
                isNullObject = true;
                return;
            }
            else
            {
                objectType = currentLine.substr(pos, endPos - 1);
                LOG_MILE(DOM, "Found class type '" + objectType + "'");
            }

            // If the object name doesn't match we throw an error
            if (tempObjectName != objectName)
            {
                LOG_ERROR(DOM, "Error happened when passing data: Current objectName '" + objectName + "' doesn't match the serialized string passed '" + tempObjectName + "'");
                
                isNullObject = true;
                return;
            }

            // Read the next line (For a class their should always be at least 2 lines, one for constructor and the last "}" to indicate the end of the class)
            if (not std::getline(iss, currentLine))
            {
                LOG_ERROR(DOM, "Serialized string doesn't end correctly for object: '" + objectName + "'");

                isNullObject = true;
                return;
            }

            // If the second line of the string is "{" then it means that the class doesn't have any children so we can return early.
            const auto trimmed = trim(currentLine);
            if (trimmed == "}")
            {
                LOG_MILE(DOM, "Object: " + objectName + " doesn't have any children in serialization !");
                
                isNullObject = false;
                return;
            }

        }
            
        while (std::getline(iss, nextLine))
        {
            nextIndent = nbLeadingSpaces(nextLine);

            bool notInClassCurrently = startOfClass == false and lockupClass == false;
            
            if (nextIndent > classChildIndent and notInClassCurrently)
                startOfClass = true;

            const auto nextLineTrimmed = trim(nextLine);
            // This can happen if the class is empty
            if (notInClassCurrently and (nextLineTrimmed == "}" or nextLineTrimmed =="},"))
            {
                startOfClass = true;
            }

            if (startOfClass)
            {
                // If lockupAttribute is true, then we finished processing the previous attribute
                if (lockupAttribute)
                {
                    children.emplace_back(tempSerializedString, tempAttributeName, false);
                    lockupAttribute = false;
                }

                // Try to find ":" to see if the class is named or not 
                pos = currentLine.find(delimiter);

                // If ":" is not found for the class declaration then it is a unnamed one
                if (pos == std::string::npos)
                {
                    tempObjectName = "";
                }
                else // Else the name of the object is the first part of the string
                {
                    tempObjectName = trim(currentLine.substr(0, pos));
                }

                // Todo: check if we need to retrive the class name from the serialized object

                // Look up for another ":" if found the serialized string is ill formated
                pos = currentLine.find(delimiter, pos + delimiter.length());
    
                if (pos != std::string::npos)
                {
                    LOG_ERROR(DOM, "Class string is ill formated, current line possess multiple ':' '" + currentLine + "'");

                    errorHappened = true;
                    break;
                }

                tempSerializedString = currentLine + "\n";

                startOfClass = false;
                lockupClass = true;
            }
            else
            {
                if (lockupClass)
                {
                    tempSerializedString += currentLine + "\n";

                    const auto trimmed = trim(currentLine);
                    if ((classChildIndent == currentIndent) and (trimmed == "}" or trimmed =="},"))
                    {
                        children.emplace_back(tempSerializedString, tempObjectName);
                        lockupClass = false;
                    }
                }
                else
                {
                    pos = currentLine.find(ATTRIBUTECONST);

                    // The current line is the beginning of a new attribute
                    if (pos != std::string::npos)
                    {
                        // If lockupAttribute is true, then we finished processing the previous attribute
                        if (lockupAttribute)
                        {
                            children.emplace_back(tempSerializedString, tempAttributeName, false);
                            lockupAttribute = false;
                        }

                        // Cut the beginning of the current line to look for the attribute name
                        tempAttributeName = currentLine.substr(0, pos);

                        // Try to find ":" to see if the attribute is named or not 
                        pos = tempAttributeName.find(delimiter);

                        // If ":" is not found for the class declaration then it is a unnamed one
                        if (pos == std::string::npos)
                            tempAttributeName = "";
                        else // Else the name of the object is the first part of the string
                            tempAttributeName = trim(tempAttributeName.substr(0, pos));

                        LOG_MILE(DOM, "Child Attribute name: '" + tempAttributeName + "'");

                        tempSerializedString = currentLine + "\n";

                        lockupAttribute = true;
                    }
                    // Error the current line describe nothing
                    else if (pos == std::string::npos and lockupAttribute == false)
                    {
                        LOG_ERROR(DOM, "Current [" << lineNumber << "] line: " << currentLine);
                        LOG_ERROR(DOM, "Line is neither a class definition, a class body nor a attribute, serialization string is ill formed. Exiting Serialization Parsing !");

                        errorHappened = true;
                        break;
                    }
                    // The current line is part of the body of the attribute
                    else if (pos == std::string::npos and lockupAttribute == true)
                    {
                        // Trim the line by the current number of indent to make it back to the original format before making the value an attribute !
                        tempSerializedString += currentLine.substr(currentIndent) + "\n";
                    }
                    
                }
            }

            currentLine = nextLine;
            currentIndent = nextIndent;
            lineNumber++;
        }

        // If lockupAttribute is true, then we finished processing the previous attribute
        if (lockupAttribute)
        {
            children.emplace_back(tempSerializedString, tempAttributeName, false);
            lockupAttribute = false;
        }

        if (errorHappened)
        {
            LOG_ERROR(DOM, "Error happened when parsing serialized string for object: '" + objectName + "'");
            isNullObject = true;

            return;
        }

        isNullObject = false;
    }

    Serializer::Serializer(const TextFile& file, bool autoSave) : autoSave(autoSave), file(file)
    {
        LOG_THIS_MEMBER(DOM);

        readFile(file.data);
    }
    
    Serializer::Serializer(const std::string& filename)
    {
        LOG_THIS_MEMBER(DOM);

        TextFile file = UniversalFileAccessor::openTextFile(filename);
        this->file = file;

        readFile(file.data);
    }

    Serializer::~Serializer()
    {
        LOG_THIS_MEMBER(DOM);

        std::lock_guard<std::mutex> lock(mutex);

        if (autoSave)
            registerToFile();
    }

    void Serializer::setFile(const TextFile& file)
    {
        LOG_THIS_MEMBER(DOM);

        std::lock_guard<std::mutex> lock(mutex);

        serializedMap.clear();

        this->file = file;

        readFile(file.data);
    }

    void Serializer::setFile(const std::string& path)
    {
        LOG_THIS_MEMBER(DOM);

        std::lock_guard<std::mutex> lock(mutex);

        serializedMap.clear();

        TextFile file = UniversalFileAccessor::openTextFile(path);
        this->file = file;

        readFile(file.data);
    }

    void Serializer::readFile(const std::string& data)
    {
        LOG_THIS_MEMBER(DOM);

        if (data.empty())
        {
            LOG_MILE(DOM, "Reading an empty file");
            return;
        }

        std::string line;

        std::istringstream stream(data);

        // First line of the file should always be the version number
        std::getline(stream, line);

        version = line;

        auto stringData = gulp(stream);

        LOG_INFO(DOM, stringData);

        serializedMap = readData(version, stringData);
    }

    std::unordered_map<std::string, std::string> Serializer::readData(const std::string& vers, const std::string& stringData, size_t baseIndent)
    {
        LOG_THIS(DOM);

        std::string line;
        std::string serializedString;

        std::string objectName = ""; 
        std::string delimiter = ": ";
        size_t pos;

        bool startOfClass;

        bool errorHappened = false;

        std::istringstream stream(stringData);

        std::unordered_map<std::string, std::string> sMap;

        // Serialization format for the current version of the serializer
        if (vers == ARCHIVEVERSION)
        {
            // First line of data should always be the beginning of a class declaration
            startOfClass = true;

            while (std::getline(stream, line))
            {
                // Todo remove this once indent is removed from serializer
                if (baseIndent > 0 and line.size() > 0)
                {
                    line = line.substr(baseIndent, line.size());
                    LOG_INFO(DOM, line);
                }

                if (startOfClass)
                {
                    pos = line.find(delimiter);

                    if (pos == std::string::npos)
                    {
                        errorHappened = true;
                        break;
                    }
                    
                    objectName = line.substr(0, pos);

                    pos = line.find(delimiter, pos + delimiter.length());

                    // This can happen if the class is actually a basic type which is only an attribute !
                    if (line.find(ATTRIBUTECONST) != std::string::npos)
                    {
                        serializedString = line + '\n';

                        sMap[objectName] = serializedString;
                        continue;
                    }

                    if (pos != std::string::npos)
                    {
                        errorHappened = true;
                        break;
                    }

                    serializedString = line + '\n';
                    startOfClass = false;
                }
                else
                {
                    serializedString += line + '\n';

                    if (line == "}")
                    {
                        sMap[objectName] = serializedString;
                        startOfClass = true;
                    }
                        
                }
            }
        }
        
        if (errorHappened)
        {
            LOG_ERROR(DOM, "Error happened when parsing serialized data");
        }

        return sMap;
    }

    void Serializer::registerSerialized(const std::string& objectName, const std::stringstream& serializedString)
    {
        LOG_THIS_MEMBER(DOM);

        std::lock_guard<std::mutex> lock(mutex);
        serializedMap[objectName] = serializedString.str();

        registerToFile();
    }

    void Serializer::registerToFile() const
    {
        LOG_THIS_MEMBER(DOM);

        std::ostringstream stream;

        // First line of the serialized file should be the version id of the serializer
        stream << version << std::endl;

        for (const auto& serializedString : serializedMap)
            stream << serializedString.first << ": " << serializedString.second;

        // LOG_INFO(DOM, "Writing to file: " << file.filepath << " " << stream.str());

        UniversalFileAccessor::writeToFile(file, stream.str(), true);
    }
}