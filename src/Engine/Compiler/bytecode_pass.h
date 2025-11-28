#pragma once

#include "chunk.h"
#include <string>
#include <memory>
#include <vector>

#include "bytecode_rewriter.h"

namespace pg
{
    struct VM;

    class BytecodePass
    {
    public:
        virtual ~BytecodePass() = default;

        virtual std::string getName() const = 0;

        virtual bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) = 0;

        virtual bool changesSize() const { return false; }

        virtual bool requiresMultiplePasses() const { return false; }
    };

    class PassManager
    {
    private:
        std::vector<std::unique_ptr<BytecodePass>> passes;
        std::unique_ptr<BytecodeRewriter> rewriter;
        bool enableDebugOutput = false;

    public:
        PassManager();

        void addPass(std::unique_ptr<BytecodePass> pass);

        void runAllPasses(VM *vm, Chunk& chunk);

        bool runPass(const std::string& passName, Chunk& chunk);

        BytecodeRewriter* getRewriter() const { return rewriter.get(); }

        void setDebugOutput(bool enable) { enableDebugOutput = enable; }

        void listPasses() const;

        void clearPasses();

        size_t getPassCount() const { return passes.size(); }

    private:
        void debugPrint(const std::string& message) const;
    };

}