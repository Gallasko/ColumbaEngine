#pragma once

#include <string>
#include <vector>

#include "Memory/elementtype.h"
#include "Compiler/value_nanbox.h"   // Value
#include "UI/prefabspec.h"           // RecordList

namespace pg
{
    struct VM;

    // Reading what a script left behind: VM values into the engine's element types. Used by the
    // prefab loader (a parsed .yaml file) and by anything else that runs a script for its data
    // (a rule script's output globals): inputs go in as globals, outputs come back through these.
    //
    // A table is an ObjInstance ({} in PgScript), a list an ObjVector ([]). Every reader reports
    // what it refuses into `errors` (when given) as "<what>: <reason>", and returns false.
    namespace vmread
    {
        // Int, double, bool or string.
        bool isScalar(const Value& v);

        // A scalar as an ElementType: int -> int, double -> float, bool, string. Anything else is
        // the empty string.
        ElementType scalar(VM& vm, const Value& v);

        // A flat table of scalars. Keys starting with "__" (the VM's own) are skipped.
        bool record(VM& vm, const Value& table, ElementMap& out, std::vector<std::string>* errors, const std::string& what);

        // A list of flat tables.
        bool records(VM& vm, const Value& list, RecordList& out, std::vector<std::string>* errors, const std::string& what);

        // A list of scalars.
        bool scalars(VM& vm, const Value& list, std::vector<ElementType>& out, std::vector<std::string>* errors, const std::string& what);
    }
}
