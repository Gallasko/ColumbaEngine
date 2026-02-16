// #include "stdafx.h"
// #include "bytecode_pass_module.h"
// #include "logger.h"
// #include "../vm.h"

// namespace pg
// {
//     namespace
//     {
//         const char* DOM = "BytecodePassModule";
//     }

//     // Global pass execution context
//     PassExecutionContext g_passContext;

//     // ============================================================================
//     // ScriptedBytecodePass Implementation
//     // ============================================================================

//     bool ScriptedBytecodePass::runPass(Chunk& chunk, BytecodeRewriter* rewriter)
//     {
//         if (!rewriter)
//         {
//             LOG_ERROR(DOM, "No rewriter provided to ScriptedBytecodePass");
//             return false;
//         }

//         // Set up global context for this pass execution
//         g_passContext.chunk = &chunk;
//         g_passContext.rewriter = rewriter;
//         g_passContext.vm = vm;

//         try
//         {
//             // Call the script function
//             vm->pushValue(scriptFunction);

//             InterpretResult result = vm->callValue(scriptFunction, 0);

//             if (result != InterpretResult::OK)
//             {
//                 LOG_ERROR(DOM, "Script pass '" + passName + "' failed to execute");
//                 g_passContext.reset();
//                 return false;
//             }

//             // Pop the return value (should be a boolean indicating success)
//             Value returnValue = vm->popValue();
//             bool success = IS_BOOL(returnValue) ? AS_BOOL(returnValue) : true;

//             g_passContext.reset();
//             return success;
//         }
//         catch (const std::exception& e)
//         {
//             LOG_ERROR(DOM, "Exception in script pass '" + passName + "': " + e.what());
//             g_passContext.reset();
//             return false;
//         }
//     }

//     // ============================================================================
//     // BytecodePassModule Implementation
//     // ============================================================================

//     BytecodePassModule::BytecodePassModule()
//     {
//         // Pattern creation helpers
//         addNativeFunction("capture", nativeCapture);
//         addNativeFunction("wildcard", nativeWildcard);
//         addNativeFunction("matchConstant", nativeMatchConstant);
//         addNativeFunction("matchLoad", nativeMatchLoad);
//         addNativeFunction("matchStore", nativeMatchStore);
//         addNativeFunction("matchJump", nativeMatchJump);

//         // Rule management
//         addNativeFunction("addRule", nativeAddRule);
//         addNativeFunction("applyRewrites", nativeApplyRewrites);

//         // Constant pool access
//         addNativeFunction("getConstant", nativeGetConstant);
//         addNativeFunction("addConstant", nativeAddConstant);

//         // Type checking helpers
//         addNativeFunction("isNumber", nativeIsNumber);
//         addNativeFunction("isString", nativeIsString);
//         addNativeFunction("isBool", nativeIsBool);

//         // Pass registration
//         addNativeFunction("registerPass", nativeRegisterPass);

//         // Register OpCode constants
//         registerOpCodes(this);
//     }

