#include "uniqueid.h"

#include "logger.h"

#include "serialization.h"

namespace pg
{
    // namespace
    // {
    //     static constexpr const char * DOM = "Unique id";
    // }

    // template <>
    // void serialize(Archive& archive, const _unique_id& value)
    // {
    //     LOG_THIS(DOM);

    //     archive.setAttribute(std::to_string(value), "unique id");
    // }

    // template <>
    // _unique_id deserialize(const UnserializedObject& serializedString)
    // {
    //     LOG_THIS(DOM);

    //     _unique_id value = 0;

    //     auto attribute = serializedString.getAsAttribute();
    //     if (attribute.name != "_unique_id")
    //     {
    //         LOG_ERROR(DOM, "Serialized string is not an _unique_id");
    //         return value;
    //     }

    //     std::stringstream sstream(attribute.value);
    //     sstream >> value;

    //     return value;
    // }
}