#include "gtest/gtest.h"
#include "Compiler/vm.h"
#include "Compiler/chunk.h"
#include "Compiler/value_nanbox.h"
#include <iostream>
#include <chrono>

namespace pg {
namespace test {

class RegisterOpsTest : public ::testing::Test {
protected:
    VM vm;

    void SetUp() override {
        // VM is initialized in constructor
        // Enable profiling
        vm.enableProfiling();
    }

    void TearDown() override {
        // Print profiling report
        vm.printProfilingReport();
    }
};

// // Test: int i = 0; while (i < 100000) { i = i + 1; }
// // Using register-based opcodes (no push/pop)
// TEST_F(RegisterOpsTest, SimpleLoopRegisterBased) {
//     Chunk chunk;

//     // Constants
//     chunk.constants.push_back(INT_VAL(0));        // constant[0] = 0
//     chunk.constants.push_back(INT_VAL(100000));   // constant[1] = 100000

//     // Slot allocation:
//     // slot[0] = i (loop counter)
//     // slot[1] = limit (10)
//     // Note: Stack operations (like OP_Less_RR pushing result) will use stack positions AFTER slots

//     // We need to advance the stack pointer past our local slots first
//     // Push dummy values to reserve slots 0 and 1
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);  // Push 0 (reserves slot[0])
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);  // Push 0 (reserves slot[1])

//     // Now initialize our actual values
//     // Initialize: i = 0
//     chunk.addCode(OpCode::OP_Load_Constant_R, 1);
//     chunk.addCode(0, 1);  // dest_slot = 0 (i)
//     chunk.addCode(0, 1);  // const_index = 0 (value 0)

//     // Load 10 into slot[1] (only once, outside loop)
//     chunk.addCode(OpCode::OP_Load_Constant_R, 1);
//     chunk.addCode(1, 1);  // dest_slot = 1
//     chunk.addCode(1, 1);  // const_index = 1 (value 10)

//     // Loop start
//     size_t loopStart = chunk.code.size();

//     // Compare: i < 1000
//     // OP_Less_RR pushes result to stack for jump
//     chunk.addCode(OpCode::OP_Less_RR, 1);
//     chunk.addCode(0, 1);  // src1_slot = 0 (i)
//     chunk.addCode(1, 1);  // src2_slot = 1 (1000)

//     // If false, jump to end
//     chunk.addCode(OpCode::OP_Jump_If_False, 1);
//     size_t jumpIfFalsePos = chunk.code.size();
//     chunk.addCode(0, 1);  // Placeholder for offset (high byte)
//     chunk.addCode(0, 1);  // Placeholder for offset (low byte)

//     // Pop the comparison result (true case)
//     chunk.addCode(OpCode::OP_Pop, 1);

//     // Increment: i++
//     chunk.addCode(OpCode::OP_Incr_R, 1);
//     chunk.addCode(0, 1);  // slot = 0 (i)

//     // Loop back
//     chunk.addCode(OpCode::OP_Loop, 1);
//     size_t loopOffset = chunk.code.size() + 2 - loopStart;
//     chunk.addCode((loopOffset >> 8) & 0xFF, 1);
//     chunk.addCode(loopOffset & 0xFF, 1);

//     // End of loop - patch jump
//     size_t endPos = chunk.code.size();
//     size_t jumpOffset = endPos - jumpIfFalsePos - 2;
//     chunk.code[jumpIfFalsePos] = (jumpOffset >> 8) & 0xFF;
//     chunk.code[jumpIfFalsePos + 1] = jumpOffset & 0xFF;

//     // Pop the comparison result (false case)
//     chunk.addCode(OpCode::OP_Pop, 1);

//     // Push a dummy return value (OP_Return expects something on stack)
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);

//     chunk.addCode(OpCode::OP_Return, 1);

//     // Execute
//     vm.setupTestChunk(chunk);
//     auto result = vm.run();

//     EXPECT_EQ(result, InterpretResult::OK);

//     std::cout << "Register-based loop executed successfully!" << std::endl;
// }

