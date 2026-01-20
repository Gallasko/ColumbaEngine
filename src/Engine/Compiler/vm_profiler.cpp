#include "stdafx.h"
#include "vm_profiler.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>

namespace pg
{
    void VMProfiler::printReport(bool sortByTime) const
    {
        if (profiles.empty())
        {
            std::cout << "No profiling data collected" << std::endl;
            return;
        }

        auto results = sortByTime ? getResultsSortedByTime() : getResultsSortedByCount();

        uint64_t totalTime = getTotalTimeNs();

        std::cout << "\n=== VM Bytecode Profiling Report ===" << std::endl;

        // Print startup timing information
        if (setupVmTimeNs > 0 || preRunTimeNs > 0)
        {
            std::cout << "\n--- Startup Timing ---" << std::endl;
            std::cout << "setupVm() time:          " << std::setw(12) << std::fixed << std::setprecision(6)
                      << (setupVmTimeNs / 1'000'000.0) << " ms" << std::endl;
            std::cout << "Pre-run time:            " << std::setw(12) << std::fixed << std::setprecision(6)
                      << (preRunTimeNs / 1'000'000.0) << " ms" << std::endl;
            std::cout << "Total startup time:      " << std::setw(12) << std::fixed << std::setprecision(6)
                      << (getTotalStartupTimeNs() / 1'000'000.0) << " ms" << std::endl;
            std::cout << std::endl;
        }

        std::cout << "--- Runtime Execution ---" << std::endl;
        std::cout << "Total instructions executed: " << totalInstructions << std::endl;
        std::cout << "Total execution time: " << (totalTime / 1'000'000.0) << " ms" << std::endl;
        std::cout << "Average time per instruction: " << (static_cast<double>(totalTime) / totalInstructions) << " ns" << std::endl;
        std::cout << "\nTop instructions by " << (sortByTime ? "total time" : "execution count") << ":\n" << std::endl;

        std::cout << std::left
                  << std::setw(20) << "Function"
                  << std::setw(8) << "Offset"
                  << std::setw(25) << "Opcode"
                  << std::setw(12) << "Count"
                  << std::setw(15) << "Total (ms)"
                  << std::setw(15) << "Avg (ns)"
                  << std::setw(10) << "% Time"
                  << std::endl;

        std::cout << std::string(105, '-') << std::endl;

        for (const auto& profile : results)
        {
            double percentage = totalTime > 0 ? (static_cast<double>(profile.totalNanoseconds) / totalTime * 100.0) : 0.0;

            std::cout << std::left
                      << std::setw(20) << profile.functionName
                      << std::setw(8) << profile.instructionOffset
                      << std::setw(25) << profile.opcodeName
                      << std::setw(12) << profile.executionCount
                      << std::setw(15) << std::fixed << std::setprecision(6) << profile.totalTimeMs()
                      << std::setw(15) << std::fixed << std::setprecision(2) << profile.averageTimeNs()
                      << std::setw(10) << std::fixed << std::setprecision(2) << percentage << "%"
                      << std::endl;
        }

        std::cout << std::endl;
    }

    void VMProfiler::printBytecodeReport() const
    {
        if (profiles.empty())
        {
            std::cout << "No profiling data collected" << std::endl;
            return;
        }

        auto results = getResultsSortedByOffset();

        uint64_t totalTime = getTotalTimeNs();

        std::cout << "\n=== VM Bytecode Execution Profile (by instruction order) ===" << std::endl;
        std::cout << "Total instructions executed: " << totalInstructions << std::endl;
        std::cout << "Total execution time: " << (totalTime / 1'000'000.0) << " ms" << std::endl;
        std::cout << "\nInstruction-by-instruction breakdown:\n" << std::endl;

        std::cout << std::left
                  << std::setw(20) << "Function"
                  << std::setw(8) << "Offset"
                  << std::setw(30) << "Opcode"
                  << std::setw(15) << "Exec Count"
                  << std::setw(15) << "Total (μs)"
                  << std::setw(15) << "Avg (ns)"
                  << std::endl;

        std::cout << std::string(103, '-') << std::endl;

        for (const auto& profile : results)
        {
            std::cout << std::left
                      << std::setw(20) << profile.functionName
                      << std::setw(8) << profile.instructionOffset
                      << std::setw(30) << profile.opcodeName
                      << std::setw(15) << profile.executionCount
                      << std::setw(15) << std::fixed << std::setprecision(3) << profile.totalTimeUs()
                      << std::setw(15) << std::fixed << std::setprecision(2) << profile.averageTimeNs()
                      << std::endl;
        }

        std::cout << std::endl;
    }

