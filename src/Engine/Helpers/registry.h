#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace pg
{

template <typename DefType, typename IdType = size_t>
struct Registry
{
    IdType add(const DefType& def)
    {
        IdType assignedId = static_cast<IdType>(m_entries.size());

        m_nameToId[def.name] = assignedId;
        m_entries.push_back(def);

        return assignedId;
    }

    IdType idOf(const std::string& name) const
    {
        auto it = m_nameToId.find(name);
        assert(it != m_nameToId.end() and "Registry::idOf() - name not found");

        return it->second;
    }

    IdType add(const DefType& def, size_t secondaryKey)
    {
        IdType assignedId = add(def);
        m_secondaryIndex[secondaryKey] = assignedId;

        return assignedId;
    }

    const DefType& get(IdType id) const
    {
        assert(static_cast<size_t>(id) < m_entries.size() and "Registry::get() - id out of bounds");
        return m_entries[static_cast<size_t>(id)];
    }

    const DefType* tryGet(IdType id) const
    {
        if (static_cast<size_t>(id) >= m_entries.size())
            return nullptr;

        return &m_entries[static_cast<size_t>(id)];
    }

    const DefType* findByName(const std::string& name) const
    {
        auto it = m_nameToId.find(name);
        if (it != m_nameToId.end())
            return &m_entries[static_cast<size_t>(it->second)];

        return nullptr;
    }

    const DefType* findByKey(size_t key) const
    {
        auto it = m_secondaryIndex.find(key);
        if (it != m_secondaryIndex.end())
            return &m_entries[static_cast<size_t>(it->second)];

        return nullptr;
    }

    size_t count() const { return m_entries.size(); }

    const std::vector<DefType>& all() const { return m_entries; }

    std::vector<DefType>                    m_entries;
    std::unordered_map<std::string, IdType> m_nameToId;
    std::unordered_map<size_t, IdType>      m_secondaryIndex;
};

} // namespace pg
