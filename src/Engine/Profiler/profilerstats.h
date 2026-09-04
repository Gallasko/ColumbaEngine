#pragma once

// Per-ECS-pass statistics recorder: named event counts, entity count and
// per-component-type instance counts. Complements the interval-based
// Profiler with counter-style data, on the same #ifdef PROFILE gate.
//
// The header is always safe to include; the class only exists in
// PROFILE builds.

#ifdef PROFILE

#include <cstdint>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ECS/entitysystem_fwd.h"

namespace pg
{
    // Snapshot of all counters for one ECS pass (one taskflow graph iteration)
    struct PassStats
    {
        uint64_t pass = 0;
        double timestampMs = 0.0;
        size_t entityCount = 0;

        /** Demangled event name -> number of dispatches during this pass */
        std::vector<std::pair<std::string, uint32_t>> eventCounts;

        /** Component type name -> instance count (sampled at a lower cadence;
         *  empty when this pass was not a sampling pass) */
        std::vector<std::pair<std::string, size_t>> componentCounts;
    };

    class ProfilerStats
    {
    public:
        static ProfilerStats& instance();

        /** Count one dispatch of a typed event. Callable from any thread
         *  (event dispatch can happen at any call site, on any worker). */
        void countEvent(_unique_id id, const char* mangledName);

        /** Same as countEvent but also returns the demangled event name
         *  (single lock), for labelling the dispatch profiler scope. */
        std::string countEventGetName(_unique_id id, const char* mangledName);

        /** Count one dispatch of a StandardEvent (identity = runtime name). */
        void countStandardEvent(const std::string& name);

        /** Attach component instance counts to the next finalized pass. */
        void setComponentCounts(std::vector<std::pair<std::string, size_t>>&& counts);

        /** Swap the accumulated counters into the pass ring. Must only be
         *  called from the BasicTask (once per graph iteration). The counts
         *  cover everything recorded since the previous finalizePass. */
        void finalizePass(uint64_t pass, double timestampMs, size_t entityCount);

        /** Copy of the most recent n finalized passes (oldest first). */
        std::vector<PassStats> lastPasses(size_t n) const;

        /** Sum of all event counts recorded since construction/clear,
         *  by demangled name (used by reportSystemProfiles). */
        std::vector<std::pair<std::string, uint64_t>> totalEventCounts() const;

        /** pass,entity_count,kind,name,count rows for every ring entry. */
        void exportToCSV(const std::string& filename) const;

        void clear();

    private:
        ProfilerStats() = default;

        /** Demangled name for a typed event id (cached). Lock must be held. */
        const std::string& eventName(_unique_id id, const char* mangledName);

        mutable std::mutex mtx;

        std::unordered_map<_unique_id, uint32_t> typedCounts;
        std::unordered_map<_unique_id, std::string> idNames;
        std::unordered_map<std::string, uint32_t> standardCounts;

        std::unordered_map<std::string, uint64_t> cumulativeCounts;

        std::vector<std::pair<std::string, size_t>> pendingComponentCounts;

        std::deque<PassStats> ring;
        static constexpr size_t maxPasses = 1024;
    };
}

#endif // PROFILE