// // Test: Simple arithmetic using register ops
// TEST_F(RegisterOpsTest, SimpleArithmetic) {
//     Chunk chunk;

//     // Constants
//     chunk.constants.push_back(INT_VAL(5));      // constant[0] = 5
//     chunk.constants.push_back(INT_VAL(10));     // constant[1] = 10

//     // Load 5 into slot[0]
//     chunk.addCode(OpCode::OP_Load_Constant_R, 1);
//     chunk.addCode(0, 1);  // dest_slot = 0
//     chunk.addCode(0, 1);  // const_index = 0

//     // Load 10 into slot[1]
//     chunk.addCode(OpCode::OP_Load_Constant_R, 1);
//     chunk.addCode(1, 1);  // dest_slot = 1
//     chunk.addCode(1, 1);  // const_index = 1

//     // Add: slot[2] = slot[0] + slot[1]
//     chunk.addCode(OpCode::OP_Add_RRR, 1);
//     chunk.addCode(2, 1);  // dest_slot = 2
//     chunk.addCode(0, 1);  // src1_slot = 0
//     chunk.addCode(1, 1);  // src2_slot = 1

//     // Increment slot[2]
//     chunk.addCode(OpCode::OP_Incr_R, 1);
//     chunk.addCode(2, 1);  // slot = 2

//     // Push a dummy return value
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);

//     chunk.addCode(OpCode::OP_Return, 1);

//     // Execute
//     vm.setupTestChunk(chunk);
//     auto result = vm.run();

//     EXPECT_EQ(result, InterpretResult::OK);

//     std::cout << "Register-based arithmetic executed successfully!" << std::endl;
//     std::cout << "  5 + 10 + 1 = 16 (computed in registers)" << std::endl;
// }

// // Test: int i = 0; while (i < 100000) { i = i + 1; }
// // Using traditional STACK-BASED opcodes for comparison
// TEST_F(RegisterOpsTest, SimpleLoopStackBased) {
//     Chunk chunk;

//     // Constants
//     chunk.constants.push_back(INT_VAL(0));        // constant[0] = 0
//     chunk.constants.push_back(INT_VAL(1));        // constant[1] = 1
//     chunk.constants.push_back(INT_VAL(100000));   // constant[2] = 100000

//     // Initialize: var i = 0 (push onto stack to create local[0])
//     chunk.addCode(OpCode::OP_Constant, 1);
//     chunk.addCode(0, 1);  // Push 0 (becomes local[0])

//     // Loop start
//     size_t loopStart = chunk.code.size();

//     // Compare: i < 100000
//     chunk.addCode(OpCode::OP_Get_Local, 1);
//     chunk.addCode(0, 1);  // Push i
//     chunk.addCode(OpCode::OP_Constant, 1);
//     chunk.addCode(2, 1);  // Push 100000
//     chunk.addCode(OpCode::OP_Less, 1);  // Pop 2, push result

//     // If false, jump to end
//     chunk.addCode(OpCode::OP_Jump_If_False, 1);
//     size_t jumpIfFalsePos = chunk.code.size();
//     chunk.addCode(0, 1);  // Placeholder
//     chunk.addCode(0, 1);

//     chunk.addCode(OpCode::OP_Pop, 1);  // Pop comparison result

//     // i++  (use the built-in increment opcode, which expects slot index on stack)
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);  // Push slot index 0
//     chunk.addCode(OpCode::OP_Incr_Local, 1);  // Pops slot index, increments local[0], pushes result
//     chunk.addCode(OpCode::OP_Pop, 1);  // Pop the incremented value

//     // Loop back
//     chunk.addCode(OpCode::OP_Loop, 1);
//     size_t loopOffset = chunk.code.size() + 2 - loopStart;
//     chunk.addCode((loopOffset >> 8) & 0xFF, 1);
//     chunk.addCode(loopOffset & 0xFF, 1);

//     // End of loop - patch jump
//     size_t endPos = chunk.code.size();
//     size_t jumpOffset = endPos - jumpIfFalsePos - 2;
//     chunk.code[jumpIfFalsePos] = (jumpOffset >> 8) & 0xFF;
//     chunk.code[jumpIfFalsePos + 1] = jumpOffset & 0xFF;

