#include "stdafx.h"

#include "gtest/gtest.h"

#include "Memory/memorypool.h"

namespace pg
{
    namespace test
    {
        struct BasicObject
        {
            int id = 0;
        };

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_test, init)
        {
            AllocatorPool<BasicObject> pool;

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_test, single_alloc)
        {
            AllocatorPool<BasicObject> pool;

            auto elem = pool.allocate();

            EXPECT_NE(elem, nullptr);
            EXPECT_EQ(pool.getNbElements(), 1);
            EXPECT_EQ(pool.getSize(), 64);

            pool.release(elem);

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 64);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_test, multiple_alloc)
        {
            AllocatorPool<BasicObject> pool;

            std::vector<BasicObject*> objects;

            objects.reserve(1025);

            for (size_t i = 0; i < 1025; i++)
            {
                objects[i] = pool.allocate();

                EXPECT_NE(objects[i], nullptr);

                if (i == 0)
                {
                    EXPECT_EQ(pool.getNbElements(), 1);
                    EXPECT_EQ(pool.getSize(), 64);
                }
                else if (i == 1)
                {
                    EXPECT_EQ(pool.getNbElements(), 2);
                    EXPECT_EQ(pool.getSize(), 64);
                }
                else if (i == 3)
                {
                    EXPECT_EQ(pool.getNbElements(), 4);
                    EXPECT_EQ(pool.getSize(), 64);
                }
                else if (i == 7)
                {
                    EXPECT_EQ(pool.getNbElements(), 8);
                    EXPECT_EQ(pool.getSize(), 64);
                }
                else if (i == 15)
                {
                    EXPECT_EQ(pool.getNbElements(), 16);
                    EXPECT_EQ(pool.getSize(), 64);
                }
                else if (i == 31)
                {
                    EXPECT_EQ(pool.getNbElements(), 32);
                    EXPECT_EQ(pool.getSize(), 64);
                }
            }

            for (size_t i = 0; i < 1025; i++)
            {
                pool.release(objects[i]);
            }


            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 2048);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(fixed_memorypool_test, init)
        {
            AllocatorPool<BasicObject, 5> pool;

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(fixed_memorypool_test, single_alloc)
        {
            AllocatorPool<BasicObject, 5> pool;

            auto elem = pool.allocate();

            EXPECT_NE(elem, nullptr);
            EXPECT_EQ(pool.getNbElements(), 1);
            EXPECT_EQ(pool.getSize(), 5);

            pool.release(elem);

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 5);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_test, reserve)
        {
            AllocatorPool<BasicObject> pool;

            // Initially empty
            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 0);

            // Reserve space for 100 elements
            pool.reserve(100);

            // Pool size should be at least 100
            EXPECT_GE(pool.getSize(), 100);
            EXPECT_EQ(pool.getNbElements(), 0);

            size_t sizeAfterReserve = pool.getSize();

            // Allocate 50 elements - pool should not grow
            std::vector<BasicObject*> objects;
            objects.reserve(50);

            for (size_t i = 0; i < 50; i++)
            {
                objects.push_back(pool.allocate());
                EXPECT_NE(objects[i], nullptr);
            }

            // Pool size should remain the same (no reallocation needed)
            EXPECT_EQ(pool.getSize(), sizeAfterReserve);
            EXPECT_EQ(pool.getNbElements(), 50);

            // Release all objects
            for (auto* obj : objects)
            {
                pool.release(obj);
            }

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), sizeAfterReserve);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(fixed_memorypool_test, reserve)
        {
            AllocatorPool<BasicObject, 10> pool;

            // Initially empty
            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 0);

            // Reserve space for 25 elements (should create 3 blocks: 10, 10, 10)
            pool.reserve(25);

            EXPECT_GE(pool.getSize(), 25);
            EXPECT_EQ(pool.getNbElements(), 0);

            size_t sizeAfterReserve = pool.getSize();

            // Allocate 20 elements - pool should not grow
            std::vector<BasicObject*> objects;
            objects.reserve(20);

            for (size_t i = 0; i < 20; i++)
            {
                objects.push_back(pool.allocate());
                EXPECT_NE(objects[i], nullptr);
            }

            // Pool size should remain the same
            EXPECT_EQ(pool.getSize(), sizeAfterReserve);
            EXPECT_EQ(pool.getNbElements(), 20);

            // Release all objects
            for (auto* obj : objects)
            {
                pool.release(obj);
            }

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), sizeAfterReserve);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_test, destroyAll)
        {
            AllocatorPool<BasicObject> pool;

            // Allocate some objects
            std::vector<BasicObject*> objects;
            objects.reserve(10);

            for (size_t i = 0; i < 10; i++)
            {
                objects.push_back(pool.allocate());
                objects[i]->id = static_cast<int>(i);
                EXPECT_NE(objects[i], nullptr);
            }

            EXPECT_EQ(pool.getNbElements(), 10);

            // Release half of them
            for (size_t i = 0; i < 5; i++)
            {
                pool.release(objects[i]);
            }

            EXPECT_EQ(pool.getNbElements(), 5);

            // Destroy all remaining objects (should clean up the 5 unreleased objects)
            pool.destroyAll();

            // After destroyAll, nbElements should be reset to 0
            EXPECT_EQ(pool.getNbElements(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(memorypool_test, reserve_then_destroyAll)
        {
            AllocatorPool<BasicObject> pool;

            // Reserve space for 100 elements
            pool.reserve(100);
            size_t reservedSize = pool.getSize();

            EXPECT_GE(reservedSize, 100);
            EXPECT_EQ(pool.getNbElements(), 0);

            // Allocate 50 objects
            std::vector<BasicObject*> objects;
            objects.reserve(50);

            for (size_t i = 0; i < 50; i++)
            {
                objects.push_back(pool.allocate());
                objects[i]->id = static_cast<int>(i * 10);
                EXPECT_NE(objects[i], nullptr);
            }

            EXPECT_EQ(pool.getNbElements(), 50);
            EXPECT_EQ(pool.getSize(), reservedSize);

            // Release 20 objects
            for (size_t i = 0; i < 20; i++)
            {
                pool.release(objects[i]);
            }

            EXPECT_EQ(pool.getNbElements(), 30);

            // Destroy all remaining objects (should clean up the 30 unreleased objects)
            pool.destroyAll();

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), reservedSize);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(fixed_memorypool_test, destroyAll)
        {
            AllocatorPool<BasicObject, 5> pool;

            // Allocate some objects
            std::vector<BasicObject*> objects;
            objects.reserve(12);

            for (size_t i = 0; i < 12; i++)
            {
                objects.push_back(pool.allocate());
                objects[i]->id = static_cast<int>(i + 100);
                EXPECT_NE(objects[i], nullptr);
            }

            EXPECT_EQ(pool.getNbElements(), 12);
            EXPECT_EQ(pool.getSize(), 15); // 3 blocks of 5

            // Release some objects
            for (size_t i = 0; i < 7; i++)
            {
                pool.release(objects[i]);
            }

            EXPECT_EQ(pool.getNbElements(), 5);

            // Destroy all remaining objects
            pool.destroyAll();

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 15);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(fixed_memorypool_test, reserve_then_destroyAll)
        {
            AllocatorPool<BasicObject, 10> pool;

            // Reserve space for 50 elements (should create 5 blocks of 10)
            pool.reserve(50);
            size_t reservedSize = pool.getSize();

            EXPECT_GE(reservedSize, 50);
            EXPECT_EQ(pool.getNbElements(), 0);

            // Allocate 35 objects
            std::vector<BasicObject*> objects;
            objects.reserve(35);

            for (size_t i = 0; i < 35; i++)
            {
                objects.push_back(pool.allocate());
                objects[i]->id = static_cast<int>(i * 5);
                EXPECT_NE(objects[i], nullptr);
            }

            EXPECT_EQ(pool.getNbElements(), 35);
            EXPECT_EQ(pool.getSize(), reservedSize);

            // Release 15 objects
            for (size_t i = 0; i < 15; i++)
            {
                pool.release(objects[i]);
            }

            EXPECT_EQ(pool.getNbElements(), 20);

            // Destroy all remaining objects (should clean up the 20 unreleased objects)
            pool.destroyAll();

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), reservedSize);
        }
    }
}