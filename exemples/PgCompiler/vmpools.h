#pragma once

/**
 * @file vmpools.h
 * @brief Pool-based memory management for VM heap objects
 * @version 1.0
 *
 * This header provides segregated pool allocators for all VM heap types.
 * Each type has its own pool for better cache locality and reduced fragmentation.
 *
 * Benefits:
 * - O(1) allocation and deallocation
 * - Better cache locality (objects of same type are contiguous)
 * - Reduced heap fragmentation
 * - Fast reference counting using vector indices
 */

#include "value_nanbox.h"
#include "object.h"
#include "../../src/Engine/Memory/memorypool.h"
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace pg
{
    /**
     * @brief Pool manager for all VM heap objects
     *
     * Provides type-segregated pools for efficient allocation and deallocation.
     * Each pool type has its own AllocatorPool and reference count vector.
     */
    class VMPools
    {
    public:
        // ====================================================================
        // Type-Segregated Pools
        // ====================================================================

        /** Pool for string objects (ElementType) */
        AllocatorPool<ElementType, 64> stringPool;

        /** Pool for closure objects */
        AllocatorPool<Closure, 32> closurePool;

        /** Pool for function objects */
        AllocatorPool<ObjFunction, 16> functionPool;

        /** Pool for upvalue objects */
        AllocatorPool<ObjUpvalue, 32> upvaluePool;

        /** Pool for class objects */
        AllocatorPool<Klass, 16> classPool;

        /** Pool for native function wrappers */
        AllocatorPool<NativeFunction, 16> nativeFuncPool;

        /** Pool for instance objects */
        AllocatorPool<ObjInstance, 32> instancePool;

        /** Pool for bound method objects */
        AllocatorPool<ObjBoundMethod, 32> boundMethodPool;

        // ====================================================================
        // Reference Count Vectors
        // ====================================================================

        /** Reference counts for string pool */
        std::vector<uint32_t> stringRefCounts;

        /** Reference counts for closure pool */
        std::vector<uint32_t> closureRefCounts;

        /** Reference counts for function pool */
        std::vector<uint32_t> functionRefCounts;

        /** Reference counts for upvalue pool */
        std::vector<uint32_t> upvalueRefCounts;

        /** Reference counts for class pool */
        std::vector<uint32_t> classRefCounts;

        /** Reference counts for native function pool */
        std::vector<uint32_t> nativeFuncRefCounts;

        /** Reference counts for instance pool */
        std::vector<uint32_t> instanceRefCounts;

        /** Reference counts for bound method pool */
        std::vector<uint32_t> boundMethodRefCounts;

        // ====================================================================
        // Public API
        // ====================================================================

        /**
         * @brief Pre-allocate pools to expected sizes
         *
         * Call this during VM initialization to avoid runtime allocations
         *
         * @param stringCount Expected number of strings
         * @param closureCount Expected number of closures
         * @param functionCount Expected number of functions
         * @param upvalueCount Expected number of upvalues
         * @param classCount Expected number of classes
         * @param instanceCount Expected number of instances
         */
        void reserve(size_t stringCount = 1024,
                     size_t closureCount = 64,
                     size_t functionCount = 32,
                     size_t upvalueCount = 64,
                     size_t classCount = 16,
                     size_t instanceCount = 128)
        {
            // Reserve pool capacity
            stringPool.reserve(stringCount);
            closurePool.reserve(closureCount);
            functionPool.reserve(functionCount);
            upvaluePool.reserve(upvalueCount);
            classPool.reserve(classCount);
            nativeFuncPool.reserve(functionCount);
            instancePool.reserve(instanceCount);
            boundMethodPool.reserve(instanceCount);

            // Reserve refcount vectors
            stringRefCounts.reserve(stringCount);
            closureRefCounts.reserve(closureCount);
            functionRefCounts.reserve(functionCount);
            upvalueRefCounts.reserve(upvalueCount);
            classRefCounts.reserve(classCount);
            nativeFuncRefCounts.reserve(functionCount);
            instanceRefCounts.reserve(instanceCount);
            boundMethodRefCounts.reserve(instanceCount);
        }

        /**
         * @brief Get reference count vector by value tag
         *
         * @param v The value to get refcount vector for
         * @return Reference to the appropriate refcount vector
         */
        std::vector<uint32_t>& getRefCountVector(Value v)
        {
            if (IS_STRING(v)) return stringRefCounts;
            if (IS_CLOSURE(v)) return closureRefCounts;
            if (IS_FUNC(v)) return functionRefCounts;
            if (IS_UPVALUE(v)) return upvalueRefCounts;
            if (IS_CLASS(v)) return classRefCounts;
            if (IS_NAT_FUNC(v)) return nativeFuncRefCounts;
            if (IS_INSTANCE(v)) return instanceRefCounts;
            if (IS_BOUND_METHOD(v)) return boundMethodRefCounts;

            throw std::runtime_error("Invalid value type for refcount");
        }

        /**
         * @brief Ensure refcount vector is large enough for index
         *
         * @param v The value to check
         * @param index The index that needs to exist
         */
        inline void ensureRefCountCapacity(Value v, uint32_t index)
        {
            auto& refCounts = getRefCountVector(v);
            if (index >= refCounts.size()) {
                refCounts.resize(index + 1, 0);
            }
        }

        // ====================================================================
        // Pool Object Access
        // ====================================================================

        /**
         * @brief Get string object from pool
         */
        inline ElementType* getString(Value v)
        {
            return stringPool.getElement(AS_STRING_INDEX(v));
        }

        /**
         * @brief Get closure object from pool
         */
        inline Closure* getClosure(Value v)
        {
            return closurePool.getElement(AS_CLOSURE_INDEX(v));
        }

        /**
         * @brief Get function object from pool
         */
        inline ObjFunction* getFunction(Value v)
        {
            return functionPool.getElement(AS_FUNCTION_INDEX(v));
        }

        /**
         * @brief Get upvalue object from pool
         */
        inline ObjUpvalue* getUpvalue(Value v)
        {
            return upvaluePool.getElement(AS_UPVALUE_INDEX(v));
        }

        /**
         * @brief Get class object from pool
         */
        inline Klass* getClass(Value v)
        {
            return classPool.getElement(AS_CLASS_INDEX(v));
        }

        /**
         * @brief Get native function object from pool
         */
        inline NativeFunction* getNativeFunc(Value v)
        {
            return nativeFuncPool.getElement(AS_NATIVE_INDEX(v));
        }

        /**
         * @brief Get instance object from pool
         */
        inline ObjInstance* getInstance(Value v)
        {
            return instancePool.getElement(AS_INSTANCE_INDEX(v));
        }

        /**
         * @brief Get bound method object from pool
         */
        inline ObjBoundMethod* getBoundMethod(Value v)
        {
            return boundMethodPool.getElement(AS_BOUND_METHOD_INDEX(v));
        }

        // ====================================================================
        // Generic Template Access (for advanced usage)
        // ====================================================================

        /**
         * @brief Template-based pool object access
         *
         * Usage: pools.getPoolObject<ElementType>(stringValue)
         */
        template<typename T>
        T* getPoolObject(Value v);

        /**
         * @brief Release object back to pool
         *
         * Called when refcount reaches zero
         */
        void releaseToPool(Value v)
        {
            if (IS_STRING(v)) {
                stringPool.release(getString(v));
            } else if (IS_CLOSURE(v)) {
                closurePool.release(getClosure(v));
            } else if (IS_FUNC(v)) {
                functionPool.release(getFunction(v));
            } else if (IS_UPVALUE(v)) {
                upvaluePool.release(getUpvalue(v));
            } else if (IS_CLASS(v)) {
                classPool.release(getClass(v));
            } else if (IS_NAT_FUNC(v)) {
                nativeFuncPool.release(getNativeFunc(v));
            } else if (IS_INSTANCE(v)) {
                instancePool.release(getInstance(v));
            } else if (IS_BOUND_METHOD(v)) {
                boundMethodPool.release(getBoundMethod(v));
            }
        }

        // ====================================================================
        // Statistics (for debugging and profiling)
        // ====================================================================

        /**
         * @brief Print pool statistics
         */
        void printStats() const
        {
            LOG_INFO("VMPools", "=== Pool Statistics ===");
            LOG_INFO("VMPools", "Strings:      " << stringPool.getNbElements() << " / " << stringPool.getSize());
            LOG_INFO("VMPools", "Closures:     " << closurePool.getNbElements() << " / " << closurePool.getSize());
            LOG_INFO("VMPools", "Functions:    " << functionPool.getNbElements() << " / " << functionPool.getSize());
            LOG_INFO("VMPools", "Upvalues:     " << upvaluePool.getNbElements() << " / " << upvaluePool.getSize());
            LOG_INFO("VMPools", "Classes:      " << classPool.getNbElements() << " / " << classPool.getSize());
            LOG_INFO("VMPools", "NativeFuncs:  " << nativeFuncPool.getNbElements() << " / " << nativeFuncPool.getSize());
            LOG_INFO("VMPools", "Instances:    " << instancePool.getNbElements() << " / " << instancePool.getSize());
            LOG_INFO("VMPools", "BoundMethods: " << boundMethodPool.getNbElements() << " / " << boundMethodPool.getSize());
        }
    };

    // ====================================================================
    // Template Specializations
    // ====================================================================

    template<>
    inline ElementType* VMPools::getPoolObject<ElementType>(Value v)
    {
        return getString(v);
    }

    template<>
    inline Closure* VMPools::getPoolObject<Closure>(Value v)
    {
        return getClosure(v);
    }

    template<>
    inline ObjFunction* VMPools::getPoolObject<ObjFunction>(Value v)
    {
        return getFunction(v);
    }

    template<>
    inline ObjUpvalue* VMPools::getPoolObject<ObjUpvalue>(Value v)
    {
        return getUpvalue(v);
    }

    template<>
    inline Klass* VMPools::getPoolObject<Klass>(Value v)
    {
        return getClass(v);
    }

    template<>
    inline NativeFunction* VMPools::getPoolObject<NativeFunction>(Value v)
    {
        return getNativeFunc(v);
    }

    template<>
    inline ObjInstance* VMPools::getPoolObject<ObjInstance>(Value v)
    {
        return getInstance(v);
    }

    template<>
    inline ObjBoundMethod* VMPools::getPoolObject<ObjBoundMethod>(Value v)
    {
        return getBoundMethod(v);
    }
}
