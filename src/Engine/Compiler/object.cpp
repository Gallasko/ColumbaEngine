#include "stdafx.h"

#include "object.h"

#include "chunk.h"

#include "vm.h"

#include <algorithm>

namespace pg
{
    Closure::Closure(ObjFunction* func) : function(func)
    {
        upvalues.resize(func->upvalueCount);
    }

    const std::string Shape::setFieldName = "__set";

    void Shape::build()
    {
        // The tree's indexes all let go when this one would take them past their budget
        if (tree and tree->builtNames + depth > ShapeTree::maxBuiltNames)
            tree->unbuildAll();

        names.assign(depth, nullptr);
        index.reserve(depth);

        for (Shape* s = this; s->depth > 0; s = s->parent)
            names[s->depth - 1] = &s->lastName;

        // A shape never holds a name twice (a table that would goes off the shapes)
        for (uint32_t slot = 0; slot < depth; ++slot)
            index.emplace(*names[slot], slot);

        built = true;

        if (tree)
            tree->builtNames += depth;
    }

    void Shape::unbuild()
    {
        if (not built)
            return;

        if (tree)
            tree->builtNames -= depth;

        // (swapped with empties: clear() would keep what they had allocated)
        std::vector<const std::string*>().swap(names);
        std::unordered_map<std::string_view, uint32_t>().swap(index);

        built = false;
        queries = 0;
    }

    void ShapeTree::unbuildAll()
    {
        root.unbuild();

        for (auto& shape : owned)
            shape->unbuild();
    }

    Shape* ShapeTree::next(Shape* from, const std::string& name)
    {
        auto it = from->next.find(name);

        if (it != from->next.end())
            return it->second;

        const size_t wide = from == &root ? maxNextAtRoot : maxNext;

        if (from->depth >= maxDepth or from->next.size() >= wide or owned.size() >= maxShapes)
            return nullptr;

        auto shape = std::make_unique<Shape>();
        shape->parent = from;
        shape->tree = this;
        shape->lastName = name;
        shape->depth = from->depth + 1;

        Shape* made = shape.get();
        owned.push_back(std::move(shape));
        from->next.emplace(name, made);

        return made;
    }

    void ObjInstance::toDictionary()
    {
        auto dict = std::make_unique<Dictionary>();

        if (shape)
        {
            dict->names.reserve(shape->depth);
            dict->index.reserve(shape->depth);

            for (size_t slot = 0; slot < shape->depth; ++slot)
            {
                dict->names.push_back(shape->nameAt(slot));
                dict->index[dict->names.back()] = slot;
            }
        }

        dictionary = std::move(dict);
        shape = nullptr;
    }

    size_t ObjInstance::appendNewField(const std::string& name, Value value)
    {
        const size_t slot = fieldValueList.size();

        if (shape)
        {
            if (Shape* next = shapes->next(shape, name))
            {
                shape = next;
                fieldValueList.push_back(value);
                noteFieldCount();

                return slot;
            }

            toDictionary();
        }

        if (not dictionary)
            dictionary = std::make_unique<Dictionary>();

        fieldValueList.push_back(value);
        noteFieldCount();
        dictionary->index[name] = slot;
        dictionary->names.push_back(name);

        return slot;
    }

    void ObjInstance::addInt(int64_t key, Value value)
    {
        if (not ints)
        {
            ints = std::make_unique<IntKeys>();
            ints->base = key;
            ints->run.push_back(value);
            return;
        }

        const int64_t end = ints->base + static_cast<int64_t>(ints->run.size());

        if (key != end)
        {
            ints->others.emplace(key, value);
            ints->ordered = false;
            return;
        }

        ints->run.push_back(value);

        // The run reaches keys written before it: they join it
        for (int64_t next = key + 1; not ints->others.empty(); ++next)
        {
            auto it = ints->others.find(next);

            if (it == ints->others.end())
                break;

            ints->run.push_back(it->second);
            ints->others.erase(it);
            ints->ordered = false;
        }
    }

    void ObjInstance::setInt(int64_t key, Value value, VM* vm)
    {
        if (Value* found = findInt(key))
        {
            vm->releaseAndDelete(*found);
            *found = value;
        }
        else
        {
            addInt(key, value);
        }
    }

    void ObjInstance::IntKeys::sortOthers()
    {
        order.clear();
        order.reserve(others.size());

        for (const auto& pair : others)
            order.push_back(pair.first);

        std::sort(order.begin(), order.end());
        below = static_cast<size_t>(std::lower_bound(order.begin(), order.end(), base) - order.begin());
        ordered = true;
    }

    int64_t ObjInstance::intKeyAt(size_t rank) const
    {
        if (not ints->ordered)
            ints->sortOthers();

        if (rank < ints->below)
            return ints->order[rank];

        rank -= ints->below;

        if (rank < ints->run.size())
            return ints->base + static_cast<int64_t>(rank);

        return ints->order[ints->below + rank - ints->run.size()];
    }

    Value& ObjInstance::intValueAt(size_t rank)
    {
        const int64_t key = intKeyAt(rank);

        return *findInt(key);
    }

    size_t ObjInstance::appendField(const std::string& name, Value value)
    {
        // A name written twice keeps both slots: off the shapes, whose names are each once
        if (shape and shape->slotOf(name) != npos)
            toDictionary();

        return appendNewField(name, value);
    }

    void ObjInstance::setField(const std::string& name, Value value, VM *vm, bool deleteOld)
    {
        const size_t slot = findField(name);

        if (slot != npos)
        {
            if (deleteOld and vm)
            {
                // Release old value if it requires ref counting
                Value oldValue = fieldValueList[slot];
                if (requiresRefCount(oldValue))
                {
                    vm->releaseValue(oldValue);
                }
            }

            // Update existing field
            fieldValueList[slot] = value;
        }
        else
        {
            appendNewField(name, value);
        }
    }
}