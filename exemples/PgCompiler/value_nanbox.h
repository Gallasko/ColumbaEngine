#pragma once

/**
 * @file value_nanbox.h
 * @brief NaN-boxed value representation for the PgCompiler VM
 * @version 1.0
 *
 * This header implements NaN-boxing for efficient value representation.
 * All values fit in a single 64-bit integer, using IEEE 754 NaN space
 * for encoding types and pool indices.
 *
 * Encoding scheme:
 * - Doubles: Standard IEEE 754 representation
 * - Tagged values: [Sign][0x7FF][1][TAG(3)][INDEX(48)]
 *   - Sign=0: Primary types (int, bool, string, closure, function, upvalue, class, native)
 *   - Sign=1: Extended types (instance, bound_method, + 6 reserved slots)
 *
 * This provides:
 * - 8 primary type tags + 8 extended type tags = 16 total types
 * - 48-bit indices: supports 281 trillion objects per pool
 * - 48-bit signed integers: ±140 trillion range
 * - Full IEEE 754 double precision
 */

#include <cstdint>
#include <type_traits>

namespace pg
{
    // ============================================================================
    // NaN-Boxing Constants
    // ============================================================================

    /** Quiet NaN mask - identifies tagged values */
    static constexpr uint64_t QNAN_MASK = 0x7FFC000000000000ULL;

    /** Sign bit - differentiates primary vs extended tags */
    static constexpr uint64_t SIGN_BIT = 0x8000000000000000ULL;

    /** Tag bits position in mantissa [50:48] */
    static constexpr uint64_t TAG_SHIFT = 48;

    /** Mask to extract 3-bit tag */
    static constexpr uint64_t TAG_MASK = 0x0007000000000000ULL;

    /** Mask to extract 48-bit index/payload */
    static constexpr uint64_t INDEX_MASK = 0x0000FFFFFFFFFFFFULL;

    /** Positive tagged value base (QNAN without sign bit) */
    static constexpr uint64_t POS_TAG_BASE = QNAN_MASK;

    /** Negative tagged value base (QNAN with sign bit) */
    static constexpr uint64_t NEG_TAG_BASE = QNAN_MASK | SIGN_BIT;

    // ============================================================================
    // Type Tags
    // ============================================================================

    /**
     * Primary value tags (Sign bit = 0)
     * These are the most commonly used types
     */
    enum ValueTag : uint8_t {
        TAG_INT       = 0,  // 48-bit signed integer
        TAG_BOOL      = 1,  // Boolean (0 or 1)
        TAG_STRING    = 2,  // Index into string pool
        TAG_CLOSURE   = 3,  // Index into closure pool
        TAG_FUNCTION  = 4,  // Index into function pool
        TAG_UPVALUE   = 5,  // Index into upvalue pool
        TAG_CLASS     = 6,  // Index into class pool
        TAG_NATIVE    = 7,  // Index into native function pool
    };

    /**
     * Extended value tags (Sign bit = 1)
     * These are less common types or reserved for future use
     */
    enum ValueTagExt : uint8_t {
        TAG_INSTANCE      = 0,  // Index into instance pool
        TAG_BOUND_METHOD  = 1,  // Index into bound method pool
        TAG_RESERVED_2    = 2,  // Reserved for future use
        TAG_RESERVED_3    = 3,  // Reserved for future use
        TAG_RESERVED_4    = 4,  // Reserved for future use
        TAG_RESERVED_5    = 5,  // Reserved for future use
        TAG_RESERVED_6    = 6,  // Reserved for future use
        TAG_RESERVED_7    = 7,  // Reserved for future use
    };

    /**
     * Value type - just a 64-bit integer
     * Can represent doubles, integers, booleans, or pool indices
     */
    typedef uint64_t Value;

    // ============================================================================
    // Type Checking
    // ============================================================================

    /**
     * Check if value is an IEEE 754 double (not a tagged value)
     */
    inline bool IS_DOUBLE(Value v) {
        return (v & QNAN_MASK) != QNAN_MASK;
    }

