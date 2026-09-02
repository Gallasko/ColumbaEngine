#include "stdafx.h"

#include "object.h"

#include "chunk.h"

#include "vm.h"

namespace pg
{
    Closure::Closure(ObjFunction* func) : function(func)
    {
        upvalues.resize(func->upvalueCount);
    }

    void ObjInstance::setField(const std::string& name, Value value, VM *vm, bool deleteOld)
    {
        auto it = internedFields.find(name);
        if (it != internedFields.end())
        {
            if (deleteOld and vm)
            {
                // Release old value if it requires ref counting
                Value oldValue = fieldValues[it->second];
                if (requiresRefCount(oldValue))
                {
                    vm->releaseValue(oldValue);
                }
            }

            // Update existing field
            fieldValues[it->second] = value;
        }
        else
        {
            // Add new field
            size_t index = fieldValues.size();
            fieldValues.push_back(value);
            fieldNames.push_back(name);
            internedFields[name] = index;
        }
    }
}