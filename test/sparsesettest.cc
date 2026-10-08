#include "stdafx.h"

#include <iostream>

#include "gtest/gtest.h"

#include "ECS/sparseset.h"
#include "ECS/entitysystem.h"

namespace pg
{
    namespace test
    {
        struct A
        {
            A(int data) : data(data) {}

            int data = 0;
        };

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sparse_test, initialization)
        {
            SparseSet set;
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(sparse_test, iterate)
        {
            SparseSet set;

            for (int i = 1; i < 1000; i++)
            {
                set.add(i);
            }

            auto view = set.view();

            for (size_t i = 1; i < set.nbElements(); i++)
            {
                EXPECT_EQ(view[i], i);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // An id that is not in the set has nothing to remove: asked twice, or never added, the set
        // says so and stays as it was. It used to take its size down and move another element.
        TEST(sparse_test, removing_what_is_not_there_changes_nothing)
        {
            SparseSet set;

            for (int i = 1; i <= 10; i++)
                set.add(i);

            const size_t count = set.nbElements();

            EXPECT_NE(set.remove(4), 0u);
            EXPECT_EQ(set.nbElements(), count - 1);
            EXPECT_FALSE(set.has(4));

            // Twice, never added, and past what the set ever held
            EXPECT_EQ(set.remove(4), 0u);
            EXPECT_EQ(set.remove(25), 0u);
            EXPECT_EQ(set.remove(100000), 0u);
            EXPECT_EQ(set.nbElements(), count - 1);

            for (int i = 1; i <= 10; i++)
                EXPECT_EQ(set.has(i), i != 4) << i;

            // The last one, then the same again
            EXPECT_NE(set.remove(10), 0u);
            EXPECT_EQ(set.remove(10), 0u);
            EXPECT_FALSE(set.has(10));
            EXPECT_EQ(set.nbElements(), count - 2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The same for a set of components: what the others hold is theirs after a removal that
        // had nothing to remove, and a component can be attached to the id again.
        TEST(sparse_test, a_component_removed_twice_leaves_the_others)
        {
            ComponentSet<A> set;

            for (int i = 1; i <= 6; i++)
                set.addComponent(static_cast<_unique_id>(i), i * 10);

            set.removeComponent(static_cast<_unique_id>(3));
            set.removeComponent(static_cast<_unique_id>(3));
            set.removeComponent(static_cast<_unique_id>(42));

            for (int i = 1; i <= 6; i++)
            {
                A* a = set.atEntity(static_cast<_unique_id>(i));

                if (i == 3)
                {
                    EXPECT_EQ(a, nullptr);
                    continue;
                }

                ASSERT_NE(a, nullptr) << i;
                EXPECT_EQ(a->data, i * 10);
            }

            A* again = set.addComponent(static_cast<_unique_id>(3), 33);

            ASSERT_NE(again, nullptr);
            EXPECT_EQ(set.atEntity(static_cast<_unique_id>(3))->data, 33);
            EXPECT_EQ(set.atEntity(static_cast<_unique_id>(6))->data, 60);
        }

    }
}