//     void BytecodePassModule::registerOpCodes(BytecodePassModule* module)
//     {
//         // Register all OpCode values as constants
//         module->addNativeVariable("OP_Return", static_cast<int>(OpCode::OP_Return));
//         module->addNativeVariable("OP_Constant", static_cast<int>(OpCode::OP_Constant));
//         module->addNativeVariable("OP_LongConstant", static_cast<int>(OpCode::OP_LongConstant));
//         module->addNativeVariable("OP_Negate", static_cast<int>(OpCode::OP_Negate));
//         module->addNativeVariable("OP_Add", static_cast<int>(OpCode::OP_Add));
//         module->addNativeVariable("OP_Subtract", static_cast<int>(OpCode::OP_Subtract));
//         module->addNativeVariable("OP_Multiply", static_cast<int>(OpCode::OP_Multiply));
//         module->addNativeVariable("OP_Divide", static_cast<int>(OpCode::OP_Divide));
//         module->addNativeVariable("OP_True", static_cast<int>(OpCode::OP_True));
//         module->addNativeVariable("OP_False", static_cast<int>(OpCode::OP_False));
//         module->addNativeVariable("OP_Not", static_cast<int>(OpCode::OP_Not));
//         module->addNativeVariable("OP_And", static_cast<int>(OpCode::OP_And));
//         module->addNativeVariable("OP_Or", static_cast<int>(OpCode::OP_Or));
//         module->addNativeVariable("OP_Equal", static_cast<int>(OpCode::OP_Equal));
//         module->addNativeVariable("OP_NotEqual", static_cast<int>(OpCode::OP_NotEqual));
//         module->addNativeVariable("OP_Greater", static_cast<int>(OpCode::OP_Greater));
//         module->addNativeVariable("OP_GreaterEqual", static_cast<int>(OpCode::OP_GreaterEqual));
//         module->addNativeVariable("OP_Less", static_cast<int>(OpCode::OP_Less));
//         module->addNativeVariable("OP_LessEqual", static_cast<int>(OpCode::OP_LessEqual));
//         module->addNativeVariable("OP_Pop", static_cast<int>(OpCode::OP_Pop));
//         module->addNativeVariable("OP_PopN", static_cast<int>(OpCode::OP_PopN));
//         module->addNativeVariable("OP_Define_Global", static_cast<int>(OpCode::OP_Define_Global));
//         module->addNativeVariable("OP_Get_Global", static_cast<int>(OpCode::OP_Get_Global));
//         module->addNativeVariable("OP_Set_Global", static_cast<int>(OpCode::OP_Set_Global));
//         module->addNativeVariable("OP_Get_Local", static_cast<int>(OpCode::OP_Get_Local));
//         module->addNativeVariable("OP_Set_Local", static_cast<int>(OpCode::OP_Set_Local));
//         module->addNativeVariable("OP_Jump_If_False", static_cast<int>(OpCode::OP_Jump_If_False));
//         module->addNativeVariable("OP_Jump", static_cast<int>(OpCode::OP_Jump));
//         module->addNativeVariable("OP_Loop", static_cast<int>(OpCode::OP_Loop));
//         module->addNativeVariable("OP_Call", static_cast<int>(OpCode::OP_Call));
//         module->addNativeVariable("OP_AddLL", static_cast<int>(OpCode::OP_AddLL));
//         module->addNativeVariable("OP_SubtractLL", static_cast<int>(OpCode::OP_SubtractLL));
//         // Add more opcodes as needed...
//     }

//     // ============================================================================
//     // Pattern Creation Helpers
//     // ============================================================================

//     Value BytecodePassModule::nativeCapture(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 1)
//         {
//             throw std::runtime_error("capture expects 1 argument: pattern element");
//         }

//         if (!IS_INT(args[0]))
//         {
//             throw std::runtime_error("capture: argument must be an integer");
//         }

//         int64_t pattern = AS_INT(args[0]);
//         // Set the capture bit
//         pattern |= PatternMarkers::CAPTURE_BIT;

//         return INT_VAL(pattern);
//     }

//     Value BytecodePassModule::nativeWildcard(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 0)
//         {
//             throw std::runtime_error("wildcard expects 0 arguments");
//         }

//         return INT_VAL(PatternMarkers::WILDCARD_BIT);
//     }

//     Value BytecodePassModule::nativeMatchConstant(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 0)
//         {
//             throw std::runtime_error("matchConstant expects 0 arguments");
//         }

//         return INT_VAL(PatternMarkers::MATCH_CONSTANT);
//     }

//     Value BytecodePassModule::nativeMatchLoad(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 0)
//         {
//             throw std::runtime_error("matchLoad expects 0 arguments");
//         }

//         return INT_VAL(PatternMarkers::MATCH_LOAD);
//     }

//     Value BytecodePassModule::nativeMatchStore(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 0)
//         {
//             throw std::runtime_error("matchStore expects 0 arguments");
//         }

//         return INT_VAL(PatternMarkers::MATCH_STORE);
//     }

