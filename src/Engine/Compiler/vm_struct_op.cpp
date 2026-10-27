#include "stdafx.h"

#include "vm.h"
#include "decoded_chunk.h"
#include "componentproxy.h"

namespace pg
{
    namespace
    {
        // Names starting with '__' always address the instance itself, never a metamethod nor a proxied component
        inline bool isInternalName(const std::string& name)
        {
            return name.length() >= 2 and name[0] == '_' and name[1] == '_';
        }
    }

    bool invoke(VM* vm, const std::string& name, uint8_t argCount)
    {
        auto receiverValue = vm->peek(argCount);

        if (not IS_INSTANCE(receiverValue))
        {
            return false;
        }

        ObjInstance* instance = vm->asInstance(receiverValue);

        // Check for field
        const size_t fieldSlot = instance->findField(name);
        if (fieldSlot != ObjInstance::npos)
        {
            Value fieldValue = instance->fieldValue(fieldSlot);
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

        // Property name pre-resolved at decode time, in the instruction's cache
        PropertyCache* cache = instr.propertyCache;
        const std::string& nameStr = *cache->name;

        // Component proxy: forward straight to the C++ component, a proxy holds no field and needs no metamethod call
        if (instance->proxyMeta != nullptr and not isInternalName(nameStr))
        {
            Value result = ComponentProxy::getProperty(vm, instance, nameStr);

            auto inst = vm->pop();
            vm->releaseAndDelete(inst);

            vm->push(result);
            return &instr + 1;
        }

        // The shape this read saw last, or one any read saw: the field is at the slot learned
        Shape* shape = instance->layout();

        if (shape != nullptr)
        {
            size_t learned = ObjInstance::npos;

            if (shape == cache->shape)
            {
                learned = cache->slot;
            }
            else
            {
                const VM::FieldStub& stub = vm->slotStub(shape, cache->name);

                if (stub.shape == shape and stub.name == cache->name)
                {
                    learned = stub.slot;
                    cache->shape = shape;
                    cache->slot = stub.slot;
                }
            }

            if (learned != ObjInstance::npos)
            {
                Value found = vm->retainValue(instance->fieldValue(learned));
                auto inst = vm->pop();
                vm->releaseAndDelete(inst);
                vm->push(found);
                return &instr + 1;
            }
        }

        // Field-found branch (no frame change).
        const size_t fieldSlot = instance->findField(nameStr);
        if (fieldSlot != ObjInstance::npos)
        {
            if (shape != nullptr)
            {
                cache->shape = shape;
                cache->slot = static_cast<uint32_t>(fieldSlot);

                VM::FieldStub& stub = vm->slotStub(shape, cache->name);
                stub.shape = shape;
                stub.name = cache->name;
                stub.slot = static_cast<uint32_t>(fieldSlot);
            }

            auto inst = vm->pop();
            vm->releaseAndDelete(inst);
            vm->push(vm->retainValue(instance->fieldValue(fieldSlot)));
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
            const size_t dynSlot = instance->findField("__get");
            if (dynSlot != ObjInstance::npos)
            {
                getMethod = instance->fieldValue(dynSlot);
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
            // Neither a field nor a method: a missing property reads as none
            auto inst = vm->pop();
            vm->releaseAndDelete(inst);

            vm->push(makeNoneValue());
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

        // Property name pre-resolved at decode time, in the instruction's cache
        PropertyCache* cache = instr.propertyCache;
        const std::string& nameStr = *cache->name;

        // Fields starting with '__' bypass metamethods to allow metamethod
        // implementations to write through to internal storage without
        // re-triggering themselves.
        bool isInternalField  = isInternalName(nameStr);
        bool isInternalAccess = isInternalField;

        // Component proxy: write through the component setter, which fires its change events
        if (instance->proxyMeta != nullptr and not isInternalField)
        {
            if (not ComponentProxy::setProperty(vm, instance, nameStr, vm->peek(0)))
                return vm->raiseError("Property '" + nameStr + "' is read-only");

            // The assigned value stays on the stack as the result of the expression
            Value value = vm->pop();
            auto inst = vm->pop();
            vm->releaseAndDelete(inst);

            vm->push(value);
            return &instr + 1;
        }

        // A table on the shapes whose class and shape have no __set: a plain write, at the slot
        // this write (or any) learned for the shape, or by the step learned for adding the field
        Shape* shape = instance->layout();

        if (shape != nullptr and not instance->klass->hasSetMeta() and not shape->hasSetField())
        {
            auto value = vm->pop();
            auto inst  = vm->pop();
            vm->releaseAndDelete(inst);

            size_t slot = ObjInstance::npos;

            if (shape == cache->shape)
            {
                slot = cache->slot;
            }
            else if (shape == cache->addFrom)
            {
                instance->appendAlong(cache->addTo, vm->retainValue(value));
                vm->push(value);
                return &instr + 1;
            }
            else
            {
                const VM::FieldStub& stub = vm->slotStub(shape, cache->name);

                if (stub.shape == shape and stub.name == cache->name)
                {
                    slot = stub.slot;
                }
                else
                {
                    const VM::FieldStub& added = vm->addStub(shape, cache->name);

                    if (added.shape == shape and added.name == cache->name)
                    {
                        cache->addFrom = shape;
                        cache->addTo = added.next;
                        instance->appendAlong(added.next, vm->retainValue(value));
                        vm->push(value);
                        return &instr + 1;
                    }

                    slot = instance->findField(nameStr);
                }

                if (slot != ObjInstance::npos)
                {
                    cache->shape = shape;
                    cache->slot = static_cast<uint32_t>(slot);

                    VM::FieldStub& learned = vm->slotStub(shape, cache->name);
                    learned.shape = shape;
                    learned.name = cache->name;
                    learned.slot = static_cast<uint32_t>(slot);
                }
            }

            if (slot != ObjInstance::npos)
            {
                vm->releaseAndDelete(instance->fieldValueRef(slot));
                instance->fieldValueRef(slot) = vm->retainValue(value);
            }
            else
            {
                instance->appendNewField(nameStr, vm->retainValue(value));

                // The step it took, when it stayed on the shapes
                if (Shape* next = instance->layout())
                {
                    cache->addFrom = shape;
                    cache->addTo = next;

                    VM::FieldStub& step = vm->addStub(shape, cache->name);
                    step.shape = shape;
                    step.name = cache->name;
                    step.next = next;
                }
            }

            vm->push(value);
            return &instr + 1;
        }

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
                const size_t dynSlot = instance->findField("__set");
                if (dynSlot != ObjInstance::npos)
                {
                    setMethod = instance->fieldValue(dynSlot);
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

        const size_t fieldSlot = instance->findField(nameStr);
        if (fieldSlot != ObjInstance::npos)
        {
            vm->releaseAndDelete(instance->fieldValueRef(fieldSlot));
            instance->fieldValueRef(fieldSlot) = vm->retainValue(value);
        }
        else
        {
            instance->appendNewField(nameStr, vm->retainValue(value));
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

            // Reading past either end gives none, there is nothing there
            if (idx < 0 || idx >= static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(target);
                vm->push(makeNoneValue());
                return &instr + 1;
            }

            // Retain the element before letting go of the vector, it may be its last owner
            Value element = vm->retainValue(vec->fields[idx]);

            vm->releaseAndDelete(target);
            vm->push(element);
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

            // Same as a vector: reading past either end gives none
            if (idx < 0 || idx >= static_cast<int>(str.length()))
            {
                if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
                vm->push(makeNoneValue());
                return &instr + 1;
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

        const size_t fieldSlot = inst->findField(key);
        if (fieldSlot != ObjInstance::npos)
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->push(vm->retainValue(inst->fieldValue(fieldSlot)));
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
                const size_t dynSlot = inst->findField("__get");
                if (dynSlot != ObjInstance::npos)
                {
                    getMethod = inst->fieldValue(dynSlot);
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

        // No metamethod, no field: a missing key reads as none
        vm->releaseAndDelete(index);
        vm->releaseAndDelete(target);
        vm->push(makeNoneValue());
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
            const size_t fieldSlot = inst->findField(key);
            if (fieldSlot != ObjInstance::npos)
            {
                vm->releaseAndDelete(inst->fieldValueRef(fieldSlot));
                inst->fieldValueRef(fieldSlot) = vm->retainValue(value);
            }
            else
            {
                inst->appendNewField(key, vm->retainValue(value));
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
        size_t totalFields = table->fieldCount();

        // Check if we've reached the end
        if (static_cast<size_t>(index) >= totalFields)
        {
            // End of iteration - push false to indicate done
            // Stack remains: [table, iterator_state, false]
            vm->push(makeBoolValue(false));
            return &instr + 1;
        }

        // The key of the current slot (it was found by scanning the whole name index, every step)
        const std::string key = table->fieldName(static_cast<size_t>(index));

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
        size_t totalSize = table->fieldCount();
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
        size_t totalFields = table->fieldCount();

        // Check bounds
        if (index < 0 || static_cast<size_t>(index) >= totalFields)
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Table index out of bounds");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // The key of that slot, in O(1) (a scan of the name index made every for-in over a table
        // quadratic in its size)
        const std::string& key = table->fieldName(static_cast<size_t>(index));

        vm->releaseAndDelete(indexVal);
        vm->releaseAndDelete(tableVal);

        // Push the key as a string
        Value keyVal = vm->createString(key);
        vm->push(keyVal);
        return &instr + 1;
    }

    // Whether a value is an instance whose class has that method. A table's own field of that name
    // does not count: a table keeps being looped over by its keys
    static bool classHasMethod(VM* vm, const Value& value, const char* name)
    {
        if (not IS_INSTANCE(value))
            return false;

        const ObjInstance* inst = vm->asInstance(value);

        return inst->klass and inst->klass->methods.find(name) != inst->klass->methods.end();
    }

    // for-in: an iterable, looped over through what its __iter() returns
    const DecodedInstruction* op_has_iter_decoded(VM* vm, const DecodedInstruction& instr)
    {
        Value value = vm->pop();
        const bool has = classHasMethod(vm, value, "__iter");
        vm->releaseAndDelete(value);

        vm->push(BOOL_VAL(has));
        return &instr + 1;
    }

    // for-in: an iterator, whose __next() is called until it returns none
    const DecodedInstruction* op_has_next_decoded(VM* vm, const DecodedInstruction& instr)
    {
        Value value = vm->pop();
        const bool has = classHasMethod(vm, value, "__next");
        vm->releaseAndDelete(value);

        vm->push(BOOL_VAL(has));
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

        // The class by its slot, found once (not a name looked up for every table)
        Value tableClassVal = vm->tableClassValue();
        if (IS_UNDEFINED(tableClassVal))
        {
            vm->runtimeError("Table class not found - was initializeTableClass() called?");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        if (!IS_CLASS(tableClassVal))
        {
            vm->runtimeError("Table is not a class");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return nullptr;
        }

        // The pairs lie on the stack in the order they were written, each value under its key:
        // they are read where they are, the keys checked first so that a bad one leaves nothing
        // half built
        const size_t base = vm->stack.size() - 2 * static_cast<size_t>(pairCount);

        for (size_t i = 0; i < pairCount; i++)
        {
            const Value key = vm->stack[base + 2 * i + 1];

            if (not IS_STRING(key) and not IS_INT(key))
            {
                for (size_t j = base; j < vm->stack.size(); j++)
                    vm->releaseAndDelete(vm->stack[j]);

                vm->stack.truncateTo(base);
                vm->runtimeError("Table key must be string or integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return nullptr;
            }
        }

        Klass* tableClass = vm->asClass(tableClassVal);

        Value instanceVal = vm->createInstance(tableClass);
        ObjInstance* table = vm->asInstance(instanceVal);

        // The same keys as the last time: the shape they made then, and the values in place
        LiteralCache* literal = instr.literalCache;

        if (literal != nullptr and literal->shape != nullptr and literal->epoch == vm->cacheEpoch and literal->keys.size() == pairCount)
        {
            bool same = true;

            for (size_t i = 0; i < pairCount and same; i++)
                same = vm->stack[base + 2 * i + 1] == literal->keys[i];

            if (same)
            {
                // The stack's references to the values are the table's now; the keys are released
                table->fillShaped(literal->shape, &vm->stack[base], pairCount, 2);

                for (size_t i = 0; i < pairCount; i++)
                    vm->releaseAndDelete(vm->stack[base + 2 * i + 1]);

                vm->stack.truncateTo(base);
                vm->push(instanceVal);
                return &instr + 1;
            }
        }

        table->reserveFields(pairCount);

        for (size_t i = 0; i < pairCount; i++)
        {
            const Value value = vm->stack[base + 2 * i];
            const Value key = vm->stack[base + 2 * i + 1];

            std::string keyStr = IS_STRING(key) ? vm->asString(key) : std::to_string(AS_INT(key));

            // A key written twice keeps both slots, found at the last (as before). The stack's
            // reference to the value is the table's now: no retain, no release
            table->appendField(std::move(keyStr), value);
        }

        // Learned for the next time, when the table is on the shapes and every key is a value whose
        // meaning cannot change (an integer, an inline or interned string, a constant one): a string
        // freed and made again could otherwise come back with the same bits and other words
        if (literal != nullptr and table->layout() != nullptr)
        {
            bool stable = true;

            for (size_t i = 0; i < pairCount and stable; i++)
            {
                const Value key = vm->stack[base + 2 * i + 1];
                stable = IS_INT(key) or IS_SMALL_STRING(key) or IS_INTERNED_STRING(key) or (IS_STRING(key) and vm->pools.isConstant(key));
            }

            if (stable)
            {
                literal->shape = table->layout();
                literal->epoch = vm->cacheEpoch;
                literal->keys.assign(pairCount, Value{});

                for (size_t i = 0; i < pairCount; i++)
                    literal->keys[i] = vm->stack[base + 2 * i + 1];
            }
        }

        for (size_t i = 0; i < pairCount; i++)
            vm->releaseAndDelete(vm->stack[base + 2 * i + 1]);

        vm->stack.truncateTo(base);

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
            // Pad with none so the value lands at its target index even
            // when the keys aren't a contiguous 0..N-1 sequence.
            while (vector->fields.size() <= static_cast<size_t>(pair.first))
            {
                vector->fields.push_back(makeNoneValue());
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