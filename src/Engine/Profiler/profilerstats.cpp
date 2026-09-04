#ifdef PROFILE

#include "profilerstats.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "Helpers/demangle.h"

namespace pg
{
    ProfilerStats& ProfilerStats::instance()
    {
        static ProfilerStats stats;
        return stats;
    }

    const std::string& ProfilerStats::eventName(_unique_id id, const char* mangledName)
    {
        auto it = idNames.find(id);

        if (it == idNames.end())
            it = idNames.emplace(id, prettyTypeName(mangledName)).first;

        return it->second;
    }

    void ProfilerStats::countEvent(_unique_id id, const char* mangledName)
    {
        std::lock_guard<std::mutex> lock(mtx);

        typedCounts[id]++;
        cumulativeCounts[eventName(id, mangledName)]++;
    }

    void ProfilerStats::countStandardEvent(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(mtx);

        standardCounts[name]++;
        cumulativeCounts[name]++;
    }

    void ProfilerStats::setComponentCounts(std::vector<std::pair<std::string, size_t>>&& counts)
    {
        std::lock_guard<std::mutex> lock(mtx);

        pendingComponentCounts = std::move(counts);
    }

    void ProfilerStats::finalizePass(uint64_t pass, double timestampMs, size_t entityCount)
    {
        std::lock_guard<std::mutex> lock(mtx);

        PassStats stats;
        stats.pass = pass;
        stats.timestampMs = timestampMs;
        stats.entityCount = entityCount;

        stats.eventCounts.reserve(typedCounts.size() + standardCounts.size());

        for (const auto& [id, count] : typedCounts)
        {
            // idNames always holds the id by now: countEvent inserts it
            stats.eventCounts.emplace_back(idNames.at(id), count);
        }

        for (const auto& [name, count] : standardCounts)
        {
            stats.eventCounts.emplace_back(name, count);
        }

        std::sort(stats.eventCounts.begin(), stats.eventCounts.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

        stats.componentCounts = std::move(pendingComponentCounts);
        pendingComponentCounts.clear();

        typedCounts.clear();
        standardCounts.clear();

        ring.push_back(std::move(stats));

        if (ring.size() > maxPasses)
            ring.pop_front();
    }

    std::vector<PassStats> ProfilerStats::lastPasses(size_t n) const
    {
        std::lock_guard<std::mutex> lock(mtx);

        const size_t count = std::min(n, ring.size());

        return std::vector<PassStats>(ring.end() - count, ring.end());
    }

    std::vector<std::pair<std::string, uint64_t>> ProfilerStats::totalEventCounts() const
    {
        std::lock_guard<std::mutex> lock(mtx);

        std::vector<std::pair<std::string, uint64_t>> totals(cumulativeCounts.begin(), cumulativeCounts.end());

        std::sort(totals.begin(), totals.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

        return totals;
    }

    void ProfilerStats::exportToCSV(const std::string& filename) const
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for profiler stats export: " << filename << std::endl;
            return;
        }

        std::lock_guard<std::mutex> lock(mtx);

        file << "pass,timestamp_ms,entity_count,kind,name,count\n";

        for (const auto& stats : ring)
        {
            for (const auto& [name, count] : stats.eventCounts)
            {
                file << stats.pass << "," << stats.timestampMs << "," << stats.entityCount
                     << ",event,\"" << name << "\"," << count << "\n";
            }

            for (const auto& [name, count] : stats.componentCounts)
            {
                file << stats.pass << "," << stats.timestampMs << "," << stats.entityCount
                     << ",component,\"" << name << "\"," << count << "\n";
            }
        }

        std::cout << "Profiler stats exported to: " << filename << " (" << ring.size() << " passes)" << std::endl;
    }

    void ProfilerStats::clear()
    {
        std::lock_guard<std::mutex> lock(mtx);

        typedCounts.clear();
        standardCounts.clear();
        cumulativeCounts.clear();
        pendingComponentCounts.clear();
        ring.clear();
    }
}

#endif // PROFILE
