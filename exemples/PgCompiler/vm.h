#pragma once

#include "chunk.h"

#include "compiler.h"

#include "Interpreter/lexer.h"

#include "logger.h"

#include "bytecode_pass.h"

#include <stack>
#include <functional>

// Todo add this as a flag in when compiling in debug
// #define DEBUG_TRACE_EXECUTION

// #define DEBUG_CHECK_STACK

#define DEBUG_PROFILE_COMPILE

#define EMIT_RUNTIME_ERROR(msg) do {runtimeError((Strfy() << msg).getData()); return InterpretResult::RUNTIME_ERROR;} while(0);

namespace pg
{
    enum class InterpretResult
    {
        OK,
        COMPILE_ERROR,
        RUNTIME_ERROR
    };

    /**
     * @brief Tagged union value representation following Crafting Interpreters pattern
     *
     * Stores small values (int, bool) directly in the union for zero-allocation performance.
     * Complex values (strings, floats) use pointer overflow to ElementType on the heap.
     *
     * Memory layout: 16 bytes total (8-byte union + 4-byte type + 4-byte padding)
     */
    typedef enum {
        VAL_BOOL,
        VAL_INT,
        VAL_OBJ     // Pointer overflow for complex values (strings, floats, etc.)
    } ValueType;

    typedef struct {
        ValueType type;
        union {
            bool boolean;
            int64_t number;     // Use int64_t for wider range than int
            ElementType* obj;   // Heap-allocated for strings, floats, size_t, etc.
        } as;
    } Value;

    // Fast type checking macros (compile-time constant)
    #define IS_BOOL(value)    ((value).type == VAL_BOOL)
    #define IS_INT(value)     ((value).type == VAL_INT)
    #define IS_OBJ(value)     ((value).type == VAL_OBJ)

    // Fast value extraction macros (direct memory access)
    #define AS_BOOL(value)    ((value).as.boolean)
    #define AS_INT(value)     ((value).as.number)
    #define AS_OBJ(value)     ((value).as.obj)

    // Fast value creation functions (C++ compatible)
    inline Value makeBoolValue(bool value) {
        Value result;
        result.type = VAL_BOOL;
        result.as.boolean = value;
        return result;
    }

    inline Value makeIntValue(int64_t value) {
        Value result;
        result.type = VAL_INT;
        result.as.number = value;
        return result;
    }

    inline Value makeObjValue(ElementType* obj) {
        Value result;
        result.type = VAL_OBJ;
        result.as.obj = obj;
        return result;
    }

    // Convenience macros
    #define BOOL_VAL(value)   makeBoolValue(value)
    #define INT_VAL(value)    makeIntValue(value)
    #define OBJ_VAL(object)   makeObjValue(object)

    // Convert ElementType to optimized Value (minimize heap allocation)
    inline Value elementToValue(const ElementType& element) {
        if (element.isBool()) {
            return BOOL_VAL(element.get<bool>());
        } else if (element.type == ElementType::UnionType::INT) {
            int intVal = element.get<int>();
            return INT_VAL(static_cast<int64_t>(intVal));
        } else {
            // Strings, floats, complex types always use heap
            return OBJ_VAL(new ElementType(element));
        }
    }

    inline int getValueAsInt(const Value& value) {
        if (IS_INT(value)) {
            return static_cast<int>(AS_INT(value));
        } else if (IS_BOOL(value)) {
            return AS_BOOL(value) ? 1 : 0;
        } else if (IS_OBJ(value) && AS_OBJ(value)->type == ElementType::UnionType::INT) {
            return AS_OBJ(value)->get<int>();
        }
        throw std::runtime_error("Value is not an integer");
    }

    // Convert Value back to ElementType when needed
    inline ElementType valueToElement(const Value& value)
    {
        switch (value.type) {
            case VAL_BOOL: return ElementType(AS_BOOL(value));
            case VAL_INT:  return ElementType(static_cast<int>(AS_INT(value)));
            case VAL_OBJ:  return *AS_OBJ(value);
        }

        return ElementType(); // Should never reach
    }