    /**
     * Check if value is a tagged value (not a double)
     */
    inline bool IS_TAGGED(Value v) {
        return (v & QNAN_MASK) == QNAN_MASK;
    }

    /**
     * Check if tagged value uses positive sign (primary tags)
     */
    inline bool IS_POS_TAGGED(Value v) {
        return (v & (QNAN_MASK | SIGN_BIT)) == QNAN_MASK;
    }

    /**
     * Check if tagged value uses negative sign (extended tags)
     */
    inline bool IS_NEG_TAGGED(Value v) {
        return (v & (QNAN_MASK | SIGN_BIT)) == (QNAN_MASK | SIGN_BIT);
    }

    /**
     * Extract 3-bit tag from tagged value
     */
    inline uint8_t GET_TAG(Value v) {
        return static_cast<uint8_t>((v & TAG_MASK) >> TAG_SHIFT);
    }

    /**
     * Extract 48-bit index from tagged value
     */
    inline uint32_t GET_INDEX(Value v) {
        return static_cast<uint32_t>(v & INDEX_MASK);
    }

    // Primary type checks
    inline bool IS_INT(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_INT;
    }

    inline bool IS_BOOL(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_BOOL;
    }

    inline bool IS_STRING(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_STRING;
    }

    inline bool IS_CLOSURE(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_CLOSURE;
    }

    inline bool IS_FUNC(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_FUNCTION;
    }

    inline bool IS_UPVALUE(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_UPVALUE;
    }

    inline bool IS_CLASS(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_CLASS;
    }

    inline bool IS_NAT_FUNC(Value v) {
        return IS_POS_TAGGED(v) && GET_TAG(v) == TAG_NATIVE;
    }

    // Extended type checks
    inline bool IS_INSTANCE(Value v) {
        return IS_NEG_TAGGED(v) && GET_TAG(v) == TAG_INSTANCE;
    }

    inline bool IS_BOUND_METHOD(Value v) {
        return IS_NEG_TAGGED(v) && GET_TAG(v) == TAG_BOUND_METHOD;
    }

    // Legacy compatibility (for transition period)
    inline bool IS_OBJ(Value v) {
        return IS_STRING(v);  // OBJ was primarily used for strings
    }

    inline bool IS_FLOAT(Value v) {
        return IS_DOUBLE(v);  // FLOAT is now DOUBLE
    }

    // ============================================================================
    // Value Creation
    // ============================================================================

    /**
     * Create a double value from native double
     */
    inline Value makeDoubleValue(double d) {
        union { double d; uint64_t u; } cast;
        cast.d = d;
        return cast.u;
    }

