#include "stdafx.h"
#include "vm_profiler.h"

#include "Interpreter/lexer.h"
#include "Interpreter/parser.h"
#include "Files/filemanager.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <sstream>
#include <unordered_set>

namespace pg
{
    namespace
    {
        // A statement of the profile by source: it owns the lines from its own to the next statement at
        // its level, but those its children own
        struct SourceNode
        {
            int start = 0;          // Its first line, 0 when it says none (it owns no line of its own then)
            std::vector<SourceNode> children;
            uint64_t selfNs = 0;
            uint64_t totalNs = 0;
            uint64_t runs = 0;      // The most runs of one instruction on its own lines
        };

        struct LineCost
        {
            uint64_t ns = 0;
            uint64_t runs = 0;
        };

        // The first line written of an expression, 0 when it says none
        int lineOf(const ExprPtr& e)
        {
            if (not e)
                return 0;

            auto first = [](int a, int b) { return a != 0 ? a : b; };

            switch (e->kind())
            {
                case AstKind::BinaryExpression:  { auto x = static_cast<BinaryExpression*>(e.get());  return first(lineOf(x->leftExpr), x->op.line); }
                case AstKind::LogicExpression:   { auto x = static_cast<LogicExpression*>(e.get());   return first(lineOf(x->leftExpr), x->op.line); }
                case AstKind::UnaryExpression:   return static_cast<UnaryExpression*>(e.get())->op.line;
                case AstKind::PreFixExpression:  return static_cast<PreFixExpression*>(e.get())->op.line;
                case AstKind::PostFixExpression: { auto x = static_cast<PostFixExpression*>(e.get()); return first(lineOf(x->expr), x->op.line); }
                case AstKind::CompoundAtom:      return lineOf(static_cast<CompoundAtom*>(e.get())->expr);
                case AstKind::List:              return static_cast<List*>(e.get())->squareBracket.line;
                case AstKind::AnonymousFunction: return static_cast<AnonymousFunction*>(e.get())->token.line;
                case AstKind::IndexGet:          { auto x = static_cast<IndexGet*>(e.get());          return first(lineOf(x->object), x->bracket.line); }
                case AstKind::IndexSet:          { auto x = static_cast<IndexSet*>(e.get());          return first(lineOf(x->object), x->bracket.line); }
                case AstKind::This:              return static_cast<This*>(e.get())->name.line;
                case AstKind::Var:               return static_cast<Var*>(e.get())->name.line;
                case AstKind::Assign:            return static_cast<Assign*>(e.get())->name.line;
                case AstKind::CallExpression:    { auto x = static_cast<CallExpression*>(e.get());    return first(lineOf(x->caller), x->paren.line); }
                case AstKind::Get:               { auto x = static_cast<Get*>(e.get());               return first(lineOf(x->object), x->name.line); }
                case AstKind::Set:               { auto x = static_cast<Set*>(e.get());               return first(lineOf(x->object), x->name.line); }
                default:                         return 0;
            }
        }

        int tokenLineOf(const StatementPtr& s);

        // The first line of a statement: its first token's, else the one the parser kept (that one can
        // be a blank line before it, at the top level)
        int lineOf(const StatementPtr& s)
        {
            const int line = tokenLineOf(s);

            return line != 0 ? line : static_cast<int>(s->line);
        }

        int tokenLineOf(const StatementPtr& s)
        {
            switch (s->kind())
            {
                case AstKind::ExpressionStatement: return lineOf(static_cast<ExpressionStatement*>(s.get())->expr);
                case AstKind::VariableStatement:   return static_cast<VariableStatement*>(s.get())->name.line;
                case AstKind::FunctionStatement:   return static_cast<FunctionStatement*>(s.get())->name.line;
                case AstKind::ClassStatement:      return static_cast<ClassStatement*>(s.get())->name.line;
                case AstKind::ReturnStatement:     return static_cast<ReturnStatement*>(s.get())->name.line;
                case AstKind::BreakStatement:      return static_cast<BreakStatement*>(s.get())->token.line;
                case AstKind::ContinueStatement:   return static_cast<ContinueStatement*>(s.get())->token.line;
                case AstKind::DPrintStatement:     return static_cast<DPrintStatement*>(s.get())->token.line;
                case AstKind::ImportStatement:     return static_cast<ImportStatement*>(s.get())->name.line;
                case AstKind::IfStatement:         return lineOf(static_cast<IfStatement*>(s.get())->condition);
                case AstKind::WhileStatement:      return lineOf(static_cast<WhileStatement*>(s.get())->condition);
                case AstKind::ForInStatement:      return static_cast<ForInStatement*>(s.get())->varName.line;
                case AstKind::ForStatement:
                {
                    auto x = static_cast<ForStatement*>(s.get());
                    return x->initializer ? lineOf(x->initializer) : lineOf(x->condition);
                }
                default:                           return 0;
            }
        }

