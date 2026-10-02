#pragma once

#include <string>

#include "logger.h"
#include "serialization.h"

#include "vm.h"

namespace pg
{
    /**
     * @brief Copy the first object serialized in an archive into a new script table
     *
     * The table gets a "__className" field plus one field per attribute. Nested objects, vectors and maps become nested tables,
     * and an ElementType is flattened to the value it holds.
     *
     * @return The table, owned by the caller
     */
    Value archiveToTable(VM* vm, const InspectorArchive& archive);

    /**
     * @brief Convert a script table into the object tree read by deserialize<Type>()
     *
     * Fields that have no serialized form (functions, proxies) are skipped.
     *
     * @param typeName Type the root object is read as
     */
    UnserializedObject tableToUnserializedObject(VM* vm, ObjInstance* table, const std::string& typeName);

    /**
     * @brief Copy any serializable object into a new script table
     *
     * The table is a snapshot: writing to it does not change the object.
     */
    template <typename Type>
    Value serializeToTable(VM* vm, const Type& object)
    {
        InspectorArchive archive;

        serialize(archive, object);

        return archiveToTable(vm, archive);
    }

    /**
     * @brief Build an object of a known type from a script table
     *
     * @return The object, or a default constructed one if the value is not a table
     */
    template <typename Type>
    Type deserializeTo(VM* vm, Value table)
    {
        if (not IS_INSTANCE(table))
        {
            LOG_ERROR("Table Serialization", "Table value is not an instance");

            return Type{};
        }

        auto objTable = vm->asInstance(table);

        // A table can name its own type, otherwise it is read as the requested one
        std::string typeName = Type::getType();

        if (objTable->hasField("__className"))
            typeName = vm->asString(objTable->getField("__className"));

        return deserialize<Type>(tableToUnserializedObject(vm, objTable, typeName));
    }
}