    void VMProfiler::printBytecodeWithPerformance(const std::string& functionName) const
    {
        if (!enabled)
        {
            std::cout << "Profiling is not enabled" << std::endl;
            return;
        }

        uint64_t totalTime = getTotalTimeNs();

        // ANSI color codes
        const char* RED = "\033[31m";
        const char* ORANGE = "\033[33m";
        const char* RESET = "\033[0m";

        std::cout << "\n=== Bytecode with Performance Data: " << functionName << " ===" << std::endl;
        std::cout << "\nFormat: [Time(μs)] [%Total] [Count] | Offset | Opcode" << std::endl;
        std::cout << RED << "Red = Top 3 hottest" << RESET << " | "
                  << ORANGE << "Orange = Next 3 hottest" << RESET << std::endl;
        std::cout << std::string(100, '-') << std::endl;

        // Collect all profile entries for this function
        std::vector<InstructionProfile> functionProfiles;
        for (const auto& [key, profile] : profiles)
        {
            if (profile.functionName == functionName)
            {
                functionProfiles.push_back(profile);
            }
        }

        // Create a sorted-by-time copy to identify top hotspots
        std::vector<InstructionProfile> sortedByTime = functionProfiles;
        std::sort(sortedByTime.begin(), sortedByTime.end(),
            [](const InstructionProfile& a, const InstructionProfile& b) {
                return a.totalNanoseconds > b.totalNanoseconds;
            });

        // Get the offsets of top 3 and next 3 hottest instructions
        std::unordered_set<size_t> top3Offsets;
        std::unordered_set<size_t> next3Offsets;

        for (size_t i = 0; i < sortedByTime.size() && i < 3; ++i)
        {
            top3Offsets.insert(sortedByTime[i].instructionOffset);
        }
        for (size_t i = 3; i < sortedByTime.size() && i < 6; ++i)
        {
            next3Offsets.insert(sortedByTime[i].instructionOffset);
        }

        // Sort by offset for display
        std::sort(functionProfiles.begin(), functionProfiles.end(),
            [](const InstructionProfile& a, const InstructionProfile& b) {
                return a.instructionOffset < b.instructionOffset;
            });

        // Print each instruction with its performance data and color coding
        for (const auto& profile : functionProfiles)
        {
            double timeUs = profile.totalTimeUs();
            double percentage = totalTime > 0 ? (static_cast<double>(profile.totalNanoseconds) / totalTime * 100.0) : 0.0;

            // Determine color
            const char* color = RESET;
            if (top3Offsets.count(profile.instructionOffset))
            {
                color = RED;
            }
            else if (next3Offsets.count(profile.instructionOffset))
            {
                color = ORANGE;
            }

            std::cout << color
                      << "[" << std::setw(10) << std::fixed << std::setprecision(3) << timeUs << "]"
                      << " [" << std::setw(6) << std::fixed << std::setprecision(2) << percentage << "%]"
                      << " [" << std::setw(8) << profile.executionCount << "] | "
                      << std::setw(6) << profile.instructionOffset << " | "
                      << profile.opcodeName << RESET << std::endl;
        }

        std::cout << std::string(100, '-') << std::endl;
        std::cout << "Total execution time: " << (totalTime / 1'000.0) << " μs" << std::endl;
        std::cout << std::endl;
    }
}