        // A block is no statement of its own: its statements go to whoever holds it
        void addStatement(std::vector<SourceNode>& into, const StatementPtr& stmt)
        {
            if (not stmt)
                return;

            if (stmt->kind() == AstKind::BlockStatement)
            {
                auto statements = static_cast<BlockStatement*>(stmt.get())->statements;

                for (; not statements.empty(); statements.pop())
                    addStatement(into, statements.front());

                return;
            }

            SourceNode node;
            node.start = lineOf(stmt);

            switch (stmt->kind())
            {
                case AstKind::FunctionStatement:
                    addStatement(node.children, static_cast<FunctionStatement*>(stmt.get())->body);
                    break;

                case AstKind::ClassStatement:
                {
                    auto methods = static_cast<ClassStatement*>(stmt.get())->methods;

                    for (; not methods.empty(); methods.pop())
                        addStatement(node.children, methods.front());

                    break;
                }

                case AstKind::IfStatement:
                {
                    auto x = static_cast<IfStatement*>(stmt.get());
                    addStatement(node.children, x->thenBranch);
                    // An `else if` is a child that starts on its own line
                    addStatement(node.children, x->elseBranch);
                    break;
                }

                case AstKind::WhileStatement: addStatement(node.children, static_cast<WhileStatement*>(stmt.get())->body); break;
                case AstKind::ForStatement:   addStatement(node.children, static_cast<ForStatement*>(stmt.get())->body);   break;
                case AstKind::ForInStatement: addStatement(node.children, static_cast<ForInStatement*>(stmt.get())->body); break;

                default:
                    break;
            }

            into.push_back(std::move(node));
        }

        // A line goes down to the last child started by then, past the parent's own line
        void place(SourceNode& root, int line, const LineCost& cost)
        {
            SourceNode* at = &root;

            while (true)
            {
                SourceNode* inner = nullptr;

                for (auto& child : at->children)
                {
                    if (child.start > at->start and child.start <= line)
                        inner = &child;
                }

                if (not inner)
                    break;

                at = inner;
            }

            at->selfNs += cost.ns;
            at->runs = std::max(at->runs, cost.runs);
        }

        uint64_t sumTotals(SourceNode& node)
        {
            node.totalNs = node.selfNs;

            for (auto& child : node.children)
                node.totalNs += sumTotals(child);

            return node.totalNs;
        }

        void collect(const SourceNode& node, std::vector<const SourceNode*>& out)
        {
            out.push_back(&node);

            for (const auto& child : node.children)
                collect(child, out);
        }

        struct SourcePrint
        {
            std::vector<std::string> lines;
            uint64_t runNs = 0;
            uint64_t minNs = 0;
            bool colored = false;   // Only on a terminal: a log file would read the escapes
            std::unordered_set<const SourceNode*> top3;
            std::unordered_set<const SourceNode*> next3;
        };

        std::string sourceText(const SourcePrint& print, int line)
        {
            if (line < 1 or line > static_cast<int>(print.lines.size()))
                return "?";

            const std::string& text = print.lines[line - 1];
            const size_t from = text.find_first_not_of(" \t");

            if (from == std::string::npos)
                return "";

            std::string trimmed = text.substr(from, text.find_last_not_of(" \t") - from + 1);

            if (trimmed.size() > 80)
                trimmed = trimmed.substr(0, 77) + "...";

            return trimmed;
        }

        bool stdoutIsTerminal()
        {
#ifdef _WIN32
            return _isatty(_fileno(stdout));
#else
            return isatty(fileno(stdout));
#endif
        }

        double percentOf(uint64_t ns, uint64_t of)
        {
            return of > 0 ? static_cast<double>(ns) / of * 100.0 : 0.0;
        }

        void printSourceRow(const SourcePrint& print, const SourceNode* node, const std::string& lineCell,
                            const std::string& label, int depth, uint64_t totalNs, uint64_t selfNs, uint64_t runs)
        {
            const char* color = not print.colored ? "" : print.top3.count(node) ? "\033[31m" : print.next3.count(node) ? "\033[33m" : "";
            const char* reset = color[0] != '\0' ? "\033[0m" : "";

            std::cout << color << std::right << std::fixed
                      << std::setw(8) << std::setprecision(2) << percentOf(totalNs, print.runNs) << "%"
                      << std::setw(11) << std::setprecision(3) << (totalNs / 1'000'000.0)
                      << std::setw(8) << std::setprecision(2) << percentOf(selfNs, print.runNs) << "%"
                      << std::setw(11) << (runs > 0 ? std::to_string(runs) : std::string("-"))
                      << std::setw(7) << lineCell << " | "
                      << std::string(depth * 2, ' ') << label << reset << std::endl;
        }

        void printSourceNode(const SourcePrint& print, const SourceNode& node, int depth);

        // The children that ran, those under the threshold folded into one row
        void printSourceChildren(const SourcePrint& print, const SourceNode& node, int depth)
        {
            size_t folded = 0;
            uint64_t foldedNs = 0;

            for (const auto& child : node.children)
            {
                if (child.totalNs == 0)
                    continue;

                if (child.totalNs < print.minNs)
                {
                    ++folded;
                    foldedNs += child.totalNs;
                    continue;
                }

                printSourceNode(print, child, depth);
            }

            if (folded > 0)
                printSourceRow(print, nullptr, "", "... " + std::to_string(folded) + " more", depth, foldedNs, foldedNs, 0);
        }

