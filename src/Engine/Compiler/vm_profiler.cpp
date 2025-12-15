#include "vm_profiler.h"

#include <iostream>
#include <iomanip>

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
}