//     Value BytecodePassModule::nativeMatchJump(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 0)
//         {
//             throw std::runtime_error("matchJump expects 0 arguments");
//         }

//         return INT_VAL(PatternMarkers::MATCH_JUMP);
//     }

//     // ============================================================================
//     // Rule Management
//     // ============================================================================

//     Value BytecodePassModule::nativeAddRule(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 2)
//         {
//             throw std::runtime_error("addRule expects 2 arguments: pattern (array), transformFunc (function)");
//         }

//         if (!IS_TABLE(args[0]))
//         {
//             throw std::runtime_error("addRule: first argument must be an array/table");
//         }

//         if (!IS_CLOSURE(args[1]))
//         {
//             throw std::runtime_error("addRule: second argument must be a function");
//         }

//         if (!g_passContext.rewriter)
//         {
//             throw std::runtime_error("addRule: no rewriter in context (must be called from within a pass)");
//         }

//         // Convert script pattern to native pattern
//         std::vector<PatternElement> nativePattern = scriptPatternToNative(vm, args[0]);

//         // Capture the closure value for use in the lambda
//         Value transformClosure = args[1];
//         VM* vmPtr = vm;

//         // Add the rule to the rewriter
//         g_passContext.rewriter->addAdvancedRule(
//             nativePattern,
//             [vmPtr, transformClosure](const std::vector<CapturedInstruction>& captured) -> std::vector<uint8_t>
//             {
//                 // Create a table representing the captured instructions
//                 Value capturedTable = BytecodePassModule::createCapturedTable(vmPtr, captured);

//                 // Call the transform function with the captured data
//                 vmPtr->pushValue(transformClosure);
//                 vmPtr->pushValue(capturedTable);

//                 InterpretResult result = vmPtr->callValue(transformClosure, 1);

//                 if (result != InterpretResult::OK)
//                 {
//                     throw std::runtime_error("Transform function failed");
//                 }

//                 // Get the return value (should be an array of bytes or 0 for no replacement)
//                 Value returnValue = vmPtr->popValue();

//                 // Convert the return value to bytecode
//                 return scriptBytecodeToNative(vmPtr, returnValue);
//             }
//         );

//         return BOOL_VAL(true);
//     }

//     Value BytecodePassModule::nativeApplyRewrites(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 0)
//         {
//             throw std::runtime_error("applyRewrites expects 0 arguments");
//         }

//         if (!g_passContext.rewriter || !g_passContext.chunk)
//         {
//             throw std::runtime_error("applyRewrites: no rewriter or chunk in context");
//         }

//         bool result = g_passContext.rewriter->rewrite(*g_passContext.chunk);

//         return BOOL_VAL(result);
//     }

//     // ============================================================================
//     // Constant Pool Access
//     // ============================================================================

//     Value BytecodePassModule::nativeGetConstant(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 1)
//         {
//             throw std::runtime_error("getConstant expects 1 argument: index");
//         }

//         if (!IS_INT(args[0]))
//         {
//             throw std::runtime_error("getConstant: argument must be an integer");
//         }

//         if (!g_passContext.chunk)
//         {
//             throw std::runtime_error("getConstant: no chunk in context");
//         }

//         int index = AS_INT(args[0]);

//         if (index < 0 || index >= static_cast<int>(g_passContext.chunk->constants.size()))
//         {
//             throw std::runtime_error("getConstant: index out of bounds");
//         }

//         return g_passContext.chunk->constants[index];
//     }

//     Value BytecodePassModule::nativeAddConstant(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 1)
//         {
//             throw std::runtime_error("addConstant expects 1 argument: value");
//         }

//         if (!g_passContext.chunk)
//         {
//             throw std::runtime_error("addConstant: no chunk in context");
//         }

//         // Add the constant without emitting bytecode (returns just the index)
//         g_passContext.chunk->constants.push_back(args[0]);
//         int index = static_cast<int>(g_passContext.chunk->constants.size() - 1);

//         return INT_VAL(index);
//     }

//     // ============================================================================
//     // Type Checking Helpers
//     // ============================================================================

