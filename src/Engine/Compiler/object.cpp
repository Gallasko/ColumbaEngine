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

                return slot;
            }

            toDictionary();
        }

        if (not dictionary)
            dictionary = std::make_unique<Dictionary>();

        fieldValueList.push_back(value);
        dictionary->index[name] = slot;
        dictionary->names.push_back(name);

        return slot;
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