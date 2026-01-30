#pragma once

#include "../bytecode_pass.h"
#include "../bytecode_rewriter.h"
#include "../native_module.h"
#include <memory>
#include <vector>

namespace pg
{
    /**
     * ScriptedBytecodePass - Wraps a script function as a BytecodePass
     *
     * This allows users to write optimization passes in PgScript instead of C++.
     * The script function receives access to the chunk and rewriter via global context.
     */
    class ScriptedBytecodePass : public BytecodePass
    {
    private:
        std::string passName;
        Value scriptFunction;  // The script function to call
        VM* vm;
        bool changesSizeFlag;
        bool requiresMultiplePassesFlag;

    public:
        ScriptedBytecodePass(const std::string& name, Value func, VM* vmInstance,
                            bool changesSize = false, bool requiresMultiple = false)
            : passName(name), scriptFunction(func), vm(vmInstance)
            , changesSizeFlag(changesSize), requiresMultiplePassesFlag(requiresMultiple)
        {
        }

        std::string getName() const override { return passName; }

        bool runPass(Chunk& chunk, BytecodeRewriter* rewriter = nullptr) override;

        bool changesSize() const override { return changesSizeFlag; }

        bool requiresMultiplePasses() const override { return requiresMultiplePassesFlag; }
    };

    /**
     * BytecodePassModule - Native module for writing optimization passes in script
     *
     * Example usage:
     *
     *   import "bytecode_pass"
     *
     *   fun myOptimizationPass() {
     *       var pattern = [capture(OP_Get_Local), capture(OP_Get_Local), OP_Add]
     *
     *       addRule(pattern, fun(captured) {
     *           var local1 = captured[0].operands[0]
     *           var local2 = captured[1].operands[0]
     *           return [OP_AddLL, local1, local2]
     *       })
     *
     *       return applyRewrites()
     *   }
     *
     *   registerPass("OptimizeLocalAddition", myOptimizationPass, {
     *       changesSize: true,
     *       requiresMultiplePasses: false
     *   })
     */
    class BytecodePassModule : public NativeModule
    {
    public:
        BytecodePassModule();

    private:
        // Pass registration
        static Value nativeRegisterPass(VM* vm, int argCount, Value* args);

        // Pattern creation helpers
        static Value nativeCapture(VM* vm, int argCount, Value* args);
        static Value nativeWildcard(VM* vm, int argCount, Value* args);
        static Value nativeMatchConstant(VM* vm, int argCount, Value* args);
        static Value nativeMatchLoad(VM* vm, int argCount, Value* args);
        static Value nativeMatchStore(VM* vm, int argCount, Value* args);
        static Value nativeMatchJump(VM* vm, int argCount, Value* args);

        // Rule management
        static Value nativeAddRule(VM* vm, int argCount, Value* args);
        static Value nativeApplyRewrites(VM* vm, int argCount, Value* args);

        // Constant pool access
        static Value nativeGetConstant(VM* vm, int argCount, Value* args);
        static Value nativeAddConstant(VM* vm, int argCount, Value* args);

        // Helper type checks (might already exist in VM)
        static Value nativeIsNumber(VM* vm, int argCount, Value* args);
        static Value nativeIsString(VM* vm, int argCount, Value* args);
        static Value nativeIsBool(VM* vm, int argCount, Value* args);

        // OpCode constants (exported as variables)
        static void registerOpCodes(BytecodePassModule* module);

        // Helper to convert script pattern array to C++ pattern
        static std::vector<PatternElement> scriptPatternToNative(VM* vm, Value patternArray);

        // Helper to convert script bytecode array to C++ bytecode
        static std::vector<uint8_t> scriptBytecodeToNative(VM* vm, Value bytecodeValue);

        // Helper to create captured instruction table for script
        static Value createCapturedTable(VM* vm, const std::vector<CapturedInstruction>& captured);
    };

    // Global context for the currently executing pass
    struct PassExecutionContext
    {
        Chunk* chunk = nullptr;
        BytecodeRewriter* rewriter = nullptr;
        VM* vm = nullptr;

        void reset()
        {
            chunk = nullptr;
            rewriter = nullptr;
            vm = nullptr;
        }
    };

    // Global context - set during pass execution
    extern PassExecutionContext g_passContext;

    // Special marker values for pattern matching
    // These are encoded as special integers that the script can use
    namespace PatternMarkers
    {
        constexpr int64_t WILDCARD_BIT = 0x0100000000000000LL;      // Bit 56
        constexpr int64_t CAPTURE_BIT  = 0x0200000000000000LL;      // Bit 57
        constexpr int64_t MULTI_OP_BIT = 0x0400000000000000LL;      // Bit 58 - indicates multiple opcodes matched

        constexpr int64_t OPCODE_MASK  = 0x00000000000000FFLL;      // Bits 0-7 for opcode
        constexpr int64_t MARKER_MASK  = 0xFF00000000000000LL;      // Top byte for markers

        // Pre-defined multi-op patterns (stored in bits 0-55)
        constexpr int64_t MATCH_CONSTANT = MULTI_OP_BIT | 0x01;    // OP_Constant | OP_LongConstant
        constexpr int64_t MATCH_LOAD     = MULTI_OP_BIT | 0x02;    // OP_Get_Local | OP_Get_Global
        constexpr int64_t MATCH_STORE    = MULTI_OP_BIT | 0x03;    // OP_Set_Local | OP_Set_Global
        constexpr int64_t MATCH_JUMP     = MULTI_OP_BIT | 0x04;    // Jump opcodes
    }
}