//     Value BytecodePassModule::nativeIsNumber(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 1)
//         {
//             throw std::runtime_error("isNumber expects 1 argument");
//         }

//         bool isNum = IS_INT(args[0]) || IS_DOUBLE(args[0]);
//         return BOOL_VAL(isNum);
//     }

//     Value BytecodePassModule::nativeIsString(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 1)
//         {
//             throw std::runtime_error("isString expects 1 argument");
//         }

//         return BOOL_VAL(IS_STRING(args[0]));
//     }

//     Value BytecodePassModule::nativeIsBool(VM* vm, int argCount, Value* args)
//     {
//         if (argCount != 1)
//         {
//             throw std::runtime_error("isBool expects 1 argument");
//         }

//         return BOOL_VAL(IS_BOOL(args[0]));
//     }

//     // ============================================================================
//     // Pass Registration
//     // ============================================================================

//     Value BytecodePassModule::nativeRegisterPass(VM* vm, int argCount, Value* args)
//     {
//         if (argCount < 2 || argCount > 3)
//         {
//             throw std::runtime_error("registerPass expects 2-3 arguments: name (string), passFunc (function), [options (table)]");
//         }

//         if (!IS_STRING(args[0]))
//         {
//             throw std::runtime_error("registerPass: first argument must be a string (pass name)");
//         }

//         if (!IS_CLOSURE(args[1]))
//         {
//             throw std::runtime_error("registerPass: second argument must be a function");
//         }

//         // Get the pass name
//         uint32_t stringIndex = AS_STRING_INDEX(args[0]);
//         auto stringElement = vm->getPools().stringPool.getElement(stringIndex);
//         std::string passName = stringElement ? stringElement->toString() : "UnknownPass";

//         // Parse options if provided
//         bool changesSize = false;
//         bool requiresMultiplePasses = false;

//         if (argCount == 3 && IS_TABLE(args[2]))
//         {
//             // TODO: Extract changesSize and requiresMultiplePasses from options table
//             // For now, use defaults
//         }

//         // Create a ScriptedBytecodePass
//         auto scriptedPass = std::make_unique<ScriptedBytecodePass>(
//             passName, args[1], vm, changesSize, requiresMultiplePasses
//         );

//         // TODO: Add method to VM/PassManager to register this pass
//         // For now, just log that it was registered
//         LOG_INFO(DOM, "Registered scripted pass: " + passName);

//         return BOOL_VAL(true);
//     }

//     // ============================================================================
//     // Helper Functions
//     // ============================================================================

//     std::vector<PatternElement> BytecodePassModule::scriptPatternToNative(VM* vm, Value patternArray)
//     {
//         std::vector<PatternElement> result;

//         if (!IS_TABLE(patternArray))
//         {
//             throw std::runtime_error("Pattern must be an array/table");
//         }

//         // Get the table
//         uint32_t tableIndex = AS_TABLE_INDEX(patternArray);
//         auto tablePtr = vm->getPools().tablePool.getElement(tableIndex);

//         if (!tablePtr)
//         {
//             throw std::runtime_error("Invalid table");
//         }

//         // Iterate through the table (assuming it's an array with numeric keys)
//         for (size_t i = 0; i < tablePtr->size(); i++)
//         {
//             Value key = INT_VAL(i);
//             Value element = tablePtr->get(key);

//             if (!IS_INT(element))
//             {
//                 continue; // Skip non-integer elements
//             }

//             int64_t patternValue = AS_INT(element);
//             bool capture = (patternValue & PatternMarkers::CAPTURE_BIT) != 0;

//             if (patternValue & PatternMarkers::WILDCARD_BIT)
//             {
//                 // Wildcard pattern
//                 result.push_back(PatternElement::wildcard(capture));
//             }
//             else if (patternValue & PatternMarkers::MULTI_OP_BIT)
//             {
//                 // Multi-opcode pattern
//                 int64_t patternType = patternValue & ~PatternMarkers::MARKER_MASK;

