#include "jsonmodule.h"

#include "Helpers/json.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace pg
{
    namespace
    {
        using Json = nlohmann::ordered_json;

        // Thrown by the parse callback past MAX_DEPTH, caught in nativeParse
        struct TooDeep {};

        // A value of the parsed text as a PgScript value. Each child is made before its parent's
        // object is looked up again: making it may grow the pool the parent lives in
        Value toValue(VM* vm, const Json& json)
        {
            switch (json.type())
            {
                case Json::value_t::null:
                    return makeNoneValue();

                case Json::value_t::boolean:
                    return makeBoolValue(json.get<bool>());

                case Json::value_t::number_integer:
                {
                    const int64_t value = json.get<int64_t>();
                    return fitsIntValue(value) ? makeIntValue(value) : makeDoubleValue(static_cast<double>(value));
                }

                case Json::value_t::number_unsigned:
                {
                    const uint64_t value = json.get<uint64_t>();
                    if (value <= static_cast<uint64_t>(INT_VALUE_MAX))
                        return makeIntValue(static_cast<int64_t>(value));
                    return makeDoubleValue(static_cast<double>(value));
                }

                case Json::value_t::number_float:
                    return makeDoubleValue(json.get<double>());

                case Json::value_t::string:
                    return vm->createString(json.get_ref<const std::string&>());

                case Json::value_t::array:
                {
                    Value list = vm->createVector();
                    for (const auto& element : json)
                    {
                        Value child = toValue(vm, element);
                        vm->asVector(list)->fields.push_back(child);
                    }
                    return list;
                }

                case Json::value_t::object:
                {
                    Value table = vm->createTable();
                    for (const auto& [key, element] : json.items())
                    {
                        Value child = toValue(vm, element);
                        vm->asInstance(table)->setField(key, child, vm, true);
                    }
                    return table;
                }

                default:
                    throw std::runtime_error("json.parse: a value JSON text cannot hold");
            }
        }

        // The line and column (from 1) of byte `byte` (from 1, as nlohmann counts) of `text`
        std::string whereIn(const std::string& text, size_t byte)
        {
            const size_t at = std::min(byte > 0 ? byte - 1 : 0, text.size());

            size_t line = 1;
            size_t lineStart = 0;
            for (size_t i = 0; i < at; ++i)
            {
                if (text[i] == '\n')
                {
                    ++line;
                    lineStart = i + 1;
                }
            }

            return "line " + std::to_string(line) + ", column " + std::to_string(at - lineStart + 1);
        }

        // What nlohmann says went wrong, without its own prefix and position
        std::string reasonOf(const std::string& what)
        {
            const auto column = what.find("column");
            const auto colon = what.find(": ", column == std::string::npos ? 0 : column);
            return colon == std::string::npos ? what : what.substr(colon + 2);
        }

        // A PgScript value as JSON. `path` holds the lists and tables being written, to refuse one
        // that contains itself
        Json toJson(VM* vm, Value value, int depth, std::unordered_set<const void*>& path)
        {
            if (depth > JsonModule::MAX_DEPTH)
                throw std::runtime_error("json.stringify: nested deeper than " + std::to_string(JsonModule::MAX_DEPTH));

            if (IS_NONE(value))
                return nullptr;

            if (IS_BOOL(value))
                return AS_BOOL(value);

            if (IS_INT(value))
                return AS_INT(value);

            if (IS_DOUBLE(value))
            {
                const double number = AS_DOUBLE(value);
                if (not std::isfinite(number))
                    throw std::runtime_error("json.stringify: a number that is not finite has no JSON text");
                return number;
            }

            if (IS_STRING(value))
                return vm->asString(value);

            if (IS_VECTOR(value))
            {
                ObjVector* list = vm->asVector(value);
                if (not path.insert(list).second)
                    throw std::runtime_error("json.stringify: a list contains itself");

                Json array = Json::array();
                for (Value element : list->fields)
                    array.push_back(toJson(vm, element, depth + 1, path));

                path.erase(list);
                return array;
            }

            if (IS_INSTANCE(value))
            {
                ObjInstance* table = vm->asInstance(value);
                if (table->klass != vm->asClass(vm->tableClassValue()))
                    throw std::runtime_error("json.stringify: an instance of class '" + (table->klass ? table->klass->name : std::string("?")) + "' is not a table");

                if (not path.insert(table).second)
                    throw std::runtime_error("json.stringify: a table contains itself");

                // As the table holds them: the integer keys in ascending order, then the fields
                Json object = Json::object();
                for (size_t rank = 0; rank < table->intCount(); ++rank)
                    object[std::to_string(table->intKeyAt(rank))] = toJson(vm, table->intValueAt(rank), depth + 1, path);

                for (size_t slot = 0; slot < table->fieldCount(); ++slot)
                {
                    const Value field = table->fieldValue(slot);
                    if (IS_UNDEFINED(field))
                        continue;

                    // t[5] and t["5"] are two keys of the table but one in JSON: refused, never one dropped
                    const std::string& name = table->fieldName(slot);
                    if (object.contains(name))
                        throw std::runtime_error("json.stringify: the table has both the key " + name + " and the key \"" + name + "\", which are one key in JSON");

                    object[name] = toJson(vm, field, depth + 1, path);
                }

                path.erase(table);
                return object;
            }

            throw std::runtime_error("json.stringify: only none, bools, numbers, strings, lists and tables can be written (a function or a class cannot)");
        }
    }

    Value JsonModule::nativeParse(VM* vm, int argCount, Value* args)
    {
        if (argCount != 1 or not IS_STRING(args[0]))
            throw std::runtime_error("json.parse expects one string");

        const std::string text = vm->asString(args[0]);

        Json parsed;
        try
        {
            parsed = Json::parse(text, [](int depth, Json::parse_event_t, Json&) {
                if (depth > MAX_DEPTH)
                    throw TooDeep{};
                return true;
            });
        }
        catch (const TooDeep&)
        {
            throw std::runtime_error("json.parse: nested deeper than " + std::to_string(MAX_DEPTH));
        }
        catch (const Json::parse_error& e)
        {
            throw std::runtime_error("json.parse: " + whereIn(text, e.byte) + ": " + reasonOf(e.what()));
        }

        return toValue(vm, parsed);
    }

    Value JsonModule::nativeStringify(VM* vm, int argCount, Value* args)
    {
        if (argCount != 1 and argCount != 2)
            throw std::runtime_error("json.stringify expects a value and, optionally, an indent");

        int indent = -1;
        if (argCount == 2)
        {
            if (not IS_INT(args[1]) or AS_INT(args[1]) < 0 or AS_INT(args[1]) > 16)
                throw std::runtime_error("json.stringify: the indent is an int from 0 to 16");
            indent = static_cast<int>(AS_INT(args[1]));
        }

        std::unordered_set<const void*> path;
        const Json json = toJson(vm, args[0], 0, path);

        // Bytes that are not UTF-8 become U+FFFD instead of stopping the write
        return vm->createString(json.dump(indent, ' ', false, Json::error_handler_t::replace));
    }
}
