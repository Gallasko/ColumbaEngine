#include "gtest/gtest.h"

#include <chrono>

#include "Memory/memorypool.h"

namespace pg
{
    namespace benchmark
    {
        struct BasicObject
        {
            int id = 0;
        };

        auto valueToTest = {10, 100, 1000, 10000, 100000, 1000000, 2000000, 5000000, 8000000, 10000000, 20000000, 50000000, 80000000, 100000000};

        void runMemoryPoolAllocDealloc(unsigned int size)
        {
            AllocatorPool<BasicObject> pool;

            std::vector<BasicObject*> objects;

            objects.reserve(size);

            auto start = std::chrono::high_resolution_clock::now();
            auto end = std::chrono::high_resolution_clock::now();

            start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < size; ++i)
            {
                objects[i] = pool.allocate();
            }

            end = std::chrono::high_resolution_clock::now();

            std::cout << "Memory Pool Allocation for " << size << " objects took: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;

            start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < size; ++i)
            {
                pool.release(objects[i]);
            }

            end = std::chrono::high_resolution_clock::now();

            std::cout << "Memory Pool Deallocation for " << size << " objects took: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;
        }

        void runMemoryPoolRealloc(unsigned int size)
        {
            AllocatorPool<BasicObject> pool;

            std::vector<BasicObject*> objects;

            objects.reserve(size);

            auto start = std::chrono::high_resolution_clock::now();
            auto end = std::chrono::high_resolution_clock::now();

            // Allocate once and delete the element right after
            for (size_t i = 0; i < size; ++i)
            {
                objects[i] = pool.allocate();
                pool.release(objects[i]);
            }

            // This measure the time for a realloc
            start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < size; ++i)
            {
                objects[i] = pool.allocate();
            }

            end = std::chrono::high_resolution_clock::now();

            std::cout << "Memory Pool Reallocation for " << size << " objects took: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;

            for (size_t i = 0; i < size; ++i)
            {
                pool.release(objects[i]);
            }
        }

        void runstdAllocDealloc(unsigned int size)
        {
            AllocatorPool<BasicObject> pool;

            std::vector<BasicObject*> objects;

            objects.reserve(size);

            auto start = std::chrono::high_resolution_clock::now();
            auto end = std::chrono::high_resolution_clock::now();

            start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < size; ++i)
            {
                objects[i] = new BasicObject();
            }

            end = std::chrono::high_resolution_clock::now();

            std::cout << "Std Allocation for " << size << " objects took: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;

            start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < size; ++i)
            {
                delete objects[i];
            }

            end = std::chrono::high_resolution_clock::now();

            std::cout << "Std Deallocation for " << size << " objects took: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;
        }

        void runStdRealloc(unsigned int size)
        {
            AllocatorPool<BasicObject> pool;

            std::vector<BasicObject*> objects;

            objects.reserve(size);

            auto start = std::chrono::high_resolution_clock::now();
            auto end = std::chrono::high_resolution_clock::now();

            // Allocate once and delete the element right after
            for (size_t i = 0; i < size; ++i)
            {
                objects[i] = new BasicObject();
                delete objects[i];
            }

            // This measure the time for a realloc
            start = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < size; ++i)
            {
                objects[i] = new BasicObject();
            }

            end = std::chrono::high_resolution_clock::now();

            std::cout << "Std Reallocation for " << size << " objects took: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns" << std::endl;

            for (size_t i = 0; i < size; ++i)
            {
                delete objects[i];
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_benchmark, allocation_desallocation)
        {
            for (auto value : valueToTest)
            {
                runMemoryPoolAllocDealloc(value);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(std_allocdealloc_benchmark, allocation_desallocation)
        {
            for (auto value : valueToTest)
            {
                runstdAllocDealloc(value);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_benchmark, realloc)
        {
            for (auto value : valueToTest)
            {
                runMemoryPoolRealloc(value);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(std_allocdealloc_benchmark, realloc)
        {
            for (auto value : valueToTest)
            {
                runStdRealloc(value);
            }
        }
    }
}