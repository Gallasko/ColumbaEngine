#pragma once

#include "chunk.h"
#include <string>
#include <memory>
#include <vector>

namespace pg {

    class BytecodePass {
    public:
        virtual ~BytecodePass() = default;
        
        virtual std::string getName() const = 0;
        
        virtual bool runPass(Chunk& chunk) = 0;
        
        virtual bool changesSize() const { return false; }
        
        virtual bool requiresMultiplePasses() const { return false; }
    };

    class PassManager {
    private:
        std::vector<std::unique_ptr<BytecodePass>> passes;
        bool enableDebugOutput = false;
        
    public:
        void addPass(std::unique_ptr<BytecodePass> pass);
        
        void runAllPasses(Chunk& chunk);
        
        bool runPass(const std::string& passName, Chunk& chunk);
        
        void setDebugOutput(bool enable) { enableDebugOutput = enable; }
        
        void listPasses() const;
        
        void clearPasses();
        
        size_t getPassCount() const { return passes.size(); }
        
    private:
        void debugPrint(const std::string& message) const;
    };

}