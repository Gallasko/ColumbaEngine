#pragma once

/**
 * @file memorypool.h
 * @author Pigeon Codeur
 * @brief Definition of the memory pool
 * @version 0.1
 * @date 2022-05-28
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <type_traits>
#include <vector>
#include <memory>
#include <mutex>
#include <cmath>
#include <atomic>
#include <set>

#include "logger.h"
namespace pg
{
    /** Static index table for log2 of a 64bits integer */
    static constexpr uint64_t tab64[64] = {
        63,  0, 58,  1, 59, 47, 53,  2,
        60, 39, 48, 27, 54, 33, 42,  3,
        61, 51, 37, 40, 49, 18, 28, 20,
        55, 30, 34, 11, 43, 14, 22,  4,
        62, 57, 46, 52, 38, 26, 32, 41,
        50, 36, 17, 19, 29, 10, 13, 21,
        56, 45, 25, 31, 35, 16,  9, 12,
        44, 24, 15,  8, 23,  7,  6,  5};

    /** Static lookup of the log2 of a 64bits integer */
    static constexpr uint64_t log2_64 (uint64_t value)
    {
        value |= value >> 1UL;
        value |= value >> 2UL;
        value |= value >> 4UL;
        value |= value >> 8UL;
        value |= value >> 16UL;
        value |= value >> 32UL;

        return tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58UL];
    }

    /**
     * @tparam T Type of the underlying object
     *
     * Union representing a chunk of memory mananaged by the pool.
     * It can be either a single object or a pointer to the next free space in the pool
     */
    template <typename T>
    union PGMemChunk
    {
        /** Storage for a single object */
        typename std::aligned_storage<sizeof(T), alignof(T)>::type element;

        /** Pointer to the next free space in the pool */
        PGMemChunk *next;
    };

    /**
     * @brief An implementation of an allocator pool
     *
     * @tparam T Type of the object to be created
     * @tparam N if N > 1, Number of object to be created at once when running out of empty element else the pool expand exponentially (default at 1)
     *
     * @warning This whole class is not thread safe ! The user should implement thread safety when using this in a concurrent environment
     */
    template <typename T, size_t N = 1>
    class AllocatorPool
    {
    public:
        /**
         * @brief Destroy the Allocator Pool object
         *
         * Delete all the object given back to the pool.
         *
         * @warning If the user forget to release memory, memory leaks can occur !
         * @note User can use destroyAll to avoid any memory leaks
         *
         * @see release
         * @see destroyAll
         */
        ~AllocatorPool()
        {
            LOG_THIS_MEMBER("Memory Pool");

            // Free the raw memory chunks
            for (PGMemChunk<T>* chunk : chunkList)
                delete[] chunk;  // Use delete[] to match new[]
        }

        /**
         * @brief Reserve enough space in the pool to hold the requested number of objects
         *
         * @param reserveSize The needed size of the pool
         */
        void reserve(size_t reserveSize)
        {
            LOG_THIS_MEMBER("Memory Pool");

            if (reserveSize < size) return;

            LOG_MILE("Memory Pool", "Reserving: " << reserveSize <<
                ", currentPoolSize = " << size <<
                " " << nbElements);

            while (reserveSize >= size)
            {
                const size_t blockSize = N >= 2 ? N : size == 0 ? 64 : size;

                LOG_MILE("Memory Pool", "Current size: " << size <<
                    ", target: " << reserveSize <<
                    ", blockSize: " << blockSize);

                auto newBlock = new PGMemChunk<T>[blockSize];

                chunkList.push_back(newBlock);

                size += blockSize;
            }
        }

        /**
         * @brief Function used to allocate a new T object
         *
         * @tparam Args Type of the arguments to be passed to create an object
         * @param args Argument to create a new T object
         * @return T* A pointer to the new T object created
         *
         * To be used instead of the default new operator to construct an object using the pool
         * To destroy this object use the release method of the pool
         *
         * @warning The release function NEED to be called on all the allocated objects before
         * deleting the pool otherwise some memory leaks will occur !
         *
         * @see release
         */
        template <typename... Args>
        T* allocate(Args&&... args)
        {
            LOG_THIS_MEMBER("Memory Pool");

            if (freeList)
            {
                auto chunk = freeList;
                freeList = chunk->next;

                ::new(&(chunk->element)) T(std::forward<Args>(args)...);

                nbElements++;

                return reinterpret_cast<T*>(chunk);
            }

            const size_t index = nbElements++;

            if (index >= size) reserve(index);

            // Track high-water mark for destructor cleanup
            if (index > maxAllocatedIndex)
                maxAllocatedIndex = index;

            // Todo Check if the chunk was created before creating a new element
            PGMemChunk<T>* chunk = getChunk(index);

            return ::new(&(chunk->element)) T(std::forward<Args>(args)...);
        }

        /**
         * @brief Allocate an element and return both the pointer and its index
         *
         * @return std::pair<T*, size_t> Pair of (pointer to element, index in pool)
         */
        template <typename... Args>
        std::pair<T*, size_t> allocateWithIndex(Args&&... args)
        {
            LOG_THIS_MEMBER("Memory Pool");

            if (freeList)
            {
                auto chunk = freeList;
                freeList = chunk->next;

                ::new(&(chunk->element)) T(std::forward<Args>(args)...);

                nbElements++;

                // Find the index by calculating from chunk pointer
                T* ptr = reinterpret_cast<T*>(chunk);
                size_t index = 0;
                for (size_t i = 0; i < size; i++)
                {
                    if (getElement(i) == ptr)
                    {
                        index = i;
                        break;
                    }
                }

                return {ptr, index};
            }

            const size_t index = nbElements++;

            if (index >= size) reserve(index);

            // Track high-water mark for destructor cleanup
            if (index > maxAllocatedIndex)
                maxAllocatedIndex = index;

            PGMemChunk<T>* chunk = getChunk(index);
            T* ptr = ::new(&(chunk->element)) T(std::forward<Args>(args)...);

            return {ptr, index};
        }

        // Todo add a bulk allocation and deallocation function

        /**
         * @brief Function used to release the memory of a T object create using the pool
         *
         * @param pointer A pointer to a T object
         *
         * Use this function to release memory of a T object created using the allocate function
         *
         * @see allocate
         */
        void release(T* pointer)
        {
            LOG_THIS_MEMBER("Memory Pool");

            if (pointer != nullptr)
            {
                pointer->~T();

                reinterpret_cast<PGMemChunk<T>*>(pointer)->next = freeList;
                freeList = reinterpret_cast<PGMemChunk<T>*>(pointer);

                nbElements--;
            }
        }

        /**
         * @brief Get the number of elements in the pool
         *
         * @return constexpr size_t The number of element in the pool
         */
        inline constexpr size_t getNbElements() const { return nbElements; }

        /**
         * @brief Get the current size of the pool (current nb max elements)
         *
         * @return constexpr size_t The size of the pool
         */
        inline constexpr size_t getSize() const { return size; }

        /**
         * @brief Destroy all remaining allocated objects
         *
         * This should be called before the pool is destroyed to properly
         * clean up objects with complex destructors (like ElementType with std::string)
         *
         * It will iterate on all chunk ever created to release all the memory held
         */
        void destroyAll()
        {
            if (maxAllocatedIndex == 0)
                return;

            // Build a set of free list pointers for fast lookup
            std::set<PGMemChunk<T>*> freeSet;
            PGMemChunk<T>* current = freeList;
            while (current != nullptr)
            {
                freeSet.insert(current);
                current = current->next;
            }

            // Iterate through all allocated indices and destroy objects not in free list
            for (size_t i = 0; i <= maxAllocatedIndex && i < size; ++i)
            {
                PGMemChunk<T>* chunk = getChunk(i);
                if (freeSet.find(chunk) == freeSet.end())
                {
                    // This object is still allocated, destroy it
                    T* obj = reinterpret_cast<T*>(chunk);
                    obj->~T();
                }
            }

            // Reset pool state - all objects are now destroyed
            nbElements = 0;
            freeList = nullptr;
        }

        /**
         * @brief Get a raw pointer to a pre-reserved slot at the given index
         *
         * The pool must already have been reserved to cover @p index.
         * The slot is returned as-is (no construction, no nbElements update).
         * Use this together with placement-new for parallel bulk allocation;
         * call advanceCount() once all slots have been constructed.
         *
         * @param index The pool index (must be < getSize())
         * @return T* Raw pointer to the storage at that index
         */
        T* getSlot(size_t index) const
        {
            return reinterpret_cast<T*>(getChunk(index));
        }

        /**
         * @brief Advance the element count by @p n without constructing any objects
         *
         * Use after parallel placement-new into pre-reserved slots obtained via getSlot().
         *
         * @param n Number of elements to mark as allocated
         */
        void advanceCount(size_t n)
        {
            if (n == 0) return;
            const size_t newMax = nbElements + n - 1;
            if (newMax > maxAllocatedIndex)
                maxAllocatedIndex = newMax;
            nbElements += n;
        }

        /**
         * @brief Get a specific element in the pool by his index
         *
         * @param index The position of the item in the pool
         * @return T* A pointer to the object
         *
         * @warning The object requested should be allocated prior to calling this
         */
        inline T* getElement(size_t index) const
        {
            LOG_THIS_MEMBER("Memory Pool");

            if (index >= size)
            {
                LOG_ERROR("Memory Pool", "Trying to acces a chunk outside of the pool");
                return nullptr;
            }

            return reinterpret_cast<T*>(getChunk(index));
        }

    protected:
        /**
         * @brief Get a specific chunk in the pool by his index
         *
         * @param index The position of the item in the pool
         * @return PGMemChunk<T>* A pointer to the chunk
         *
         * @warning The object requested should be allocated prior to calling this
         */
        inline PGMemChunk<T>* getChunk(size_t index) const
        {
            LOG_THIS_MEMBER("Memory Pool");

            if (N >= 2)
                return &chunkList[index / N][index % N];

            // Block layout (N == 1):
            //   Block 0          : indices [0,   63], size = 64
            //   Block k (k >= 1) : indices [2^(k+5), 2^(k+6) - 1], size = 2^(k+5)
            // For index < 64: block 0, offset = index
            // For index >= 64: n = floor(log2(index)), listPos = n - 5, offset = index - 2^n
            if (index < 64)
                return &chunkList[0][index];

#if defined(__GNUC__) || defined(__clang__)
            const uint64_t n = static_cast<uint64_t>(63 - __builtin_clzll(static_cast<unsigned long long>(index)));
#else
            const uint64_t n = log2_64(index);
#endif
            return &chunkList[n - 5][index - (size_t(1) << n)];
        }

    private:
        /** Current size of the memory pool */
        size_t size = 0;

        /** Current number of elements allocated in the memory pool */
        size_t nbElements = 0;

        /** High-water mark - highest index ever allocated */
        size_t maxAllocatedIndex = 0;

        /** Pointer to the next free object in the pool */
        PGMemChunk<T>* freeList = nullptr;

        /** PGMemChunk Lists used in the pool (used to free the memory) */
        std::vector<PGMemChunk<T>*> chunkList;
    };
}
