#include "stdafx.h"

#include "vm.h"

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

    void op_invoke(VM* vm)
    {
        uint8_t stringIndex = *vm->currentFrame->ip++;
        uint8_t argCount = *vm->currentFrame->ip++;

        // Get method name from constant strings (no conversion needed!)
        auto* function = vm->currentFrame->closure->function;
        const std::string& methodName = function->chunk.constantStrings[stringIndex];

        if (not invoke(vm, methodName, argCount))
        {
            vm->runtimeError("Method '" + methodName + "' not found.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        vm->currentFrame = &vm->frames[vm->frameCount - 1];
        vm->updateChunkCache();
    }

    void op_closure(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto functionValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        if (not IS_FUNC(functionValue))
        {
            vm->runtimeError("Closure operand must be a function.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        ObjFunction* function = vm->asFunction(functionValue);
        auto closure = vm->createClosure(function);  // Already tracked in createClosure
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
    }


    void op_class(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto classNameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];

        ElementType classNameElem = vm->valueToElement(classNameValue);
        if (not classNameElem.isLitteral())
        {
            vm->runtimeError("Class name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        std::string className = classNameElem.toString();
        auto newClass = vm->createClass(className);  // Already tracked in createClass
        vm->push(newClass);
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

    void op_get_property(VM* vm)
    {
        if (not IS_INSTANCE(vm->peek(0)))
        {
            vm->runtimeError("Only instances have properties.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        auto* instance = vm->asInstance(vm->peek(0));


        uint8_t stringIndex = *vm->currentFrame->ip++;
        auto* function = vm->currentFrame->closure->function;

        // Get property name from constant strings (no conversion needed!)
        const std::string& nameStr = function->chunk.constantStrings[stringIndex];

        // Try to find field (direct string lookup, O(1) average case)
        auto fieldIt = instance->internedFields.find(nameStr);
        if (fieldIt != instance->internedFields.end())
        {
            auto inst = vm->pop(); // Remove the instance from the stack
            vm->releaseAndDelete(inst);
            vm->push(vm->retainValue(instance->fieldValues[fieldIt->second]));
            return;
        }

        // Check for __get metamethod in class methods OR instance fields
        Value getMethod;
        bool hasGetMethod = false;
        bool isDynamicGet = false;

        // First check class methods
        auto getMetaIt = instance->klass->methods.find("__get");
        if (getMetaIt != instance->klass->methods.end())
        {
            getMethod = getMetaIt->second;
            hasGetMethod = true;
        }
        else
        {
            // Also check instance fields for __get (for dynamic metamethods)
            auto fieldIt = instance->internedFields.find("__get");
            if (fieldIt != instance->internedFields.end())
            {
                getMethod = instance->fieldValues[fieldIt->second];
                hasGetMethod = true;
                isDynamicGet = true;
            }
        }

        if (hasGetMethod and (IS_CLOSURE(getMethod) or IS_NAT_FUNC(getMethod)))
        {
            // Call __get(instance, propertyName)
            // Create a real string Value for the metamethod
            Value nameValue = vm->createString(nameStr);

            if (IS_CLOSURE(getMethod))
            {
                // Push property name as argument
                vm->push(nameValue);

                if (isDynamicGet)
                {
                    // For dynamic __get (stored in instance field), we need to set up the stack as:
                    // [...] [getMethod] [instance] [propertyName]
                    // Currently stack is: [...] [instance] [propertyName]

                    // We need to insert the getMethod closure before the arguments
                    // Pop the arguments temporarily
                    Value propName = vm->pop();  // propertyName
                    Value inst = vm->pop();      // instance

                    // Push in correct order: getMethod, instance, propertyName
                    vm->push(vm->retainValue(getMethod));
                    vm->push(inst);
                    vm->push(propName);

                    // Call the __get method as a closure with stack [getMethod, inst, propName]
                    if (not vm->call(vm->asClosure(getMethod), 2))
                    {
                        vm->releaseAndDelete(getMethod);
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }

                }
                else
                {
                    // Call the __get method as a bound method (instance is already on stack)
                    if (not vm->callBound(vm->asClosure(getMethod), 1))
                    {
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }
                }

                // No need to pop the values as callValue handles that

                // Function has been called, frame is set up with stackBase pointing to where to truncate on return
                vm->currentFrame = &vm->frames[vm->frameCount - 1];
                vm->updateChunkCache(); // Update cached chunk data for new frame
                return;
            }
            else if (IS_NAT_FUNC(getMethod))
            {
                auto* native = vm->asNativeFunc(getMethod);

                // Call native __get(instance, propertyName)
                // Stack: [instance] -> args[0]=instance, args[1]=propertyName
                vm->push(nameValue);  // Push property name
                Value result = native->function(vm, 2, vm->stack.data() + vm->stack.size() - 2);

                // Remove arguments from stack
                auto propName = vm->pop();
                auto inst = vm->pop();
                vm->releaseAndDelete(propName);
                vm->releaseAndDelete(inst);

                vm->push(result);
                return;
            }
        }

        // Try to find a method in the class
        if (not bindMethod(vm, instance->klass, nameStr))
        {
            vm->runtimeError("Undefined property '" + nameStr + "'.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
        }
    }

    void op_set_property(VM *vm)
    {
        if (not IS_INSTANCE(vm->peek(1)))
        {
            vm->runtimeError("Only instances have fields.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        auto* instance = vm->asInstance(vm->peek(1));

        uint8_t stringIndex = *vm->currentFrame->ip++;
        auto* function = vm->currentFrame->closure->function;

        // Get property name from constant strings (no conversion needed!)
        const std::string& nameStr = function->chunk.constantStrings[stringIndex];

        // IMPORTANT: Fields starting with '__' (double underscore) are treated as "internal"
        // and bypass metamethods. This prevents infinite recursion when metamethods like
        // __set need to actually store data in internal fields.
        bool isInternalField = (nameStr.length() >= 2 and nameStr[0] == '_' and nameStr[1] == '_');

        // Also check if we're accessing from within a bound method of the same instance
        bool isInternalAccess = isInternalField;

        if (not isInternalAccess and vm->frameCount > 1)
        {
            // Check if current frame is a bound method call
            // In callBound(), stackBase is set to slots (see vm_helpers.cpp:222)
            bool isBoundMethodContext = (vm->currentFrame->stackBase == vm->currentFrame->slots);

            if (isBoundMethodContext)
            {
                Value currentReceiver = vm->currentFrame->slots[0];
                Value accessedInstance = vm->peek(1);

                // Only bypass if we're accessing the same instance that's the receiver
                if (IS_INSTANCE(currentReceiver) and currentReceiver == accessedInstance)
                {
                    isInternalAccess = true;
                }
            }
        }

        // Check for __set metamethod in class methods OR instance fields
        Value setMethod;
        bool hasSetMethod = false;
        bool isDynamicSet = false;

        // Only check for metamethods if this is NOT an internal access
        if (not isInternalAccess)
        {
            // First check class methods
            auto setMetaIt = instance->klass->methods.find("__set");
            if (setMetaIt != instance->klass->methods.end())
            {
                setMethod = setMetaIt->second;
                hasSetMethod = true;
            }
            else
            {
                // Also check instance fields for __set (for dynamic metamethods)
                auto fieldIt = instance->internedFields.find("__set");
                if (fieldIt != instance->internedFields.end())
                {
                    setMethod = instance->fieldValues[fieldIt->second];
                    hasSetMethod = true;
                    isDynamicSet = true;
                }
            }
        }

        if (hasSetMethod)
        {
            // Call __set(instance, propertyName, value)
            // Create a real string Value for the metamethod
            Value nameValue = vm->createString(nameStr);

            if (IS_CLOSURE(setMethod))
            {
                // Stack is currently: [instance, value]
                Value value = vm->pop();     // Pop value
                // instance is still on stack

                vm->push(nameValue);  // Push property name
                vm->push(value);      // Push value

                if (isDynamicSet)
                {
                    // For dynamic __set (instance field), we need to set up the stack as:
                    // [...] [setMethod] [instance] [propertyName] [value]
                    // Currently stack is: [...] [instance] [propertyName] [value]

                    // Pop the arguments temporarily
                    Value val = vm->pop();       // value
                    Value propName = vm->pop();  // propertyName
                    Value inst = vm->pop();      // instance

                    // Push in correct order: setMethod, instance, propertyName, value
                    vm->push(vm->retainValue(setMethod));
                    vm->push(inst);
                    vm->push(propName);
                    vm->push(val);

                    // For dynamic __set (instance field), call with 3 args: [instance, propertyName, value]
                    if (not vm->call(vm->asClosure(setMethod), 3))
                    {
                        vm->releaseAndDelete(setMethod);
                        vm->runtimeError("Cannot call __set metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }
                }
                else
                {
                    // For class method __set, call as bound method with 2 args: [propertyName, value]
                    if (not vm->callBound(vm->asClosure(setMethod), 2))
                    {
                    vm->runtimeError("Cannot call __set metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }
                }

                // Function has been called, frame is set up
                vm->currentFrame = &vm->frames[vm->frameCount - 1];
                vm->updateChunkCache();
                return;
            }
            else if (IS_NAT_FUNC(setMethod))
            {
                auto* native = vm->asNativeFunc(setMethod);

                // Stack: [instance, value]
                // Call native __set(instance, propertyName, value)
                Value value = vm->pop();  // Pop value temporarily

                vm->push(nameValue);  // Push property name
                vm->push(value);      // Push value

                // Now stack: [instance, propertyName, value]
                // Call with args[0]=instance, args[1]=propertyName, args[2]=value
                Value result = native->function(vm, 3, vm->stack.data() + vm->stack.size() - 3);

                // Clean up stack: remove [propertyName, value]
                vm->pop(); // value
                vm->pop(); // propertyName
                auto inst = vm->pop(); // instance
                vm->releaseAndDelete(inst);

                vm->push(result); // Push result (usually the value that was set)
                return;
            }
        }

        // No __set metamethod, do normal field assignment
        auto value = vm->pop(); // Value to set
        auto inst = vm->pop(); // Instance
        vm->releaseAndDelete(inst);

        // Check if field exists
        auto fieldIt = instance->internedFields.find(nameStr);
        if (fieldIt != instance->internedFields.end())
        {
            // Update existing field
            size_t valueIndex = fieldIt->second;
            vm->releaseAndDelete(instance->fieldValues[valueIndex]);
            instance->fieldValues[valueIndex] = vm->retainValue(value);
        }
        else
        {
            // New field - add to storage
            size_t newIndex = instance->fieldValues.size();
            instance->fieldValues.push_back(vm->retainValue(value));
            instance->internedFields[nameStr] = newIndex;
        }

        vm->push(value);
    }

    void op_method(VM* vm)
    {
        uint8_t constantIndex = *vm->currentFrame->ip++;

        auto methodNameValue = vm->currentFrame->closure->function->chunk.constants[constantIndex];
        ElementType methodNameElem = vm->valueToElement(methodNameValue);
        if (not methodNameElem.isLitteral())
        {
            vm->runtimeError("Method name must be a litteral.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        std::string methodName = methodNameElem.toString();

        // The class is below the method closure/native function on the stack
        auto methodValue = vm->pop();
        auto classValue = vm->peek();

        if (not IS_CLASS(classValue))
        {
            vm->runtimeError("Method definition must be on a class.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        Klass* klass = vm->asClass(classValue);

        // Methods can be either closures (script-defined) or native functions (C++-defined)
        if (not IS_CLOSURE(methodValue) and not IS_NAT_FUNC(methodValue))
        {
            vm->runtimeError("Method must be a closure or native function.");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);

            return;
        }

        klass->methods[methodName] = methodValue;
    }


    // ========================================================================
    // Table Operations
    // ========================================================================

    void op_build_table(VM* vm)
    {
        uint8_t pairCount = *vm->currentFrame->ip++;

        // Get the built-in Table class
        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            vm->runtimeError("Table class not found - was initializeTableClass() called?");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        Value tableClassVal = it->second;
        if (!IS_CLASS(tableClassVal))
        {
            vm->runtimeError("Table is not a class");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        Klass* tableClass = vm->asClass(tableClassVal);

        // Create new instance of Table
        Value instanceVal = vm->createInstance(tableClass);
        ObjInstance* table = vm->asInstance(instanceVal);

        // Pre-allocate storage
        table->fieldValues.reserve(pairCount);
        table->internedFields.reserve(pairCount);

        // Pop pairCount key-value pairs from stack (in reverse)
        std::vector<std::string> keys;
        std::vector<Value> values;
        keys.reserve(pairCount);
        values.reserve(pairCount);

        for (int i = 0; i < pairCount; i++)
        {
            Value key = vm->pop();
            Value value = vm->pop();

            // Convert key to string and intern
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
                return;
            }

            // Intern the key string
            vm->releaseAndDelete(key);  // We've converted it, release original

            keys.push_back(keyStr);
            values.push_back(value);
        }

        // Insert pairs in correct order (we popped in reverse)
        for (int i = pairCount - 1; i >= 0; i--)
        {
            size_t valueIndex = table->fieldValues.size();
            table->fieldValues.push_back(vm->retainValue(values[i]));
            table->setField(keys[i], valueIndex);  // it->first is the string key
            vm->releaseAndDelete(values[i]);  // Release our temporary reference
        }

        // Push the table instance we created
        vm->push(instanceVal);
    }

    void op_build_vector(VM* vm)
    {
        uint8_t pairCount = *vm->currentFrame->ip++;

        // Create new vector
        Value vectorVal = vm->createVector();
        ObjVector* vector = vm->asVector(vectorVal);

        // Pop pairCount key-value pairs from stack (in reverse order)
        // Stack layout: [value, index, value, index, ...]
        std::vector<std::pair<int64_t, Value>> pairs;
        pairs.reserve(pairCount);

        for (int i = 0; i < pairCount; i++)
        {
            Value index = vm->pop();
            Value value = vm->pop();

            // Index must be an integer
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(value);
                // Clean up the vector we created before returning
                vm->releaseAndDelete(vectorVal);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            int64_t indexInt = AS_INT(index);
            vm->releaseAndDelete(index);  // We've extracted the int, release the value
            pairs.push_back({indexInt, value});
        }

        // Sort pairs by index to ensure correct order
        std::sort(pairs.begin(), pairs.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        // Insert values in sorted order
        for (const auto& pair : pairs)
        {
            // Ensure vector is large enough
            while (vector->fields.size() <= static_cast<size_t>(pair.first))
            {
                vector->fields.push_back(makeIntValue(0));  // Fill with zeros
            }

            vector->fields[pair.first] = vm->retainValue(pair.second);
            vm->releaseAndDelete(pair.second);  // Release our temporary reference
        }

        // Push the vector we created
        vm->push(vectorVal);
    }

    void op_get_index(VM* vm)
    {
        Value index = vm->pop();
        Value target = vm->pop();

        // Handle vector indexing
        if (IS_VECTOR(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(target);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            ObjVector* vec = vm->asVector(target);
            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(vec->fields.size()) + idx;
            }

            if (idx < 0 || idx >= static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(target);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            vm->releaseAndDelete(target);

            // Return the value at the index
            vm->push(vm->retainValue(vec->fields[idx]));
            return;
        }

        // Handle string indexing (both long and small strings)
        if (IS_STRING(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(index);
                if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
                vm->runtimeError("String index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            std::string str = vm->asString(target);

            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(str.length()) + idx;
            }

            if (idx < 0 || idx >= static_cast<int>(str.length()))
            {
                if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);
                vm->runtimeError("String index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            if (IS_LONG_STRING(target)) vm->releaseAndDelete(target);

            // Return single character as a string using cached value
            unsigned char ch = static_cast<unsigned char>(str[idx]);
            vm->push(vm->retainValue(vm->singleCharCache[ch]));
            return;
        }

        // Handle table/instance indexing
        if (!IS_INSTANCE(target))
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->runtimeError("Can only index vectors, strings, or tables/instances");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* inst = vm->asInstance(target);

        // Convert index to string key
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
            return;
        }

        // Look up field
        auto fieldIt = inst->internedFields.find(key);
        if (fieldIt != inst->internedFields.end())
        {
            vm->releaseAndDelete(index);
            vm->releaseAndDelete(target);
            vm->push(vm->retainValue(inst->fieldValues[fieldIt->second]));
            return;
        }

        // Check for __get metamethod in class methods OR instance fields
        Value getMethod;
        bool hasGetMethod = false;
        bool isDynamicGet = false;

        if (inst->klass)
        {
            // First check class methods
            auto getMetaIt = inst->klass->methods.find("__get");
            if (getMetaIt != inst->klass->methods.end())
            {
                getMethod = getMetaIt->second;
                hasGetMethod = true;
            }
            else
            {
                // Also check instance fields for __get
                auto fieldIt = inst->internedFields.find("__get");
                if (fieldIt != inst->internedFields.end())
                {
                    getMethod = inst->fieldValues[fieldIt->second];
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
                // Push for closure call
                vm->push(target);   // Push instance
                vm->push(keyValue); // Push key

                if (isDynamicGet)
                {
                    // For dynamic __get (instance field), we need to set up the stack as:
                    // [...] [getMethod] [instance] [key]
                    // Currently stack is: [...] [instance] [key]

                    // Pop arguments temporarily
                    Value key = vm->pop();  // key
                    Value inst = vm->pop(); // instance

                    // Push in correct order: getMethod, instance, key
                    vm->push(vm->retainValue(getMethod));
                    vm->push(inst);
                    vm->push(key);

                    // Dynamic __get: call with 2 args [instance, key]
                    if (not vm->call(vm->asClosure(getMethod), 2))
                    {
                        vm->releaseAndDelete(getMethod);
                        vm->releaseAndDelete(keyValue);
                        vm->releaseAndDelete(index);
                        vm->releaseAndDelete(target);
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }
                }
                else
                {
                    // Class method __get: call as bound method with 1 arg [key]
                    if (not vm->callBound(vm->asClosure(getMethod), 1))
                    {
                        vm->releaseAndDelete(keyValue);
                        vm->releaseAndDelete(index);
                        vm->releaseAndDelete(target);
                        vm->runtimeError("Cannot call __get metamethod.");
                        vm->vm_return(InterpretResult::RUNTIME_ERROR);
                        return;
                    }
                }

                // Clean up
                vm->releaseAndDelete(keyValue);
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(target);

                // Frame is set up
                vm->currentFrame = &vm->frames[vm->frameCount - 1];
                vm->updateChunkCache();
                return;
            }
            else if (IS_NAT_FUNC(getMethod))
            {
                auto* native = vm->asNativeFunc(getMethod);

                // Native functions always get explicit args: [instance, key]
                vm->push(target);   // Push instance
                vm->push(keyValue); // Push key as string

                Value result = native->function(vm, 2, vm->stack.data() + vm->stack.size() - 2);

                // Clean up stack
                vm->pop(); // keyValue
                vm->pop(); // target
                vm->releaseAndDelete(keyValue);
                vm->releaseAndDelete(index);
                vm->releaseAndDelete(target);

                vm->push(result);
                return;
            }
        }

        // Property not found and no __get metamethod
        vm->releaseAndDelete(index);
        vm->releaseAndDelete(target);
        vm->push(BOOL_VAL(false));  // Or NIL_VAL if you have it
    }

    void op_set_index(VM* vm)
    {
        Value value = vm->pop();
        Value index = vm->pop();
        Value target = vm->peek(0); // Keep target on stack

        if (!IS_INSTANCE(target) && !IS_STRING(target) && !IS_VECTOR(target))
        {
            vm->releaseAndDelete(value);
            vm->releaseAndDelete(index);
            vm->runtimeError("Can only index vectors, tables/instances, or strings");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Handle vector indexing
        if (IS_VECTOR(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Vector index must be an integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            ObjVector* vec = vm->asVector(target);
            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(vec->fields.size()) + idx;
            }

            // Allow setting at the very end to push back
            if (idx == static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(index);
                vec->fields.push_back(vm->retainValue(value));
                vm->releaseAndDelete(value);
                return;
            }

            if (idx < 0 || idx > static_cast<int>(vec->fields.size()))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Vector index out of bounds");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            vm->releaseAndDelete(index);

            // Release old value at this index
            vm->releaseAndDelete(vec->fields[idx]);

            // Store new value
            vec->fields[idx] = vm->retainValue(value);
            vm->releaseAndDelete(value);  // Release our reference (vector now owns it)
            return;
        }

        // Handle instance/table indexing (most common case)
        if (IS_INSTANCE(target))
        {
            ObjInstance* inst = vm->asInstance(target);

            // Convert index to string key
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
                return;
            }

            vm->releaseAndDelete(index);

            // IMPORTANT: Fields starting with '__' (double underscore) are treated as "internal"
            // and bypass metamethods. This prevents infinite recursion when metamethods like
            // __set need to actually store data in internal fields.
            bool isInternalField = (key.length() >= 2 and key[0] == '_' and key[1] == '_');

            // Check for __set metamethod in class methods OR instance fields
            Value setMethod;
            bool hasSetMethod = false;
            bool isDynamicSet = false;

            // Only check for metamethods if this is NOT an internal field
            if (not isInternalField and inst->klass)
            {
                // First check class methods
                auto setMetaIt = inst->klass->methods.find("__set");
                if (setMetaIt != inst->klass->methods.end())
                {
                    setMethod = setMetaIt->second;
                    hasSetMethod = true;
                }
                else
                {
                    // Also check instance fields for __set
                    if (inst->hasField("__set"))
                    {
                        setMethod = inst->getField("__set");
                        hasSetMethod = true;
                        isDynamicSet = true;
                    }
                }
            }

            if (hasSetMethod and (IS_CLOSURE(setMethod) or IS_NAT_FUNC(setMethod)))
            {
                Value keyValue = vm->createString(key);

                if (IS_CLOSURE(setMethod))
                {
                    // Push for closure call
                    vm->push(target);    // Push instance
                    vm->push(keyValue);  // Push key
                    vm->push(value);     // Push value

                    if (isDynamicSet)
                    {
                        // For dynamic __set (instance field), we need to set up the stack as:
                        // [...] [setMethod] [instance] [key] [value]
                        // Currently stack is: [...] [instance] [key] [value]

                        // Pop arguments temporarily
                        Value val = vm->pop();  // value
                        Value key = vm->pop();  // key
                        Value inst = vm->pop(); // instance

                        // Push in correct order: setMethod, instance, key, value
                        vm->push(vm->retainValue(setMethod));
                        vm->push(inst);
                        vm->push(key);
                        vm->push(val);

                        // Dynamic __set: call with 3 args [instance, key, value]
                        if (not vm->call(vm->asClosure(setMethod), 3))
                        {
                            vm->releaseAndDelete(setMethod);
                            vm->releaseAndDelete(keyValue);
                            vm->releaseAndDelete(value);
                            vm->runtimeError("Cannot call __set metamethod.");
                            vm->vm_return(InterpretResult::RUNTIME_ERROR);
                            return;
                        }
                    }
                    else
                    {
                        // Class method __set: call as bound method with 2 args [key, value]
                        if (not vm->callBound(vm->asClosure(setMethod), 2))
                        {
                            vm->releaseAndDelete(keyValue);
                            vm->releaseAndDelete(value);
                            vm->runtimeError("Cannot call __set metamethod.");
                            vm->vm_return(InterpretResult::RUNTIME_ERROR);
                            return;
                        }
                    }

                    // Clean up
                    vm->releaseAndDelete(keyValue);
                    vm->releaseAndDelete(value);

                    // Frame is set up
                    vm->currentFrame = &vm->frames[vm->frameCount - 1];
                    vm->updateChunkCache();
                    return;
                }
                else if (IS_NAT_FUNC(setMethod))
                {
                    auto* native = vm->asNativeFunc(setMethod);

                    // Native functions always get explicit args: [instance, key, value]
                    vm->push(target);    // Push instance
                    vm->push(keyValue);  // Push key as string
                    vm->push(value);     // Push value

                    native->function(vm, 3, vm->stack.data() + vm->stack.size() - 3);

                    // Clean up stack
                    vm->pop(); // value
                    vm->pop(); // keyValue
                    vm->pop(); // target
                    vm->releaseAndDelete(keyValue);
                    vm->releaseAndDelete(value);

                    // Note: target stays on stack (peek(0)) as per op_set_index contract
                    return;
                }
            }

            // No __set metamethod, do normal field assignment
            auto fieldIt = inst->internedFields.find(key);
            if (fieldIt != inst->internedFields.end())
            {
                // Update existing field
                size_t valueIndex = fieldIt->second;
                vm->releaseAndDelete(inst->fieldValues[valueIndex]);
                inst->fieldValues[valueIndex] = vm->retainValue(value);
            }
            else
            {
                // New field - add to storage
                size_t newIndex = inst->fieldValues.size();
                inst->fieldValues.push_back(vm->retainValue(value));
                inst->internedFields[key] = newIndex;
            }

            vm->releaseAndDelete(value);  // Release our reference (field now owns it)
            return;
        }

        // Handle string indexing
        if (IS_STRING(target))
        {
            if (!IS_INT(index))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("String index must be integer");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            if (!IS_STRING(value))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Can only assign string to string index");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            std::string str = vm->asString(target);
            int idx = AS_INT(index);

            // Handle negative indices (Python-style)
            if (idx < 0)
            {
                idx = static_cast<int>(str.length()) + idx;
            }

            // Allow appending at the end (idx == str.length())
            if (idx < 0 || idx > static_cast<int>(str.length()))
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("String index out of range");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            std::string valueString = vm->asString(value);

            // Can only set a single character
            if (valueString.length() != 1)
            {
                vm->releaseAndDelete(value);
                vm->releaseAndDelete(index);
                vm->runtimeError("Can only assign single character to string index");
                vm->vm_return(InterpretResult::RUNTIME_ERROR);
                return;
            }

            // If appending at the end, append the character
            if (idx == static_cast<int>(str.length()))
            {
                str += valueString[0];
            }
            else
            {
                // Modify the string at the index
                str[idx] = valueString[0];
            }

            // Create a new string with the modified content and replace on stack
            vm->pop(); // Remove old string
            vm->push(vm->createString(str));

            vm->releaseAndDelete(value);
            vm->releaseAndDelete(index);
            return;
        }
    }

    // ========================================================================
    // Iterator Operations
    // ========================================================================

    void op_get_iterator(VM* vm)
    {
        Value tableVal = vm->peek(0);  // Don't pop, we keep table on stack

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only iterate over tables");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        // Create an integer to track iteration index (we'll iterate by index in fields map)
        // Push 0 as the initial iterator state
        vm->push(makeIntValue(0));
    }

    void op_iterator_next(VM* vm)
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
            return;
        }

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only iterate over tables");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
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
            return;
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
    }

    void op_table_size(VM* vm)
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
            return;
        }

        if (!IS_INSTANCE(tableVal))
        {
            vm->runtimeError("Can only get size of tables or vectors");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        ObjInstance* table = vm->asInstance(tableVal);

        // Pop the table
        vm->pop();
        vm->releaseAndDelete(tableVal);

        // Push the field count
        size_t totalSize = table->fieldValues.size();
        vm->push(makeIntValue(static_cast<int64_t>(totalSize)));
    }

    void op_table_at(VM* vm)
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
                return;
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
                return;
            }

            // Get the value directly
            Value value = vector->fields[index];

            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);

            // For vectors, return the value directly (not the index)
            // This is different from tables where we return the key
            vm->push(vm->retainValue(value));
            return;
        }

        // Handle table access (returns key at index position in map)
        if (!IS_INSTANCE(tableVal))
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Can only get keys from tables or vectors");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
        }

        if (!IS_INT(indexVal))
        {
            vm->releaseAndDelete(indexVal);
            vm->releaseAndDelete(tableVal);
            vm->runtimeError("Table index must be an integer");
            vm->vm_return(InterpretResult::RUNTIME_ERROR);
            return;
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
            return;
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
    }
}