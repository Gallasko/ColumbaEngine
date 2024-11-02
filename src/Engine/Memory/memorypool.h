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
    union Chunk
    {
        /** Storage for a single object */
        typename std::aligned_storage<sizeof(T), alignof(T)>::type element;

        /** Pointer to the next free space in the pool */
        Chunk *next;
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
         * 
         * @see release
         */
        ~AllocatorPool()
        {
            LOG_THIS_MEMBER("Memory Pool");

            for (Chunk<T>* chunk : chunkList)
                delete chunk;
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
                const size_t blockSize = N >= 2 ? N : size == 0 ? 1 : size + 1;

                LOG_MILE("Memory Pool", "Current size: " << size <<
                    ", target: " << reserveSize <<
                    ", blockSize: " << blockSize);

                auto newBlock = new Chunk<T>[blockSize];

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

            // Todo Check if the chunk was created before creating a new element 
            Chunk<T>* chunk = getChunk(index);

            return ::new(&(chunk->element)) T(std::forward<Args>(args)...);
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

                reinterpret_cast<Chunk<T>*>(pointer)->next = freeList;
                freeList = reinterpret_cast<Chunk<T>*>(pointer);

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
         * @return Chunk<T>* A pointer to the chunk
         * 
         * @warning The object requested should be allocated prior to calling this
         */
        inline Chunk<T>* getChunk(size_t index) const
        {
            LOG_THIS_MEMBER("Memory Pool");

            const uint64_t n = log2_64(index + 1);
            const size_t containerSize = N >= 2 ? N : n == 0 ? 0 : 1 << n;

            const size_t listPos = N >= 2 ? index / containerSize : n;
            const size_t vectorPos = N >= 2 ? index % containerSize : n == 0 ? 0 : index + 1 - containerSize;

            return &chunkList[listPos][vectorPos];
        }
    
    private:
        /** Current size of the memory pool */
        size_t size = 0;

        /** Current number of elements allocated in the memory pool */
        size_t nbElements = 0;

        /** Pointer to the next free object in the pool */
        Chunk<T>* freeList = nullptr;

        /** Chunk Lists used in the pool (used to free the memory) */
        std::vector<Chunk<T>*> chunkList;
    };
}
