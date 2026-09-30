#include "stdafx.h"

#include "vmreaders.h"

#include "Compiler/vm.h"

namespace pg
{
    namespace vmread
    {
        namespace
        {
            void fail(std::vector<std::string>* errors, const std::string& message)
            {
                if (errors)
                    errors->push_back(message);
            }
        }

        bool isScalar(const Value& v)
        {
            return IS_INT(v) or IS_DOUBLE(v) or IS_BOOL(v) or IS_STRING(v);
        }

        ElementType scalar(VM& vm, const Value& v)
        {
            if (IS_INT(v))
                return ElementType{static_cast<int>(AS_INT(v))};
            if (IS_DOUBLE(v))
                return ElementType{static_cast<float>(AS_DOUBLE(v))};
            if (IS_BOOL(v))
                return ElementType{AS_BOOL(v)};
            if (IS_STRING(v))
                return ElementType{vm.asString(v)};

            return ElementType{std::string()};
        }

        bool record(VM& vm, const Value& table, ElementMap& out, std::vector<std::string>* errors, const std::string& what)
        {
            if (not IS_INSTANCE(table))
            {
                fail(errors, what + ": must be a table");
                return false;
            }

            ObjInstance* instance = vm.asInstance(table);

            for (size_t i = 0; i < instance->fieldValues.size(); ++i)
            {
                const std::string& key = instance->fieldNames[i];

                if (key.empty() or key.rfind("__", 0) == 0)
                    continue;

                const Value v = instance->fieldValues[i];

                if (not isScalar(v))
                {
                    fail(errors, what + "." + key + ": only scalars are allowed inside a record");
                    return false;
                }

                out[key] = scalar(vm, v);
            }

            return true;
        }

        bool records(VM& vm, const Value& list, RecordList& out, std::vector<std::string>* errors, const std::string& what)
        {
            if (not IS_VECTOR(list))
            {
                fail(errors, what + ": must be a list of tables");
                return false;
            }

            size_t n = 0;

            for (Value item : vm.asVector(list)->fields)
            {
                ElementMap rec;

                if (not record(vm, item, rec, errors, what + "[" + std::to_string(n++) + "]"))
                    return false;

                out.push_back(std::move(rec));
            }

            return true;
        }

        bool scalars(VM& vm, const Value& list, std::vector<ElementType>& out, std::vector<std::string>* errors, const std::string& what)
        {
            if (not IS_VECTOR(list))
            {
                fail(errors, what + ": must be a list of scalars");
                return false;
            }

            size_t n = 0;

            for (Value item : vm.asVector(list)->fields)
            {
                if (not isScalar(item))
                {
                    fail(errors, what + "[" + std::to_string(n) + "]: must be a scalar");
                    return false;
                }

                out.push_back(scalar(vm, item));
                ++n;
            }

            return true;
        }
    }
}