    // Fast arithmetic operations on Value types
    inline Value addValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return INT_VAL(AS_INT(a) + AS_INT(b));
        }
        // Fall back to ElementType for complex cases
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA + elemB);
    }

    inline Value subtractValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return INT_VAL(AS_INT(a) - AS_INT(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA - elemB);
    }

    inline Value multiplyValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return INT_VAL(AS_INT(a) * AS_INT(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA * elemB);
    }

    inline Value divideValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b) && AS_INT(b) != 0) {
            return INT_VAL(AS_INT(a) / AS_INT(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA / elemB);
    }

    inline Value negateValue(const Value& val) {
        if (IS_INT(val)) {
            return INT_VAL(-AS_INT(val));
        }
        ElementType elem = valueToElement(val);
        return elementToValue(-elem);
    }

    inline bool isValueNumber(const Value& val) {
        if (IS_INT(val)) return true;
        if (IS_OBJ(val)) {
            return AS_OBJ(val)->isNumber();
        }
        return false;
    }

    inline bool isValueTrue(const Value& val) {
        if (IS_BOOL(val)) return AS_BOOL(val);
        if (IS_INT(val)) return AS_INT(val) != 0;
        if (IS_OBJ(val)) {
            return AS_OBJ(val)->isTrue();
        }
        return false;
    }

    // Fast comparison operations
    inline Value equalsValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return BOOL_VAL(AS_INT(a) == AS_INT(b));
        }
        if (IS_BOOL(a) && IS_BOOL(b)) {
            return BOOL_VAL(AS_BOOL(a) == AS_BOOL(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA == elemB);
    }

    inline Value notEqualsValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return BOOL_VAL(AS_INT(a) != AS_INT(b));
        }
        if (IS_BOOL(a) && IS_BOOL(b)) {
            return BOOL_VAL(AS_BOOL(a) != AS_BOOL(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA != elemB);
    }

    inline Value greaterValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return BOOL_VAL(AS_INT(a) > AS_INT(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA > elemB);
    }

    inline Value greaterEqualValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return BOOL_VAL(AS_INT(a) >= AS_INT(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA >= elemB);
    }

    inline Value lessValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return BOOL_VAL(AS_INT(a) < AS_INT(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA < elemB);
    }

    inline Value lessEqualValues(const Value& a, const Value& b) {
        if (IS_INT(a) && IS_INT(b)) {
            return BOOL_VAL(AS_INT(a) <= AS_INT(b));
        }
        ElementType elemA = valueToElement(a);
        ElementType elemB = valueToElement(b);
        return elementToValue(elemA <= elemB);
    }

    // Memory management for heap-allocated objects
    inline void freeValue(Value& value) {
        if (IS_OBJ(value)) {
            delete AS_OBJ(value);
            value.as.obj = nullptr;
        }
    }

    class IndexableStack
    {
    private:
        static constexpr size_t MAX_STACK_SIZE = 16384; // 16K elements max
        alignas(Value) char stack_memory[MAX_STACK_SIZE * sizeof(Value)];
        size_t stack_top = 0;

        Value* stack_data() {
            return reinterpret_cast<Value*>(stack_memory);
        }

        const Value* stack_data() const {
            return reinterpret_cast<const Value*>(stack_memory);
        }

    public:
        // Direct Value operations (fast path)
        void push(const Value& value) {
            if (stack_top >= MAX_STACK_SIZE)
                throw std::runtime_error("Stack overflow");
            stack_data()[stack_top++] = value;  // Simple assignment, no constructor
        }

        // Legacy ElementType support (converts to Value)
        void push(const ElementType& element) {
            push(elementToValue(element));
        }

        void push(ElementType&& element) {
            push(elementToValue(element));
        }

        Value pop() {
            if (stack_top == 0)
                throw std::runtime_error("Trying to pop on an empty stack");
            Value value = stack_data()[stack_top - 1];
            stack_top--;  // No destructor needed for POD-like Value
            return value;
        }

        // For legacy compatibility - returns ElementType
        ElementType popElement() {
            Value val = pop();
            ElementType result = valueToElement(val);
            freeValue(val);  // Clean up any heap allocation
            return result;
        }

        Value& operator[](size_t index) { return stack_data()[index]; }
        const Value& operator[](size_t index) const { return stack_data()[index]; }

        Value top() const {
            if (stack_top == 0)
                throw std::runtime_error("Stack is empty");
            return stack_data()[stack_top - 1];
        }

        // For legacy compatibility
        ElementType topElement() const {
            return valueToElement(top());
        }

        bool empty() const { return stack_top == 0; }
        size_t size() const { return stack_top; }

        void clear() {
            // Clean up any heap-allocated objects
            while (stack_top > 0) {
                freeValue(stack_data()[--stack_top]);
            }
        }
    };

    struct VM
    {
        InterpretResult interpretFromText(const std::string& source)
        {
            Lexer lexer;

            try
            {
                lexer.readFromText(source);
            }
            catch(const std::exception& e)
            {
                LOG_ERROR("VM", e.what());
                return InterpretResult::COMPILE_ERROR;
            }

            auto tokens = lexer.getTokens();

            return interpret(tokens);
        }

        InterpretResult interpret(const std::queue<Token>& tokens);

        InterpretResult run();

        Value readConstant();
        Value readLongConstant();

        void binaryOp(std::function<Value(Value, Value)> op);
        void fastBinaryOp(Value (*op)(const Value&, const Value&));

        // Core Value operations for performance
        inline void push(const Value& value)
        {
            stack.push(value);
        }

        inline void push(const ElementType& value)
        {
            stack.push(value);  // Automatically converts to Value
        }

        Value pop()
        {
#ifdef DEBUG_CHECK_STACK
            if (stack.empty())
                throw std::runtime_error("Trying to pop on an empty stack");
#endif
            return stack.pop();
        }

        Value peek(size_t distance = 0) const
        {
#ifdef DEBUG_CHECK_STACK
            if (distance >= stack.size())
                throw std::runtime_error("Trying to peek too far in the stack");
#endif
            return stack[stack.size() - 1 - distance];
        }

        uint16_t readUint16()
        {
#ifdef DEBUG_CHECK_STACK
            if (ip + 1 >= chunk.code.size())
            {
                throw std::runtime_error("Not enough bytes to read uint16.");
            }
#endif
            uint16_t value = (static_cast<uint16_t>(chunk.code[ip]) << 8);
            ip++;
            value |= static_cast<uint16_t>(chunk.code[ip]);
            ip++;

            return value;
        }

        uint32_t readUint32()
        {
#ifdef DEBUG_CHECK_STACK
            if (ip + 3 >= chunk.code.size())
            {
                throw std::runtime_error("Not enough bytes to read uint32.");
            }
#endif
            uint32_t value = (static_cast<uint32_t>(chunk.code[ip]) << 24);
            ip++;
            value |= (static_cast<uint32_t>(chunk.code[ip]) << 16);
            ip++;
            value |= (static_cast<uint32_t>(chunk.code[ip]) << 8);
            ip++;
            value |= static_cast<uint32_t>(chunk.code[ip]);
            ip++;

            return value;
        }

        inline void resetStack()
        {
            stack.clear();
        }

        void runtimeError(const std::string& message)
        {
            LOG_ERROR("VM", "[line " << chunk.lines[ip - 1] << "] in script");
            LOG_ERROR("VM", message);
            resetStack();
        }

        bool checkBooleanBinaryOp()
        {
#ifdef DEBUG_CHECK_STACK
            if (stack.size() < 2)
            {
                runtimeError("Stack underflow on binary operation.");
                return false;
            }
#endif
            return true;
        }

        Compiler compiler;

        /* The chunk being interpreted */
        Chunk chunk;

        /* Instruction pointer */
        size_t ip = 0;

        /* The stack of the VM */
        IndexableStack stack;

        std::unordered_map<std::string, Value> globals;

        // Test output buffer for __dprint (used in tests)
        std::string testOutput;

        // Bytecode optimization
        PassManager passManager;
        bool enableOptimizations = true;

        // Optimization control methods
        void enableBytecodeOptimization() {
            enableOptimizations = true;
            LOG_INFO("VM", "Bytecode optimization enabled");
        }

        void disableBytecodeOptimization() {
            enableOptimizations = false;
            LOG_INFO("VM", "Bytecode optimization disabled");
        }

        void enableOptimizationDebugging() {
            passManager.setDebugOutput(true);
        }

        void disableOptimizationDebugging() {
            passManager.setDebugOutput(false);
        }

        void listOptimizationPasses() const {
            passManager.listPasses();
        }

        void addOptimizationPass(std::unique_ptr<BytecodePass> pass) {
            passManager.addPass(std::move(pass));
        }
    };

}