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

#include <unordered_map>
#include <functional>

namespace pg
{
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