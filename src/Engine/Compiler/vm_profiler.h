#pragma once

#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace pg
{
    /**
     * @brief Profiler for VM bytecode execution
     *
     * Tracks execution count and time spent for each instruction BY POSITION in bytecode
     */
    class VMProfiler
    {
    public:
        struct InstructionProfile
        {
            uint64_t executionCount = 0;
            uint64_t totalNanoseconds = 0;
            uint8_t opcode = 0;
            size_t instructionOffset = 0;  // Position in bytecode stream
            std::string opcodeName;

            double averageTimeNs() const
            {
                return executionCount > 0 ? static_cast<double>(totalNanoseconds) / executionCount : 0.0;
            }

            double totalTimeMs() const
            {
                return totalNanoseconds / 1'000'000.0;
            }

            double totalTimeUs() const
            {
                return totalNanoseconds / 1'000.0;
            }
        };

        VMProfiler() : enabled(false), totalInstructions(0) {}

        /**
         * @brief Enable or disable profiling
         */
        void setEnabled(bool enable) { enabled = enable; }

        bool isEnabled() const { return enabled; }

        /**
         * @brief Record execution of an instruction at a specific bytecode position
         * @param instructionOffset The position/offset in the bytecode stream
         * @param opcode The opcode being executed
         * @param opcodeName The name of the opcode for display
         * @param nanoseconds Time spent executing this instruction
         */
        inline void recordInstruction(size_t instructionOffset, uint8_t opcode, const std::string& opcodeName, uint64_t nanoseconds)
        {
            if (!enabled) return;

            auto& profile = profiles[instructionOffset];

            // Initialize on first recording
            if (profile.executionCount == 0)
            {
                profile.opcode = opcode;
                profile.instructionOffset = instructionOffset;
                profile.opcodeName = opcodeName;
            }

            profile.executionCount++;
            profile.totalNanoseconds += nanoseconds;
            totalInstructions++;
        }

        /**
         * @brief Reset all profiling data
         */
        void reset()
        {
            profiles.clear();
            totalInstructions = 0;
        }

        /**
         * @brief Get profiling results sorted by total time
         */
        std::vector<InstructionProfile> getResultsSortedByTime() const
        {
            std::vector<InstructionProfile> results;
            results.reserve(profiles.size());

            for (const auto& [offset, profile] : profiles)
            {
                results.push_back(profile);
            }

            std::sort(results.begin(), results.end(),
                [](const auto& a, const auto& b) {
                    return a.totalNanoseconds > b.totalNanoseconds;
                });

            return results;
        }

        /**
         * @brief Get profiling results sorted by execution count
         */
        std::vector<InstructionProfile> getResultsSortedByCount() const
        {
            std::vector<InstructionProfile> results;
            results.reserve(profiles.size());

            for (const auto& [offset, profile] : profiles)
            {
                results.push_back(profile);
            }

            std::sort(results.begin(), results.end(),
                [](const auto& a, const auto& b) {
                    return a.executionCount > b.executionCount;
                });

            return results;
        }

        /**
         * @brief Get profiling results sorted by instruction offset (bytecode order)
         */
        std::vector<InstructionProfile> getResultsSortedByOffset() const
        {
            std::vector<InstructionProfile> results;
            results.reserve(profiles.size());

            for (const auto& [offset, profile] : profiles)
            {
                results.push_back(profile);
            }

            std::sort(results.begin(), results.end(),
                [](const auto& a, const auto& b) {
                    return a.instructionOffset < b.instructionOffset;
                });

            return results;
        }

        /**
         * @brief Get profile for specific instruction offset
         */
        const InstructionProfile* getProfile(size_t instructionOffset) const
        {
            auto it = profiles.find(instructionOffset);
            return it != profiles.end() ? &it->second : nullptr;
        }

        /**
         * @brief Get total number of instructions executed
         */
        uint64_t getTotalInstructions() const { return totalInstructions; }

        /**
         * @brief Get total execution time in nanoseconds
         */
        uint64_t getTotalTimeNs() const
        {
            uint64_t total = 0;
            for (const auto& [offset, profile] : profiles)
            {
                total += profile.totalNanoseconds;
            }
            return total;
        }

        /**
         * @brief Print profiling report to stdout
         */
        void printReport(bool sortByTime = true) const;

        /**
         * @brief Print profiling report formatted for bytecode comparison
         * Shows instruction offset, opcode, execution count, and timing
         */
        void printBytecodeReport() const;

    private:
        bool enabled;
        std::unordered_map<size_t, InstructionProfile> profiles;
        uint64_t totalInstructions;
    };
}