    /**
     * Create an integer value (48-bit signed integer)
     * Range: -140,737,488,355,328 to +140,737,488,355,327
     */
    inline Value makeIntValue(int64_t i) {
        // Mask to 48 bits (preserves sign bit in bit 47)
        uint64_t index = static_cast<uint64_t>(i) & INDEX_MASK;
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_INT) << TAG_SHIFT) | index;
    }

    /**
     * Create a boolean value
     */
    inline Value makeBoolValue(bool b) {
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_BOOL) << TAG_SHIFT) |
               static_cast<uint64_t>(b);
    }

    /**
     * Create a string value (pool index)
     */
    inline Value makeStringValue(uint32_t index) {
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_STRING) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    /**
     * Create a closure value (pool index)
     */
    inline Value makeClosureValue(uint32_t index) {
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_CLOSURE) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    /**
     * Create a function value (pool index)
     */
    inline Value makeFunctionValue(uint32_t index) {
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_FUNCTION) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    /**
     * Create an upvalue (pool index)
     */
    inline Value makeUpvalueValue(uint32_t index) {
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_UPVALUE) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    /**
     * Create a class value (pool index)
     */
    inline Value makeClassValue(uint32_t index) {
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_CLASS) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    /**
     * Create a native function value (pool index)
     */
    inline Value makeNativeFuncValue(uint32_t index) {
        return POS_TAG_BASE | (static_cast<uint64_t>(TAG_NATIVE) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    /**
     * Create an instance value (pool index) - uses negative tag
     */
    inline Value makeInstanceValue(uint32_t index) {
        return NEG_TAG_BASE | (static_cast<uint64_t>(TAG_INSTANCE) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    /**
     * Create a bound method value (pool index) - uses negative tag
     */
    inline Value makeBoundMethodValue(uint32_t index) {
        return NEG_TAG_BASE | (static_cast<uint64_t>(TAG_BOUND_METHOD) << TAG_SHIFT) |
               static_cast<uint64_t>(index);
    }

    // Legacy compatibility
    inline Value makeObjValue(uint32_t index) {
        return makeStringValue(index);
    }

    inline Value makeFloatValue(double d) {
        return makeDoubleValue(d);
    }

    // ============================================================================
    // Value Extraction
    // ============================================================================

    /**
     * Extract double from value
     */
    inline double AS_DOUBLE(Value v) {
        union { uint64_t u; double d; } cast;
        cast.u = v;
        return cast.d;
    }

    /**
     * Extract 48-bit signed integer from value
     * Properly handles sign extension
     */
    inline int64_t AS_INT(Value v) {
        // Extract 48-bit value
        int64_t i = static_cast<int64_t>(v & INDEX_MASK);

        // Sign extend from bit 47 to 64 bits
        if (i & 0x800000000000LL) {
            i |= 0xFFFF000000000000LL;
        }

        return i;
    }

    /**
     * Extract boolean from value
     */
    inline bool AS_BOOL(Value v) {
        return static_cast<bool>(v & 1);
    }

    /**
     * Extract pool index for string
     */
    inline uint32_t AS_STRING_INDEX(Value v) {
        return GET_INDEX(v);
    }

    /**
     * Extract pool index for closure
     */
    inline uint32_t AS_CLOSURE_INDEX(Value v) {
        return GET_INDEX(v);
    }

    /**
     * Extract pool index for function
     */
    inline uint32_t AS_FUNCTION_INDEX(Value v) {
        return GET_INDEX(v);
    }

    /**
     * Extract pool index for upvalue
     */
    inline uint32_t AS_UPVALUE_INDEX(Value v) {
        return GET_INDEX(v);
    }

    /**
     * Extract pool index for class
     */
    inline uint32_t AS_CLASS_INDEX(Value v) {
        return GET_INDEX(v);
    }

    /**
     * Extract pool index for native function
     */
    inline uint32_t AS_NATIVE_INDEX(Value v) {
        return GET_INDEX(v);
    }

    /**
     * Extract pool index for instance
     */
    inline uint32_t AS_INSTANCE_INDEX(Value v) {
        return GET_INDEX(v);
    }

    /**
     * Extract pool index for bound method
     */
    inline uint32_t AS_BOUND_METHOD_INDEX(Value v) {
        return GET_INDEX(v);
    }

    // Legacy compatibility - these will need to be updated to work with pools
    inline uint32_t AS_OBJ_INDEX(Value v) {
        return AS_STRING_INDEX(v);
    }

    inline double AS_FLOAT(Value v) {
        return AS_DOUBLE(v);
    }

    // ============================================================================
    // Utility Functions
    // ============================================================================

    /**
     * Check if value requires pool-based reference counting
     */
    inline bool requiresRefCount(Value v) {
        if (!IS_TAGGED(v)) return false;  // Doubles don't need refcount
        if (IS_INT(v) || IS_BOOL(v)) return false;  // Primitives don't need refcount
        return true;  // All pool-based values need refcount
    }

    /**
     * Get a string representation of the value type (for debugging)
     */
    inline const char* valueTypeName(Value v) {
        if (IS_DOUBLE(v)) return "double";
        if (IS_INT(v)) return "int";
        if (IS_BOOL(v)) return "bool";
        if (IS_STRING(v)) return "string";
        if (IS_CLOSURE(v)) return "closure";
        if (IS_FUNC(v)) return "function";
        if (IS_UPVALUE(v)) return "upvalue";
        if (IS_CLASS(v)) return "class";
        if (IS_NAT_FUNC(v)) return "native";
        if (IS_INSTANCE(v)) return "instance";
        if (IS_BOUND_METHOD(v)) return "bound_method";
        return "unknown";
    }
}