//     chunk.addCode(OpCode::OP_Pop, 1);  // Pop comparison result

//     // Push dummy return value
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);

//     chunk.addCode(OpCode::OP_Return, 1);

//     // Execute
//     vm.setupTestChunk(chunk);
//     auto result = vm.run();

//     EXPECT_EQ(result, InterpretResult::OK);

//     std::cout << "Stack-based loop executed successfully!" << std::endl;
// }

// // Test: int i = 0; while (i < 100000) { i = i + 1; }
// // Using OPTIMIZED register-based opcodes (no push/pop at all!)
// TEST_F(RegisterOpsTest, SimpleLoopRegisterBasedOptimized) {
//     Chunk chunk;

//     // Constants
//     chunk.constants.push_back(INT_VAL(0));        // constant[0] = 0
//     chunk.constants.push_back(INT_VAL(100000));   // constant[1] = 100000

//     // Slot allocation:
//     // slot[0] = i (loop counter)
//     // slot[1] = limit (100000)
//     // slot[2] = comparison result (for jump)

//     // Reserve slots
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);  // Reserve slot[0]
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);  // Reserve slot[1]
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);  // Reserve slot[2]

//     // Initialize: i = 0
//     chunk.addCode(OpCode::OP_Load_Constant_R, 1);
//     chunk.addCode(0, 1);  // dest_slot = 0 (i)
//     chunk.addCode(0, 1);  // const_index = 0 (value 0)

//     // Load 100000 into slot[1]
//     chunk.addCode(OpCode::OP_Load_Constant_R, 1);
//     chunk.addCode(1, 1);  // dest_slot = 1
//     chunk.addCode(1, 1);  // const_index = 1 (value 100000)

//     // Loop start
//     size_t loopStart = chunk.code.size();

//     // Compare: slot[2] = slot[0] < slot[1] (NO PUSH!)
//     chunk.addCode(OpCode::OP_Less_RRR, 1);
//     chunk.addCode(2, 1);  // dest_slot = 2 (result)
//     chunk.addCode(0, 1);  // src1_slot = 0 (i)
//     chunk.addCode(1, 1);  // src2_slot = 1 (100000)

//     // Jump if slot[2] is false (NO POP!)
//     chunk.addCode(OpCode::OP_Jump_If_False_R, 1);
//     chunk.addCode(2, 1);  // Read from slot[2]
//     size_t jumpIfFalsePos = chunk.code.size();
//     chunk.addCode(0, 1);  // Placeholder for offset (high byte)
//     chunk.addCode(0, 1);  // Placeholder for offset (low byte)

//     // Increment: i++ (NO PUSH/POP!)
//     chunk.addCode(OpCode::OP_Incr_R, 1);
//     chunk.addCode(0, 1);  // slot = 0 (i)

//     // Loop back
//     chunk.addCode(OpCode::OP_Loop, 1);
//     size_t loopOffset = chunk.code.size() + 2 - loopStart;
//     chunk.addCode((loopOffset >> 8) & 0xFF, 1);
//     chunk.addCode(loopOffset & 0xFF, 1);

//     // End of loop - patch jump
//     size_t endPos = chunk.code.size();
//     size_t jumpOffset = endPos - jumpIfFalsePos - 2;
//     chunk.code[jumpIfFalsePos] = (jumpOffset >> 8) & 0xFF;
//     chunk.code[jumpIfFalsePos + 1] = jumpOffset & 0xFF;

//     // Push dummy return value
//     chunk.addCode(OpCode::OP_Short_Int, 1);
//     chunk.addCode(0, 1);

//     chunk.addCode(OpCode::OP_Return, 1);

//     // Execute
//     vm.setupTestChunk(chunk);
//     auto result = vm.run();

//     EXPECT_EQ(result, InterpretResult::OK);

//     std::cout << "OPTIMIZED Register-based loop executed successfully!" << std::endl;
// }

} // namespace test
} // namespace pg