        void printSourceNode(const SourcePrint& print, const SourceNode& node, int depth)
        {
            printSourceRow(print, &node, std::to_string(node.start), sourceText(print, node.start),
                           depth, node.totalNs, node.selfNs, node.runs);

            printSourceChildren(print, node, depth + 1);
        }
    }

    void VMProfiler::printSourceReport(double minPercent) const
    {
        if (profiles.empty())
            return;

        // The cost of each line, file by file
        std::map<std::string, std::map<int, LineCost>> byFile;

        for (const auto& [key, profile] : profiles)
        {
            if (profile.sourceFile.empty())
                continue;

            auto& cost = byFile[profile.sourceFile][profile.line];
            cost.ns += profile.totalNanoseconds;
            cost.runs = std::max(cost.runs, profile.executionCount);
        }

        const uint64_t runNs = getTotalTimeNs();

        std::vector<std::pair<uint64_t, std::string>> files;

        for (const auto& [file, lines] : byFile)
        {
            uint64_t ns = 0;

            for (const auto& [line, cost] : lines)
                ns += cost.ns;

            files.emplace_back(ns, file);
        }

        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

        std::cout << "\n=== VM Profile by Source ===" << std::endl;
        std::cout << "Each statement owns the lines from its own to the next one at its level; Total is what is" << std::endl;
        std::cout << "written inside it (a call to a script function counts where that function is written)." << std::endl;
        std::cout << "% are of the whole run; statements under " << minPercent << "% are folded." << std::endl;

        for (const auto& [fileNs, file] : files)
        {
            SourcePrint print;
            print.runNs = runNs;
            print.minNs = static_cast<uint64_t>(runNs * minPercent / 100.0);
            print.colored = stdoutIsTerminal();

            const std::string source = UniversalFileAccessor::openTextFile(file).data;

            std::cout << "\n--- " << file << " ---" << std::endl;

            if (source.empty())
            {
                std::cout << "(its source could not be read)" << std::endl;
                continue;
            }

            {
                std::istringstream in(source);

                for (std::string line; std::getline(in, line);)
                {
                    if (not line.empty() and line.back() == '\r')
                        line.pop_back();

                    print.lines.push_back(line);
                }
            }

            std::queue<StatementPtr> statements;

            try
            {
                Lexer lexer;
                lexer.readFromText(source);

                Parser parser(lexer.getTokens());
                statements = parser.parse();

                if (parser.hasError())
                {
                    std::cout << "(its source does not parse any more)" << std::endl;
                    continue;
                }
            }
            catch (const std::exception& e)
            {
                std::cout << "(its source does not parse any more: " << e.what() << ")" << std::endl;
                continue;
            }

            SourceNode root;

            for (; not statements.empty(); statements.pop())
                addStatement(root.children, statements.front());

            for (const auto& [line, cost] : byFile[file])
            {
                // A line the file does not have (code inlined from a longer file) stays with the file
                if (line < 1 or line > static_cast<int>(print.lines.size()))
                {
                    root.selfNs += cost.ns;
                    root.runs = std::max(root.runs, cost.runs);
                }
                else
                    place(root, line, cost);
            }

            sumTotals(root);

            // The hottest statements by their own time, as the bytecode report colors its instructions
            std::vector<const SourceNode*> nodes;

            for (const auto& child : root.children)
                collect(child, nodes);

            std::sort(nodes.begin(), nodes.end(), [](const SourceNode* a, const SourceNode* b) { return a->selfNs > b->selfNs; });

            for (size_t i = 0; i < nodes.size() and i < 6 and nodes[i]->selfNs > 0; ++i)
                (i < 3 ? print.top3 : print.next3).insert(nodes[i]);

            std::cout << std::right
                      << std::setw(9) << "Total" << std::setw(11) << "ms"
                      << std::setw(9) << "Self" << std::setw(11) << "Runs"
                      << std::setw(7) << "Line" << " | Statement" << std::endl;
            std::cout << std::string(100, '-') << std::endl;

            printSourceRow(print, &root, "", file, 0, root.totalNs, root.selfNs, root.runs);
            printSourceChildren(print, root, 1);
        }

        std::cout << std::endl;
    }
    void VMProfiler::printReport(bool sortByTime) const
    {
        if (profiles.empty())
        {
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
            LOG_WARNING("VM_Profiling", "Profiling is not enabled");
            return;
        }

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
        uint64_t functionTotalTime = 0;
        for (const auto& [key, profile] : profiles)
        {
            if (profile.functionName == functionName)
            {
                functionProfiles.push_back(profile);
                functionTotalTime += profile.totalNanoseconds;
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
            double percentage = functionTotalTime > 0 ? (static_cast<double>(profile.totalNanoseconds) / functionTotalTime * 100.0) : 0.0;

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
        std::cout << "Function execution time: " << (functionTotalTime / 1'000.0) << " μs" << std::endl;
        std::cout << std::endl;
    }
}
