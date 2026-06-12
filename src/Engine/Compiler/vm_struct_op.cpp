#include "stdafx.h"

#include "vm.h"
#include "decoded_chunk.h"

namespace pg
{
    bool invoke(VM* vm, const std::string& name, uint8_t argCount)
    {
        auto receiverValue = vm->peek(argCount);

        if (not IS_INSTANCE(receiverValue))
        {
            return false;
        }

        ObjInstance* instance = vm->asInstance(receiverValue);

        // Check for field
        auto fieldIt = instance->internedFields.find(name);
        if (fieldIt != instance->internedFields.end())
        {
            Value fieldValue = instance->fieldValues[fieldIt->second];
            vm->releaseAndDelete(vm->stack[vm->stack.size() - argCount - 1]); // Remove receiver
            vm->stack[vm->stack.size() - argCount - 1] = vm->retainValue(fieldValue); // Replace it with the field value
            return vm->callValue(fieldValue, argCount);
        }

        // Then check for method
        return vm->callMethod(instance->klass, name, argCount);
    }

    const DecodedInstruction* op_invoke_decoded(VM* vm, const DecodedInstruction& instr)
    {
        const uint8_t argCount = instr.operands.indexed.byte2;
        const std::string& methodName = *instr.propertyNamePtr;

        const int frameCountBefore = vm->frameCount;

        // Resume point for op_return_decoded if invoke pushes a frame.
        vm->pendingCallResume = &instr + 1;

        if (not invoke(vm, methodName, argCount))
        {
            vm->runtimeError("Method '" + methodName + "' not found.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        return vm->completeDecodedFrameSwitch(frameCountBefore, &instr + 1);
    }


    bool bindMethod(VM* vm, Klass* klass, const std::string& name)
    {
        auto methodIt = klass->methods.find(name);
        if (methodIt == klass->methods.end())
        {
            return false;
        }

        auto methodValue = methodIt->second;

        // Handle closures (script-defined methods) - create bound method
        if (IS_CLOSURE(methodValue))
        {
            auto bound = vm->createBoundMethod(vm->peek(0), vm->asClosure(methodValue));  // Already tracked

            auto instance = vm->pop(); // Remove the instance
            vm->releaseAndDelete(instance);
            vm->push(bound);

            return true;
        }

        // Handle native functions (C++-defined methods) - return native function directly
        // Native functions will receive the receiver as the first argument when called
        if (IS_NAT_FUNC(methodValue))
        {
            auto instance = vm->pop(); // Remove the instance
            vm->releaseAndDelete(instance);
            vm->push(vm->retainValue(methodValue));  // Push the native function directly

            return true;
        }

        return false;
    }

    const DecodedInstruction* op_get_property_decoded(VM* vm, const DecodedInstruction& instr)
    {
        if (not IS_INSTANCE(vm->peek(0)))
        {
            vm->runtimeError("Only instances have properties.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto* instance = vm->asInstance(vm->peek(0));

        // Property name pre-resolved at decode time — no chunk.constantStrings
        // lookup, no *ip++.
        const std::string& nameStr = *instr.propertyNamePtr;

        // Field-found branch (no frame change).
        auto fieldIt = instance->internedFields.find(nameStr);
        if (fieldIt != instance->internedFields.end())
        {
            auto inst = vm->pop();
            vm->releaseAndDelete(inst);
            vm->push(vm->retainValue(instance->fieldValues[fieldIt->second]));
            return &instr + 1;
        }

        // Look up __get metamethod (class method or dynamic instance field).
        Value getMethod;
        bool hasGetMethod = false;
        bool isDynamicGet = false;

        auto getMetaIt = instance->klass->methods.find("__get");
        if (getMetaIt != instance->klass->methods.end())
        {
            getMethod = getMetaIt->second;
            hasGetMethod = true;
        }
        else
        {
            auto dynIt = instance->internedFields.find("__get");
            if (dynIt != instance->internedFields.end())
            {
                getMethod = instance->fieldValues[dynIt->second];
                hasGetMethod = true;
                isDynamicGet = true;
            }
        }

        if (hasGetMethod and (IS_CLOSURE(getMethod) or IS_NAT_FUNC(getMethod)))
        {
            Value nameValue = vm->createString(nameStr);

            if (IS_CLOSURE(getMethod))
            {
                vm->push(nameValue);

                const int frameCountBefore = vm->frameCount;

                // Resume point captured into the metamethod's frame.
                vm->pendingCallResume = &instr + 1;

                if (isDynamicGet)
                {
                    // Stack rewrite: [...] [instance] [propName] →
                    //                [...] [getMethod] [instance] [propName]
                    Value propName = vm->pop();
                    Value inst     = vm->pop();
                    vm->push(vm->retainValue(getMethod));
                    vm->push(inst);
                    vm->push(propName);

                    if (not vm->call(vm->asClosure(getMethod), 2))
                    {
                        vm->releaseAndDelete(getMethod);
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return nullptr;
                    }
                }
                else
                {
                    if (not vm->callBound(vm->asClosure(getMethod), 1))
                    {
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return nullptr;
                    }
                }

                // Frame was pushed — continue in the metamethod.
                return vm->completeDecodedFrameSwitch(frameCountBefore, &instr + 1);
            }
            else // IS_NAT_FUNC(getMethod)
            {
                auto* native = vm->asNativeFunc(getMethod);

                vm->push(nameValue);
                Value result = native->function(vm, 2, vm->stack.data() + vm->stack.size() - 2);

                auto propName = vm->pop();
                auto inst     = vm->pop();
                vm->releaseAndDelete(propName);
                vm->releaseAndDelete(inst);

                vm->push(result);
                return &instr + 1;
            }
        }

        // Fallback: bind a regular method by name.
        if (not bindMethod(vm, instance->klass, nameStr))
        {
            vm->runtimeError("Undefined property '" + nameStr + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }
        return &instr + 1;
    }

    const DecodedInstruction* op_set_property_decoded(VM* vm, const DecodedInstruction& instr)
    {
        if (not IS_INSTANCE(vm->peek(1)))
        {
            vm->runtimeError("Only instances have fields.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto* instance = vm->asInstance(vm->peek(1));

        // Property name pre-resolved at decode time — no chunk.constantStrings
        // lookup, no *ip++.
        const std::string& nameStr = *instr.propertyNamePtr;

        // Fields starting with '__' bypass metamethods to allow metamethod
        // implementations to write through to internal storage without
        // re-triggering themselves.
        bool isInternalField  = (nameStr.length() >= 2 and nameStr[0] == '_' and nameStr[1] == '_');
        bool isInternalAccess = isInternalField;

        if (not isInternalAccess and vm->frameCount > 1)
        {
            // Bound-method-on-self check: in callBound, stackBase == slots.
            const bool isBoundMethodContext = (vm->currentFrame->stackBase == vm->currentFrame->slots);
            if (isBoundMethodContext)
            {
                Value currentReceiver  = vm->currentFrame->slots[0];
                Value accessedInstance = vm->peek(1);
                if (IS_INSTANCE(currentReceiver) and currentReceiver == accessedInstance)
                {
                    isInternalAccess = true;
                }
            }
        }

        // Look up __set metamethod (class method or dynamic instance field).
        Value setMethod;
        bool hasSetMethod = false;
        bool isDynamicSet = false;

        if (not isInternalAccess)
        {
            auto setMetaIt = instance->klass->methods.find("__set");
            if (setMetaIt != instance->klass->methods.end())
            {
                setMethod = setMetaIt->second;
                hasSetMethod = true;
            }
            else
            {
                auto dynIt = instance->internedFields.find("__set");
                if (dynIt != instance->internedFields.end())
                {
                    setMethod = instance->fieldValues[dynIt->second];
                    hasSetMethod = true;
                    isDynamicSet = true;
                }
            }
        }

        if (hasSetMethod)
        {
            Value nameValue = vm->createString(nameStr);

            if (IS_CLOSURE(setMethod))
            {
                // Stack: [instance, value] → [instance, name, value]
                Value value = vm->pop();
                vm->push(nameValue);
                vm->push(value);

                const int frameCountBefore = vm->frameCount;

                // Resume point captured into the metamethod's frame.
                vm->pendingCallResume = &instr + 1;

                if (isDynamicSet)
                {
                    // Rewrite to: [...] [setMethod] [instance] [name] [value]
                    Value val      = vm->pop();
                    Value propName = vm->pop();
                    Value inst     = vm->pop();

                    vm->push(vm->retainValue(setMethod));
                    vm->push(inst);
                    vm->push(propName);
                    vm->push(val);

                    if (not vm->call(vm->asClosure(setMethod), 3))
                    {
                        vm->releaseAndDelete(setMethod);
                        vm->runtimeError("Cannot call __set metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return nullptr;
                    }
                }
                else
                {
                    if (not vm->callBound(vm->asClosure(setMethod), 2))
                    {
                        vm->runtimeError("Cannot call __set metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return nullptr;
                    }
                }

                return vm->completeDecodedFrameSwitch(frameCountBefore, &instr + 1);
            }
            else if (IS_NAT_FUNC(setMethod))
            {
                auto* native = vm->asNativeFunc(setMethod);

                Value value = vm->pop();
                vm->push(nameValue);
                vm->push(value);

                Value result = native->function(vm, 3, vm->stack.data() + vm->stack.size() - 3);

                vm->pop(); // value
                vm->pop(); // name
                auto inst = vm->pop();
                vm->releaseAndDelete(inst);

                vm->push(result);
                return &instr + 1;
            }
        }

        // Plain field assignment (no metamethod).
        auto value = vm->pop();
        auto inst  = vm->pop();
        vm->releaseAndDelete(inst);

        auto fieldIt = instance->internedFields.find(nameStr);
        if (fieldIt != instance->internedFields.end())
        {
            size_t valueIndex = fieldIt->second;
            vm->releaseAndDelete(instance->fieldValues[valueIndex]);
            instance->fieldValues[valueIndex] = vm->retainValue(value);
        }
        else
        {
            size_t newIndex = instance->fieldValues.size();
            instance->fieldValues.push_back(vm->retainValue(value));
            instance->internedFields[nameStr] = newIndex;
        }

        vm->push(value);
        return &instr + 1;
    }

    // ========================================================================
    // Table Operations
    // ========================================================================

    const DecodedInstruction* op_get_index_decoded(VM* vm, const DecodedInstruction& instr)
    {
        Value index  = vm->pop();
        Value target = vm->pop();

        // Vector indexing.
        if (IS_VECTOR(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(target);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            ObjVector* vec = vm->asVector(target);
            int idx = AS_INT(index);
            if (idx < 0) idx = static_cast<int>(vec->fields.size()) + idx;

            if (idx < 0 || idx >= static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(target);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            vm->releaseAndDelete(target);
            vm->push(vm->retainValue(vec->fields[idx]));
            return &instr + 1;
        }

        // String indexing.
        if (IS_STRING(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
                vm->runtimeError("String index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            std::string str = vm->asString(target);
            int idx = AS_INT(index);
            if (idx < 0) idx = static_cast<int>(str.length()) + idx;

            if (idx < 0 || idx >= static_cast<int>(str.length()))
            {
                if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
                vm->runtimeError("String index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
            unsigned char ch = static_cast<unsigned char>(str[idx]);
            vm->push(vm->retainValue(vm->singleCharCache[ch]));
            return &instr + 1;
        }

        // Table / instance indexing.
        if (!IS_INSTANCE(target))
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->runtimeError("Can only index vectors, strings, or tables/instances");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        ObjInstance* inst = vm->asInstance(target);

        std::string key;
        if (IS_INT(index))
        {
            key = std::to_string(AS_INT(index));
        }
        else if (IS_STRING(index))
        {
            key = vm->asString(index);
        }
        else
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->runtimeError("Index must be integer or string");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        auto fieldIt = inst->internedFields.find(key);
        if (fieldIt != inst->internedFields.end())
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->push(vm->retainValue(inst->fieldValues[fieldIt->second]));
            return &instr + 1;
        }

        // Look up __get metamethod.
        Value getMethod;
        bool hasGetMethod = false;
        bool isDynamicGet = false;

        if (inst->klass)
        {
            auto getMetaIt = inst->klass->methods.find("__get");
            if (getMetaIt != inst->klass->methods.end())
            {
                getMethod = getMetaIt->second;
                hasGetMethod = true;
            }
            else
            {
                auto dynIt = inst->internedFields.find("__get");
                if (dynIt != inst->internedFields.end())
                {
                    getMethod = inst->fieldValues[dynIt->second];
                    hasGetMethod = true;
                    isDynamicGet = true;
                }
            }
        }

        if (hasGetMethod and (IS_CLOSURE(getMethod) or IS_NAT_FUNC(getMethod)))
        {
            Value keyValue = vm->createString(key);

            if (IS_CLOSURE(getMethod))
            {
                vm->push(target);
                vm->push(keyValue);

                const int frameCountBefore = vm->frameCount;

                // Resume point captured into the metamethod's frame.
                vm->pendingCallResume = &instr + 1;

                if (isDynamicGet)
                {
                    // Rewrite to [...] [getMethod] [instance] [key].
                    Value k    = vm->pop();
                    Value inst = vm->pop();
                    vm->push(vm->retainValue(getMethod));
                    vm->push(inst);
                    vm->push(k);

                    if (not vm->call(vm->asClosure(getMethod), 2))
                    {
                        vm->releaseAndDelete(getMethod);
                        vm->releaseAndDelete(keyValue);
                        vm->releaseAndDelete(index);
                        vm->releaseAndDelete(target);
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return nullptr;
                    }
                }
                else
                {
                    if (not vm->callBound(vm->asClosure(getMethod), 1))
                    {
                        vm->releaseAndDelete(keyValue);
                        vm->releaseAndDelete(index);
                        vm->releaseAndDelete(target);
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return nullptr;
                    }
                }

                vm->releaseAndDelete(keyValue);
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(target);

                return vm->completeDecodedFrameSwitch(frameCountBefore, &instr + 1);
            }
            else // IS_NAT_FUNC(getMethod)
            {
                auto* native = vm->asNativeFunc(getMethod);

                vm->push(target);
                vm->push(keyValue);

                Value result = native->function(vm, 2, vm->stack.data() + vm->stack.size() - 2);

                vm->pop(); // keyValue
                vm->pop(); // target
                vm->releaseAndDelete(keyValue);
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(target);

                vm->push(result);
                return &instr + 1;
            }
        }

        // No metamethod, no field — soft-fail by pushing false.
        vm->releaseAndDelete(index);
        vm->releaseAndDelete(target);
        vm->push(BOOL_VAL(false));
        return &instr + 1;
    }

    const DecodedInstruction* op_set_index_decoded(VM* vm, const DecodedInstruction& instr)
    {
        Value value  = vm->pop();
        Value index  = vm->pop();
        Value target = vm->peek(0);  // keep target on stack per op_set_index contract

        if (!IS_INSTANCE(target) && !IS_STRING(target) && !IS_VECTOR(target))
        {
            vm->releaseAndDelete(value);
            vm->releaseAndDelete(index);
            vm->runtimeError("Can only index vectors, tables/instances, or strings");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Vector indexing.
        if (IS_VECTOR(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            ObjVector* vec = vm->asVector(target);
            int idx = AS_INT(index);
            if (idx < 0) idx = static_cast<int>(vec->fields.size()) + idx;

            // Append-at-end is allowed (push_back behaviour).
            if (idx == static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(index);
                vec->fields.push_back(vm->retainValue(value));
                vm->releaseAndDelete(value);
                return &instr + 1;
            }

            if (idx < 0 || idx > static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            vm->releaseAndDelete(index);
            vm->releaseAndDelete(vec->fields[idx]);
            vec->fields[idx] = vm->retainValue(value);
            vm->releaseAndDelete(value);
            return &instr + 1;
        }

        // Instance / table indexing.
        if (IS_INSTANCE(target))
        {
            ObjInstance* inst = vm->asInstance(target);

            std::string key;
            if (IS_INT(index))
            {
                key = std::to_string(AS_INT(index));
            }
            else if (IS_STRING(index))
            {
                key = vm->asString(index);
            }
            else
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Index must be integer or string");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            vm->releaseAndDelete(index);

            // Fields starting with '__' bypass metamethods (prevents
            // metamethod implementations from re-triggering themselves).
            const bool isInternalField =
                (key.length() >= 2 and key[0] == '_' and key[1] == '_');

            // Look up __set metamethod.
            Value setMethod;
            bool hasSetMethod = false;
            bool isDynamicSet = false;

            if (not isInternalField and inst->klass)
            {
                auto setMetaIt = inst->klass->methods.find("__set");
                if (setMetaIt != inst->klass->methods.end())
                {
                    setMethod = setMetaIt->second;
                    hasSetMethod = true;
                }
                else if (inst->hasField("__set"))
                {
                    setMethod = inst->getField("__set");
                    hasSetMethod = true;
                    isDynamicSet = true;
                }
            }

            if (hasSetMethod and (IS_CLOSURE(setMethod) or IS_NAT_FUNC(setMethod)))
            {
                Value keyValue = vm->createString(key);

                if (IS_CLOSURE(setMethod))
                {
                    vm->push(target);
                    vm->push(keyValue);
                    vm->push(value);

                    const int frameCountBefore = vm->frameCount;

                    // Resume point captured into the metamethod's frame.
                    vm->pendingCallResume = &instr + 1;

                    if (isDynamicSet)
                    {
                        // Rewrite to [...] [setMethod] [instance] [key] [value].
                        Value val  = vm->pop();
                        Value k    = vm->pop();
                        Value inst2 = vm->pop();
                        vm->push(vm->retainValue(setMethod));
                        vm->push(inst2);
                        vm->push(k);
                        vm->push(val);

                        if (not vm->call(vm->asClosure(setMethod), 3))
                        {
                            vm->releaseAndDelete(setMethod);
                            vm->releaseAndDelete(keyValue);
                            vm->releaseAndDelete(value);
                            vm->runtimeError("Cannot call __set metamethod.");
                            vm->vm_return(InterpretResult::RUNTIME_ERROR);
                            return nullptr;
                        }
                    }
                    else
                    {
                        if (not vm->callBound(vm->asClosure(setMethod), 2))
                        {
                            vm->releaseAndDelete(keyValue);
                            vm->releaseAndDelete(value);
                            vm->runtimeError("Cannot call __set metamethod.");
                            vm->vm_return(InterpretResult::RUNTIME_ERROR);
                            return nullptr;
                        }
                    }

                    vm->releaseAndDelete(keyValue);
                    vm->releaseAndDelete(value);

                    return vm->completeDecodedFrameSwitch(frameCountBefore, &instr + 1);
                }
                else // IS_NAT_FUNC(setMethod)
                {
                    auto* native = vm->asNativeFunc(setMethod);

                    vm->push(target);
                    vm->push(keyValue);
                    vm->push(value);

                    native->function(vm, 3, vm->stack.data() + vm->stack.size() - 3);

                    vm->pop(); // value
                    vm->pop(); // keyValue
                    vm->pop(); // target
                    vm->releaseAndDelete(keyValue);
                    vm->releaseAndDelete(value);
                    // Note: target stays on stack (peek(0)) per op_set_index contract.
                    return &instr + 1;
                }
            }

            // Plain field assignment.
            auto fieldIt = inst->internedFields.find(key);
            if (fieldIt != inst->internedFields.end())
            {
                size_t valueIndex = fieldIt->second;
                vm->releaseAndDelete(inst->fieldValues[valueIndex]);
                inst->fieldValues[valueIndex] = vm->retainValue(value);
            }
            else
            {
                size_t newIndex = inst->fieldValues.size();
                inst->fieldValues.push_back(vm->retainValue(value));
                inst->internedFields[key] = newIndex;
            }

            vm->releaseAndDelete(value);
            return &instr + 1;
        }

        // String indexing — assign one character at idx (append allowed).
        if (IS_STRING(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("String index must be integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            if (!IS_STRING(value))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Can only assign string to string index");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            std::string str = vm->asString(target);
            int idx = AS_INT(index);
            if (idx < 0) idx = static_cast<int>(str.length()) + idx;

            if (idx < 0 || idx > static_cast<int>(str.length()))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("String index out of range");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            std::string valueString = vm->asString(value);
            if (valueString.length() != 1)
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Can only assign single character to string index");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            if (idx == static_cast<int>(str.length()))
            {
                str += valueString[0];
            }
            else
            {
                str[idx] = valueString[0];
            }

            vm->pop();                          // remove old string
            vm->push(vm->createString(str));    // push new one
            vm->releaseAndDelete(value);
            vm->releaseAndDelete(index);
            return &instr + 1;
        }

        // Unreachable: the type guard at the top rejects anything that is
        // not a vector, instance, or string.
        return &instr + 1;
    }

    // ========================================================================
    // Iterator Operations
    // ========================================================================

    const DecodedInstruction* op_get_iterator_decoded(VM* vm, const DecodedInstruction& instr)
    {
        Value tableVal = vm->peek(0);  // Don't pop, we keep table on stack

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only iterate over tables");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Create an integer to track iteration index (we'll iterate by index in fields map)
        // Push 0 as the initial iterator state
        vm->push(makeIntValue(0));
        return &instr + 1;
    }

    const DecodedInstruction* op_iterator_next_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Stack layout: [table, iterator_state]
        // We need to:
        // 1. Check if iterator is past the end
        // 2. If not, push the current key
        // 3. Increment the iterator
        // Final stack: [table, iterator_state+1, key] or [table, iterator_state, false]

        Value iteratorState = vm->peek(0);  // Peek, don't pop yet
        Value tableVal = vm->peek(1);        // Table is one below

        if (!IS_INT(iteratorState))
        {
            vm->runtimeError("Invalid iterator state");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only iterate over tables");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        ObjInstance* table = vm->asInstance(tableVal);
        int64_t index = AS_INT(iteratorState);

        // Determine total field count
        size_t totalFields = table->fieldValues.size();

        // Check if we've reached the end
        if (static_cast<size_t>(index) >= totalFields)
        {
            // End of iteration - push false to indicate done
            // Stack remains: [table, iterator_state, false]
            vm->push(makeBoolValue(false));
            return &instr + 1;
        }

        // Get the key at the current index
        // We need to find the key that maps to this index
        std::string key;
        for (const auto& pair : table->internedFields)
        {
            if (pair.second == static_cast<size_t>(index))
            {
                key = pair.first;  // pair.first is the string key
                break;
            }
        }

        // Pop old iterator state
        vm->pop();  // Remove old iterator state
        vm->releaseAndDelete(iteratorState);

        // Push incremented iterator state
        vm->push(makeIntValue(index + 1));

        // Push the key as a string
        Value keyVal = vm->createString(key);
        vm->push(keyVal);

        // Final stack: [table, iterator_state+1, key]
        return &instr + 1;
    }

    const DecodedInstruction* op_table_size_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Stack: [table or vector]
        Value tableVal = vm->peek(0);

        if (IS_VECTOR(tableVal))
        {
            ObjVector* vector = vm->asVector(tableVal);

            // Pop the vector
            vm->pop();
            vm->releaseAndDelete(tableVal);

            // Push the size as an integer
            vm->push(makeIntValue(static_cast<int64_t>(vector->fields.size())));
            return &instr + 1;
        }

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only get size of tables or vectors");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        ObjInstance* table = vm->asInstance(tableVal);

        // Pop the table
        vm->pop();
        vm->releaseAndDelete(tableVal);

        // Push the field count
        size_t totalSize = table->fieldValues.size();
        vm->push(makeIntValue(static_cast<int64_t>(totalSize)));
        return &instr + 1;
    }

    const DecodedInstruction* op_table_at_decoded(VM* vm, const DecodedInstruction& instr)
    {
        // Stack: [table or vector, index]
        Value indexVal = vm->pop();
        Value tableVal = vm->pop();

        // Handle vector access (direct indexed access, returns value at index)
        if (IS_VECTOR(tableVal))
        {
            if (!IS_INT(indexVal))
            {
                vm->releaseAndDelete(indexVal);
                vm->releaseAndDelete(tableVal);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            ObjVector* vector = vm->asVector(tableVal);
            int64_t index = AS_INT(indexVal);

            // Check bounds
            if (index < 0 || static_cast<size_t>(index) >= vector->fields.size())
            {
                vm->releaseAndDelete(indexVal);
                vm->releaseAndDelete(tableVal);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            // Get the value directly
            Value value = vector->fields[index];

            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);

            // For vectors, return the value directly (not the index)
            // This is different from tables where we return the key
            vm->push(vm->retainValue(value));
            return &instr + 1;
        }

        // Handle table access (returns key at index position in map)
        if (!IS_INSTANCE(tableVal))
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Can only get keys from tables or vectors");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        if (!IS_INT(indexVal))
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Table index must be an integer");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        ObjInstance* table = vm->asInstance(tableVal);
        int64_t index = AS_INT(indexVal);

        // Determine total field count
        size_t totalFields = table->fieldValues.size();

        // Check bounds
        if (index < 0 || static_cast<size_t>(index) >= totalFields)
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Table index out of bounds");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // Get the key at the specified index
        std::string key;
        for (const auto& pair : table->internedFields)
        {
            if (pair.second == static_cast<size_t>(index))
            {
                key = pair.first;  // pair.first is the string key
                break;
            }
        }

        vm->releaseAndDelete(indexVal);
        vm->releaseAndDelete(tableVal);

        // Push the key as a string
        Value keyVal = vm->createString(key);
        vm->push(keyVal);
        return &instr + 1;
    }

    // ---------------------------------------------------------------------
    // Proper decoded handlers. One-byte-operand ops read from
    // instr.operands.byte; zero-operand ops delegate directly to the legacy
    // body (no benefit to duplicating). op_closure has a variable-length
    // upvalue payload that can't fit in DecodedInstruction.operands, so its
    // decoded form pre-extracts the constant index but still reads the
    // upvalue list from bytecode via ip after positioning ip past the
    // constant byte.
    // ---------------------------------------------------------------------

    const DecodedInstruction* op_closure_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t constantIndex = instr.operands.byte;

        // Variable-length upvalue payload (2 bytes per upvalue) can't be
        // pre-decoded — position ip past the opcode + constant byte so the
        // per-upvalue *ip++ reads land on the right bytes. The bytecode
        // offset is cold metadata, looked up via the owning chunk's
        // parallel array (this op is rare; the lookup is off the hot path).
        const DecodedChunk* ownerDecoded = vm->currentFrame->closure->function->decodedChunk;
        const size_t bytecodeOffset =
            ownerDecoded->meta[&instr - ownerDecoded->instructions.data()].bytecodeOffset;

        vm->currentFrame->ip = vm->currentFrame->closure->function->chunk.code.data()
                             + bytecodeOffset + 2;

        auto functionValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        if (not IS_FUNC(functionValue))
        {
            vm->runtimeError("Closure operand must be a function.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        ObjFunction* function = vm->asFunction(functionValue);
        auto closure = vm->createClosure(function);
        vm->push(closure);

        for (int i = 0; i < function->upvalueCount; i++)
        {
            uint8_t isLocal = *vm->currentFrame->ip++;
            uint8_t index = *vm->currentFrame->ip++;
            if (isLocal)
            {
                vm->asClosure(closure)->upvalues[i] = vm->captureUpvalue(vm->currentFrame->slots + index);
            }
            else
            {
                vm->asClosure(closure)->upvalues[i] = vm->currentFrame->closure->upvalues[index];
            }
        }
        return &instr + 1;
    }

    const DecodedInstruction* op_class_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t constantIndex = instr.operands.byte;
        auto classNameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        ElementType classNameElem = vm->valueToElement(classNameValue);
        if (not classNameElem.isLitteral())
        {
            vm->runtimeError("Class name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        std::string className = classNameElem.toString();
        auto newClass = vm->createClass(className);
        vm->push(newClass);
        return &instr + 1;
    }

    const DecodedInstruction* op_method_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t constantIndex = instr.operands.byte;
        auto methodNameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        ElementType methodNameElem = vm->valueToElement(methodNameValue);
        if (not methodNameElem.isLitteral())
        {
            vm->runtimeError("Method name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        std::string methodName = methodNameElem.toString();

        auto methodValue = vm->pop();
        auto classValue = vm->peek();

        if (not IS_CLASS(classValue))
        {
            vm->runtimeError("Method definition must be on a class.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        Klass* klass = vm->asClass(classValue);

        if (not IS_CLOSURE(methodValue) and not IS_NAT_FUNC(methodValue))
        {
            vm->runtimeError("Method must be a closure or native function.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        klass->methods[methodName] = methodValue;
        return &instr + 1;
    }

    const DecodedInstruction* op_build_table_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t pairCount = instr.operands.byte;

        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            vm->runtimeError("Table class not found - was initializeTableClass() called?");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        Value tableClassVal = it->second;
        if (!IS_CLASS(tableClassVal))
        {
            vm->runtimeError("Table is not a class");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        Klass* tableClass = vm->asClass(tableClassVal);

        Value instanceVal = vm->createInstance(tableClass);
        ObjInstance* table = vm->asInstance(instanceVal);

        table->fieldValues.reserve(pairCount);
        table->internedFields.reserve(pairCount);

        std::vector<std::string> keys;
        std::vector<Value> values;
        keys.reserve(pairCount);
        values.reserve(pairCount);

        for (int i = 0; i < pairCount; i++)
        {
            Value key = vm->pop();
            Value value = vm->pop();

            std::string keyStr;
            if (IS_STRING(key))
            {
                keyStr = vm->asString(key);
            }
            else if (IS_INT(key))
            {
                keyStr = std::to_string(AS_INT(key));
            }
            else
            {
                vm->releaseAndDelete(key);
                vm->releaseAndDelete(value);
                vm->runtimeError("Table key must be string or integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            vm->releaseAndDelete(key);
            keys.push_back(keyStr);
            values.push_back(value);
        }

        for (int i = pairCount - 1; i >= 0; i--)
        {
            size_t valueIndex = table->fieldValues.size();
            table->fieldValues.push_back(vm->retainValue(values[i]));
            table->internedFields[keys[i]] = valueIndex;
            vm->releaseAndDelete(values[i]);
        }

        vm->push(instanceVal);
        return &instr + 1;
    }

    const DecodedInstruction* op_build_vector_decoded(VM* vm, const DecodedInstruction& instr)
    {
        uint8_t pairCount = instr.operands.byte;

        Value vectorVal = vm->createVector();
        ObjVector* vector = vm->asVector(vectorVal);

        std::vector<std::pair<int64_t, Value>> pairs;
        pairs.reserve(pairCount);

        for (int i = 0; i < pairCount; i++)
        {
            Value index = vm->pop();
            Value value = vm->pop();

            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(vectorVal);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }

            int64_t indexInt = AS_INT(index);
            vm->releaseAndDelete(index);
            pairs.push_back({indexInt, value});
        }

        std::sort(pairs.begin(), pairs.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& pair : pairs)
        {
            // Pad with zeros so the value lands at its target index even
            // when the keys aren't a contiguous 0..N-1 sequence.
            while (vector->fields.size() <= static_cast<size_t>(pair.first))
            {
                vector->fields.push_back(makeIntValue(0));
            }

            vector->fields[pair.first] = vm->retainValue(pair.second);
            vm->releaseAndDelete(pair.second);
        }

        vm->push(vectorVal);
        return &instr + 1;
    }

    // Zero-operand ops: no bytecode to pre-extract, so the decoded form
    // just delegates to the legacy body.

}