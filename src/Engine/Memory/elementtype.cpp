#include "elementtype.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        const char * DOM = "Expression";
    }

    Strfy& operator<<(Strfy& os, const ElementType& value)
    {
        return os << value.toString();
    }

    Strfy& operator<<(Strfy& os, const ElementType::UnionType& value)
    {
        switch(value)
        {
        case ElementType::UnionType::FLOAT:
            return os << "float";
            break;

        case ElementType::UnionType::INT:
            return os << "int";
            break;
        
        case ElementType::UnionType::SIZE_T:
            return os << "size_t";
            break;

        case ElementType::UnionType::STRING:
            return os << "string";
            break;

        case ElementType::UnionType::BOOL:
            return os << "boolean";
            break;

        default:
            return os << "unknown";
            break;
        }

        return os << "unknown";
    }

    template <>
    void serialize(Archive& archive, const ElementType& element)
    {
        LOG_THIS(DOM);

        archive.startSerialization(ElementType::getType());
        
        switch(element.type)
        {
        case ElementType::UnionType::FLOAT:
            serialize(archive, "type", element.enumTypeToString(element.type));
            serialize(archive, "data", element.data.f);
            break;

        case ElementType::UnionType::INT:
            serialize(archive, "type", element.enumTypeToString(element.type));
            serialize(archive, "data", element.data.i);
            break;

        case ElementType::UnionType::SIZE_T:
            serialize(archive, "type", element.enumTypeToString(element.type));
            serialize(archive, "data", element.data.l);
            break;

        case ElementType::UnionType::STRING:
            serialize(archive, "type", element.enumTypeToString(element.type));
            serialize(archive, "data", element.data.s);
            break;

        case ElementType::UnionType::BOOL:
            serialize(archive, "type", element.enumTypeToString(element.type));
            serialize(archive, "data", element.data.b);
            break; 
        }

        archive.endSerialization();
    }

    template <>
    ElementType deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        ElementType value;

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_MILE(DOM, "Deserializing an Element Type");

            type = deserialize<std::string>(serializedString["type"]);

            if (type == "float")
                value.setValue(deserialize<float>(serializedString["data"]));
            else if (type == "int")
                value.setValue(deserialize<int>(serializedString["data"]));
            else if (type == "size_t")
                value.setValue(deserialize<size_t>(serializedString["data"]));
            else if (type == "string")
                value.setValue(deserialize<std::string>(serializedString["data"]));
            else if (type == "bool")
                value.setValue(deserialize<bool>(serializedString["data"]));
            else
            {
                LOG_ERROR(DOM, "Error in casting a string to UnionType");
                return value;
            }
        }

        return value;
    }

    bool ElementType::isTrue() const
    {
        switch(type)
        {
            case UnionType::FLOAT:  return get<float>() != 0.0f;     break;
            case UnionType::INT:    return get<int>() != 0;          break;
            case UnionType::SIZE_T: return get<size_t>() != 0;       break;
            case UnionType::STRING: return get<std::string>() != ""; break;
            case UnionType::BOOL:   return get<bool>();              break;

            default:
                LOG_ERROR(DOM, "Error in casting type to a boolean");
                return false;
                break;
        }
    }

    ElementType ElementType::operator+(const ElementType& other) const
    {
        if (type == UnionType::FLOAT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<float>() + other.get<float>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::INT)
        {
            return ElementType { get<float>() + other.get<int>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<float>() + other.get<size_t>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::INT)
        {
            return ElementType { get<int>() + other.get<int>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<int>() + other.get<float>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<int>() + other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<size_t>() + other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::INT)
        {
            return ElementType { get<size_t>() + other.get<int>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::FLOAT)
        {
            return ElementType { get<size_t>() + other.get<float>() };
        }
        else if (type == UnionType::STRING and other.type == UnionType::STRING)
        {
            return ElementType { get<std::string>() + other.get<std::string>() };
        }
        else
        {
            throw std::runtime_error(Strfy() << "Operator + between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator-(const ElementType& other) const
    {
        if (type == UnionType::FLOAT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<float>() - other.get<float>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::INT)
        {
            return ElementType { get<float>() - other.get<int>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<float>() - other.get<size_t>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::INT)
        {
            return ElementType { get<int>() - other.get<int>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<int>() - other.get<float>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<int>() - other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<size_t>() - other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::INT)
        {
            return ElementType { get<size_t>() - other.get<int>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::FLOAT)
        {
            return ElementType { get<size_t>() - other.get<float>() };
        }
        else
        {
            throw std::runtime_error(Strfy() << "Operator - between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator*(const ElementType& other) const
    {
        if (type == UnionType::FLOAT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<float>() * other.get<float>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::INT)
        {
            return ElementType { get<float>() * other.get<int>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<float>() * other.get<size_t>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::INT)
        {
            return ElementType { get<int>() * other.get<int>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<int>() * other.get<float>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<int>() * other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<size_t>() * other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::INT)
        {
            return ElementType { get<size_t>() * other.get<int>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::FLOAT)
        {
            return ElementType { get<size_t>() * other.get<float>() };
        }
        else
        {
            throw std::runtime_error(Strfy() << "Operator * between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator/(const ElementType& other) const
    {
        if (type == UnionType::FLOAT and other.type == UnionType::FLOAT)
        {
            if (other.get<float>() == 0.0f)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<float>() / other.get<float>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::INT)
        {
            if (other.get<int>() == 0)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<float>() / other.get<int>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::SIZE_T)
        {
            if (other.get<size_t>() == 0.0f)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<float>() / other.get<size_t>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::INT)
        {
            if (other.get<int>() == 0)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { static_cast<float>(get<int>()) / other.get<int>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::FLOAT)
        {
            if (other.get<float>() == 0.0f)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<int>() / other.get<float>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::SIZE_T)
        {
            if(other.get<size_t>() == 0)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<int>() / other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::SIZE_T)
        {
            if (other.get<size_t>() == 0)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<size_t>() / other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::INT)
        {
            if(other.get<int>() == 0)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<size_t>() / other.get<int>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::FLOAT)
        {
            if (other.get<float>() == 0.0f)
            {
                throw std::runtime_error("Division by zero");
            }

            return ElementType { get<size_t>() / other.get<float>() };
        }
        else
        {
            throw std::runtime_error(Strfy() << "Operator / between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator%(const ElementType& other) const
    {
        if (type == UnionType::INT and other.type == UnionType::INT)
        {
            return ElementType { get<int>() % other.get<int>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<int>() % other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<size_t>() % other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::INT)
        {
            return ElementType { get<size_t>() % other.get<int>() };
        }
        else
        {
            throw std::runtime_error(Strfy() << "Operator % between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator>(const ElementType& other) const
    {
        if (type == UnionType::FLOAT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<float>() > other.get<float>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::INT)
        {
            return ElementType { get<float>() > other.get<int>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<float>() > other.get<size_t>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::INT)
        {
            return ElementType { get<int>() > other.get<int>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<int>() > other.get<float>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::SIZE_T)
        {
            if (get<int>() < 0)
                return ElementType { false };
            else
                return ElementType { static_cast<unsigned int>(get<int>()) > other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<size_t>() > other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::INT)
        {
            if (other.get<int>() < 0)
                return ElementType { false };
            else
                return ElementType { get<size_t>() > static_cast<unsigned int>(other.get<int>()) };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::FLOAT)
        {
            return ElementType { get<size_t>() > other.get<float>() };
        }
        else
        {
            throw std::runtime_error(Strfy() << "Operator > between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator>=(const ElementType& other) const
    {
        try
        {
            return ElementType { not (other > *this) };
        }
        catch (const std::runtime_error& e)
        {
            throw std::runtime_error(Strfy() << "Operator >= between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator< (const ElementType& other) const
    {
        try
        {
            return ElementType { (other > *this) };
        }
        catch (const std::runtime_error& e)
        {
            throw std::runtime_error(Strfy() << "Operator < between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator<=(const ElementType& other) const
    {
        try
        {
            return ElementType { not (*this > other) };
        }
        catch (const std::runtime_error& e)
        {
            throw std::runtime_error(Strfy() << "Operator <= between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator==(const ElementType& other) const
    {
        if (type == UnionType::FLOAT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<float>() == other.get<float>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::INT)
        {
            return ElementType { get<float>() == other.get<int>() };
        }
        else if (type == UnionType::FLOAT and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<float>() == other.get<size_t>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::INT)
        {
            return ElementType { get<int>() == other.get<int>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::FLOAT)
        {
            return ElementType { get<int>() == other.get<float>() };
        }
        else if (type == UnionType::INT and other.type == UnionType::SIZE_T)
        {
            if (get<int>() < 0)
                return ElementType { false };
            else
                return ElementType { static_cast<unsigned int>(get<int>()) == other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::SIZE_T)
        {
            return ElementType { get<size_t>() == other.get<size_t>() };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::INT)
        {
            if(other.get<int>() < 0)
                return ElementType { false };
            else
                return ElementType { get<size_t>() == static_cast<unsigned int>(other.get<int>()) };
        }
        else if (type == UnionType::SIZE_T and other.type == UnionType::FLOAT)
        {
            return ElementType { get<size_t>() == other.get<float>() };
        }
        else if (type == UnionType::STRING and other.type == UnionType::STRING)
        {
            return ElementType { get<std::string>() == other.get<std::string>() };
        }
        else if (type == UnionType::BOOL and other.type == UnionType::BOOL)
        {
            return ElementType { get<bool>() == other.get<bool>() };
        }
        else
        {
            return ElementType { false };
        }
    }

    ElementType ElementType::operator!=(const ElementType& other) const
    {
        try
        {
            return ElementType { not (*this == other) };
        }
        catch (const std::runtime_error& e)
        {
            throw std::runtime_error(Strfy() << "Operator != between two incompatible operand: " << type << " and " << other.type);
        }
    }

    ElementType ElementType::operator-() const
    {
        if (type == UnionType::INT)
        {
            return ElementType { -get<int>() };
        }
        else if (type == UnionType::FLOAT)
        {
            return ElementType { -get<float>() };
        }
        else if (type == UnionType::SIZE_T)
        {
            return ElementType { -get<size_t>() };
        }
        else
        {
            throw std::runtime_error(Strfy() << "Operator unary - on an incompatible operand: " << type);
        }
    }

    std::string ElementType::toString() const
    {
        switch(type)
        {
            case UnionType::STRING: return get<std::string>(); break;
            case UnionType::INT: return std::to_string(get<int>()); break;
            case UnionType::SIZE_T: return std::to_string(get<size_t>()); break;
            case UnionType::FLOAT: return std::to_string(get<float>()); break;
            case UnionType::BOOL: return get<bool>() ? "true" : "false"; break;

            default:
                LOG_ERROR(DOM, "Error in casting type to string");
                return "Invalid";
                break;
        }
    }

    std::string ElementType::enumTypeToString(const ElementType::UnionType& type) const
    {
        switch(type)
        {
            case UnionType::FLOAT: return "float"; break;
            case UnionType::INT: return "int"; break;
            case UnionType::SIZE_T: return "size_t"; break;
            case UnionType::STRING: return "string"; break;
            case UnionType::BOOL: return "bool"; break;
            default:
                LOG_ERROR(DOM, "Error in casting type to a string");
                return "int";
                break;
        }
    }

    ElementType::operator float() const
    {
        if (this->type == UnionType::FLOAT)
            return data.f;
        else if (this->type == UnionType::INT)
            return static_cast<float>(data.i);
        else if (this->type == UnionType::SIZE_T)
            return static_cast<float>(data.l);
        else
        {
            LOG_ERROR(DOM, "Error in casting an element to float when defined as " + enumTypeToString(this->type));
            return 0.0f;
        }
    }

    ElementType::operator int() const
    {
        if (this->type == UnionType::INT)
            return data.i;
        else if (this->type == UnionType::FLOAT)
            return static_cast<int>(data.f);
        else if (this->type == UnionType::SIZE_T)
            return static_cast<int>(data.l);
        else
        {
            LOG_ERROR(DOM, "Error in casting an element to int when defined as " + enumTypeToString(this->type));
            return 0;
        }
    }

    ElementType::operator size_t() const
    {
        if (this->type == UnionType::SIZE_T)
            return data.l;
        else if (this->type == UnionType::INT)
            return static_cast<size_t>(data.i);
        else if (this->type == UnionType::FLOAT)
            return static_cast<size_t>(data.f);
        else
        {
            LOG_ERROR(DOM, "Error in casting an element to int when defined as " + enumTypeToString(this->type));
            return 0;
        }
    }

    ElementType::operator std::string() const
    {
        if (this->type == UnionType::STRING)
            return data.s;
        else
        {
            LOG_ERROR(DOM, "Error in casting an element to string when defined as " + enumTypeToString(this->type));
            return "";
        }
    }

    ElementType::operator bool() const
    {
        if (this->type == UnionType::BOOL)
            return data.b;
        else
        {
            LOG_ERROR(DOM, "Error in casting an element to bool when defined as " + enumTypeToString(this->type));
            return false;
        }
    }

}