//                 if (patternType == (PatternMarkers::MATCH_CONSTANT & ~PatternMarkers::MARKER_MASK))
//                 {
//                     result.push_back(PatternElement::constant(capture));
//                 }
//                 else if (patternType == (PatternMarkers::MATCH_LOAD & ~PatternMarkers::MARKER_MASK))
//                 {
//                     result.push_back(PatternElement::load(capture));
//                 }
//                 else if (patternType == (PatternMarkers::MATCH_STORE & ~PatternMarkers::MARKER_MASK))
//                 {
//                     result.push_back(PatternElement::store(capture));
//                 }
//                 else if (patternType == (PatternMarkers::MATCH_JUMP & ~PatternMarkers::MARKER_MASK))
//                 {
//                     result.push_back(PatternElement::jump(capture));
//                 }
//             }
//             else
//             {
//                 // Single opcode pattern
//                 OpCode opcode = static_cast<OpCode>(patternValue & PatternMarkers::OPCODE_MASK);
//                 result.push_back(PatternElement::match(opcode, capture));
//             }
//         }

//         return result;
//     }

//     std::vector<uint8_t> BytecodePassModule::scriptBytecodeToNative(VM* vm, Value bytecodeValue)
//     {
//         std::vector<uint8_t> result;

//         // If the value is 0 (our null replacement), return empty vector to signal "don't replace"
//         if (IS_INT(bytecodeValue) && AS_INT(bytecodeValue) == 0)
//         {
//             // Note: The rewriter might need a special signal for "don't replace"
//             // For now, we'll use an empty vector which means "remove the pattern"
//             // TODO: Discuss with user how to handle "keep original" case
//             return result;
//         }

//         if (!IS_TABLE(bytecodeValue))
//         {
//             throw std::runtime_error("Transform function must return an array of bytes or 0");
//         }

//         // Get the table
//         uint32_t tableIndex = AS_TABLE_INDEX(bytecodeValue);
//         auto tablePtr = vm->getPools().tablePool.getElement(tableIndex);

//         if (!tablePtr)
//         {
//             throw std::runtime_error("Invalid table");
//         }

//         // Iterate through the table and extract bytes
//         for (size_t i = 0; i < tablePtr->size(); i++)
//         {
//             Value key = INT_VAL(i);
//             Value element = tablePtr->get(key);

//             if (IS_INT(element))
//             {
//                 int value = AS_INT(element);
//                 result.push_back(static_cast<uint8_t>(value & 0xFF));
//             }
//         }

//         return result;
//     }

//     Value BytecodePassModule::createCapturedTable(VM* vm, const std::vector<CapturedInstruction>& captured)
//     {
//         // Create an array of instruction objects
//         // Each object has: { opcode: int, operands: [bytes...] }

//         // For simplicity, we'll create a table where each index maps to an instruction table
//         auto mainTable = vm->getPools().tablePool.allocate();

//         for (size_t i = 0; i < captured.size(); i++)
//         {
//             const auto& instr = captured[i];

//             // Create a table for this instruction
//             auto instrTable = vm->getPools().tablePool.allocate();

//             // Set opcode field
//             Value opcodeKey = vm->createString("opcode");
//             instrTable->set(opcodeKey, INT_VAL(static_cast<int>(instr.opcode)));

//             // Create operands array
//             auto operandsTable = vm->getPools().tablePool.allocate();
//             for (size_t j = 0; j < instr.operands.size(); j++)
//             {
//                 operandsTable->set(INT_VAL(j), INT_VAL(instr.operands[j]));
//             }

//             Value operandsKey = vm->createString("operands");
//             instrTable->set(operandsKey, MAKE_TABLE(vm->getPools().tablePool.getElementIndex(operandsTable)));

//             // Add instruction to main array
//             mainTable->set(INT_VAL(i), MAKE_TABLE(vm->getPools().tablePool.getElementIndex(instrTable)));
//         }

//         return MAKE_TABLE(vm->getPools().tablePool.getElementIndex(mainTable));
//     }
// }
