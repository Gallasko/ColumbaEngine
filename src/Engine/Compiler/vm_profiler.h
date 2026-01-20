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
     * Tracks execution count and time spent for each instruction BY POSITION in bytecode AND chunk
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
            const void* chunkPtr = nullptr;  // Pointer to chunk (for uniqueness)
            std::string functionName;        // Name of the function/chunk
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

        // Key for uniquely identifying an instruction: (chunk pointer, offset)
        struct InstructionKey
        {
            const void* chunkPtr;
            size_t offset;

            bool operator==(const InstructionKey& other) const
            {
                return chunkPtr == other.chunkPtr && offset == other.offset;
            }
        };

        // Hash function for InstructionKey
        struct InstructionKeyHash
        {
            std::size_t operator()(const InstructionKey& key) const
            {
                // Combine hash of pointer and offset
                std::size_t h1 = std::hash<const void*>{}(key.chunkPtr);
                std::size_t h2 = std::hash<size_t>{}(key.offset);
                return h1 ^ (h2 << 1);
            }
        };

        VMProfiler() : enabled(false), totalInstructions(0), setupVmTimeNs(0), preRunTimeNs(0) {}

        /**
         * @brief Enable or disable profiling
         */
        void setEnabled(bool enable) { enabled = enable; }

        bool isEnabled() const { return enabled; }

        /**
         * @brief Record execution of an instruction at a specific bytecode position in a specific chunk
         * @param chunkPtr Pointer to the chunk (for uniqueness across functions)
         * @param functionName Name of the function/chunk
         * @param instructionOffset The position/offset in the bytecode stream
         * @param opcode The opcode being executed
         * @param opcodeName The name of the opcode for display
         * @param nanoseconds Time spent executing this instruction
         */
        inline void recordInstruction(const void* chunkPtr, const std::string& functionName, size_t instructionOffset,
                                      uint8_t opcode, const std::string& opcodeName, uint64_t nanoseconds)
        {
            if (!enabled) return;

            InstructionKey key{chunkPtr, instructionOffset};
            auto& profile = profiles[key];

            // Initialize on first recording
            if (profile.executionCount == 0)
            {
                profile.opcode = opcode;
                profile.instructionOffset = instructionOffset;
                profile.chunkPtr = chunkPtr;
                profile.functionName = functionName;
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
            setupVmTimeNs = 0;
            preRunTimeNs = 0;
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
         * @brief Get profiling results sorted by function name, then by instruction offset (bytecode order)
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
                    // First sort by function name
                    if (a.functionName != b.functionName)
                        return a.functionName < b.functionName;
                    // Then by instruction offset within the same function
                    return a.instructionOffset < b.instructionOffset;
                });

            return results;
        }

        /**
         * @brief Get profile for specific instruction in a specific chunk
         */
        const InstructionProfile* getProfile(const void* chunkPtr, size_t instructionOffset) const
        {
            InstructionKey key{chunkPtr, instructionOffset};
            auto it = profiles.find(key);
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
         * @brief Record time spent in setupVm()
         * @param nanoseconds Time spent in setupVm (native module registration, etc.)
         */
        void recordSetupVmTime(uint64_t nanoseconds)
        {
            setupVmTimeNs = nanoseconds;
        }

        /**
         * @brief Record time from end of setupVm until run()
         * @param nanoseconds Time spent in compilation and pre-run setup
         */
        void recordPreRunTime(uint64_t nanoseconds)
        {
            preRunTimeNs = nanoseconds;
        }

        /**
         * @brief Get total VM startup time (setupVm + preRun)
         */
        uint64_t getTotalStartupTimeNs() const
        {
            return setupVmTimeNs + preRunTimeNs;
        }

        /**
         * @brief Get time spent in setupVm
         */
        uint64_t getSetupVmTimeNs() const
        {
            return setupVmTimeNs;
        }

        /**
         * @brief Get time spent before run()
         */
        uint64_t getPreRunTimeNs() const
        {
            return preRunTimeNs;
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
        std::unordered_map<InstructionKey, InstructionProfile, InstructionKeyHash> profiles;
        uint64_t totalInstructions;
        uint64_t setupVmTimeNs;      // Time spent in setupVm()
        uint64_t preRunTimeNs;        // Time from end of setupVm until run()
    };
}
