#include "bytecode_pass.h"
#include "compiler_debug.h"
#include "logger.h"
#include <algorithm>
#include <iostream>

namespace pg {

    PassManager::PassManager() {
        rewriter = std::make_unique<BytecodeRewriter>();
        LOG_INFO("PassManager", "Initialized with shared BytecodeRewriter");
    }

    void PassManager::addPass(std::unique_ptr<BytecodePass> pass) {
        if (pass) {
            if (enableDebugOutput) {
                LOG_INFO("PassManager", "Added pass: " << pass->getName());
            }
            passes.push_back(std::move(pass));
        }
    }

    void PassManager::runAllPasses(Chunk& chunk) {
        if (passes.empty()) {
            if (enableDebugOutput) {
                LOG_INFO("PassManager", "No passes to run");
            }
            return;
        }

        LOG_INFO("PassManager", "Running " << passes.size() << " passes");

        for (auto& pass : passes) {
            if (enableDebugOutput) {
                LOG_INFO("PassManager", "Running pass: " << pass->getName());
            }

            bool changed = false;
            int iterations = 0;
            const int maxIterations = 10; // Prevent infinite loops

            do {
                // Clear rewriter rules before each pass execution
                rewriter->clearRules();

                changed = pass->runPass(chunk, rewriter.get());
                iterations++;

                if (changed && enableDebugOutput) {
                    LOG_INFO("PassManager", "Pass " << pass->getName() << " made changes (iteration " << iterations << ")");
                }

                if (iterations >= maxIterations) {
                    LOG_WARNING("PassManager", "Pass " << pass->getName() << " reached maximum iterations (" << maxIterations << ")");
                    break;
                }

            } while (changed && pass->requiresMultiplePasses());

            if (enableDebugOutput) {
                LOG_INFO("PassManager", "Pass " << pass->getName() << " completed in " << iterations << " iteration(s)");

                // Print bytecode after this pass
                std::cout << "\n=== BYTECODE AFTER " << pass->getName() << " ===" << std::endl;
                disassembleChunk(chunk, "After " + pass->getName());
                std::cout << std::endl;
            }
        }

        LOG_INFO("PassManager", "All passes completed");
    }

    bool PassManager::runPass(const std::string& passName, Chunk& chunk) {
        auto it = std::find_if(passes.begin(), passes.end(),
            [&passName](const std::unique_ptr<BytecodePass>& pass) {
                return pass->getName() == passName;
            });

        if (it != passes.end()) {
            LOG_INFO("PassManager", "Running specific pass: " << passName);

            // Clear rewriter rules before pass execution
            rewriter->clearRules();

            bool changed = (*it)->runPass(chunk, rewriter.get());

            if (changed) {
                LOG_INFO("PassManager", "Pass " << passName << " made changes");
            } else if (enableDebugOutput) {
                LOG_INFO("PassManager", "Pass " << passName << " made no changes");
            }

            return changed;
        }

        LOG_ERROR("PassManager", "Pass not found: " << passName);
        return false;
    }

    void PassManager::listPasses() const {
        if (passes.empty()) {
            LOG_INFO("PassManager", "No passes registered");
            return;
        }

        LOG_INFO("PassManager", "Registered passes (" << passes.size() << "):");
        for (size_t i = 0; i < passes.size(); ++i) {
            std::string passInfo = std::to_string(i + 1) + ". " + passes[i]->getName();

            if (passes[i]->changesSize()) {
                passInfo += " [size-changing]";
            }

            if (passes[i]->requiresMultiplePasses()) {
                passInfo += " [multi-pass]";
            }

            LOG_INFO("PassManager", passInfo);
        }
    }

    void PassManager::clearPasses() {
        if (enableDebugOutput) {
            LOG_INFO("PassManager", "Clearing all passes");
        }
        passes.clear();
    }

}