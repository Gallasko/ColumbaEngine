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
            EXPECT_EQ(pool.getSize(), 1);

            pool.release(elem);

            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 1);
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
                    EXPECT_EQ(pool.getSize(), 1);
                }
                else if (i == 1)
                {
                    EXPECT_EQ(pool.getNbElements(), 2);
                    EXPECT_EQ(pool.getSize(), 3);
                }
                else if (i == 3)
                {
                    EXPECT_EQ(pool.getNbElements(), 4);
                    EXPECT_EQ(pool.getSize(), 7);
                }
                else if (i == 7)
                {
                    EXPECT_EQ(pool.getNbElements(), 8);
                    EXPECT_EQ(pool.getSize(), 15);
                }
                else if (i == 15)
                {
                    EXPECT_EQ(pool.getNbElements(), 16);
                    EXPECT_EQ(pool.getSize(), 31);
                }
                else if (i == 31)
                {
                    EXPECT_EQ(pool.getNbElements(), 32);
                    EXPECT_EQ(pool.getSize(), 63);
                }
            }

            for (size_t i = 0; i < 1025; i++)
            {
                pool.release(objects[i]);
            }


            EXPECT_EQ(pool.getNbElements(), 0);
            EXPECT_EQ(pool.getSize(), 2047);
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
    }
}