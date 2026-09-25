/**
 * @file helpers.h
 * @author Pigeon Codeur (pigeoncodeur@gmail.com)
 * @brief Store all global templates and functions that can ease some process
 * @version 0.1
 * @date 2025-01-10
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

namespace pg
{
    // Drain a pending update set (clearing it) and return its elements that are
    // also present in the sorted `sortedFilter` vector, in sorted order.
    // Used by render systems to intersect queued entity updates with the entities
    // currently in the render group.
    template <typename T>
    std::vector<T> drainIntersectSorted(std::unordered_set<T>& pending, const std::vector<T>& sortedFilter)
    {
        std::vector<T> temp(pending.begin(), pending.end());
        std::sort(temp.begin(), temp.end());
        pending.clear();

        std::vector<T> out;
        std::set_intersection(sortedFilter.begin(), sortedFilter.end(),
                              temp.begin(), temp.end(),
                              std::back_inserter(out));

        return out;
    }

    // Function which invert an unordered map
    template<typename Kin, typename Vin>
    std::unordered_map<Vin, Kin> invertMap(const std::unordered_map<Kin, Vin>& inMap)
    {
        auto mapFunc = [](const std::pair<Kin, Vin>& p) {
            return std::make_pair(p.second, p.first);
        };

        std::unordered_map<Vin, Kin> outMap;

        std::for_each(inMap.begin(), inMap.end(),
            [&outMap, &mapFunc] (const std::pair<Kin, Vin> &p) {
                outMap.insert(mapFunc(p));
            }
        );

        return outMap;
    }
}