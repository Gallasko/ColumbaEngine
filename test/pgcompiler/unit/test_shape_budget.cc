#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/object.h"

#include <algorithm>
#include <string>
#include <vector>

// A VM's tree of shapes (object.h) is bounded: past ShapeTree::maxShapes a table keeps a dictionary
// of its own, and past ShapeTree::maxBuiltNames the built indexes are let go and built again as they
// are asked. These pin that tables still read and write the same either way, and that a name a
// shape hands out stays good when its index is let go.

namespace pg
{
    namespace test
    {
        namespace
        {
            // A table of `vm` filled with `names`, in that order, each worth its place in the list
            Value filled(VM& vm, const std::vector<std::string>& names)
            {
                Value v = vm.createTable();
                ObjInstance* table = vm.asInstance(v);

                for (size_t i = 0; i < names.size(); ++i)
                    table->setField(names[i], makeIntValue(static_cast<int64_t>(i)));

                return v;
            }

            // Every name reads its value back, by name and by slot
            void expectFilled(VM& vm, Value v, const std::vector<std::string>& names)
            {
                ObjInstance* table = vm.asInstance(v);

                ASSERT_EQ(table->fieldCount(), names.size());

                for (size_t i = 0; i < names.size(); ++i)
                {
                    const size_t slot = table->findField(names[i]);

                    ASSERT_NE(slot, ObjInstance::npos) << names[i];
                    EXPECT_EQ(AS_INT(table->fieldValue(slot)), static_cast<int64_t>(i)) << names[i];
                    EXPECT_EQ(table->fieldName(slot), names[i]);
                }

                EXPECT_EQ(table->findField("absent"), ObjInstance::npos);
            }

            std::vector<std::string> namesOf(size_t count, const std::string& prefix)
            {
                std::vector<std::string> names;

                for (size_t i = 0; i < count; ++i)
                    names.push_back(prefix + std::to_string(i));

                return names;
            }
        }

        // Tables filled in more orders than the tree keeps shapes for: the tree stops at its bound,
        // and every table, on a shape or off them, reads as it was filled
        TEST(shape_budget_test, tables_in_every_order_stay_within_the_shapes)
        {
            VM vm;

            std::vector<std::string> names = namesOf(6, "stat_");
            std::vector<Value> tables;
            std::vector<std::vector<std::string>> orders;

            // 6! = 720 orders, ten times over with a different seventh name: 7,200 tables, far more
            // shapes than that once every prefix is counted
            for (int round = 0; round < 10; ++round)
            {
                std::vector<std::string> order = names;
                std::sort(order.begin(), order.end());

                do
                {
                    std::vector<std::string> withExtra = order;
                    withExtra.push_back("extra_" + std::to_string(round));

                    tables.push_back(filled(vm, withExtra));
                    orders.push_back(withExtra);
                }
                while (std::next_permutation(order.begin(), order.end()));
            }

            EXPECT_LE(vm.shapeTree.owned.size(), ShapeTree::maxShapes);

            for (size_t i = 0; i < tables.size(); ++i)
                expectFilled(vm, tables[i], orders[i]);
        }

        // Deep shapes, each asked often enough to be built: the names their indexes hold stay within
        // the budget, and the tables read the same after the indexes were let go
        TEST(shape_budget_test, built_indexes_stay_within_their_budget)
        {
            VM vm;

            std::vector<Value> tables;
            std::vector<std::vector<std::string>> fills;

            // 300 chains of 90 names, none sharing a first name, and a table at every step of each:
            // 27,000 shapes, whose indexes built would hold 300 * (1 + ... + 90) = 1,228,500 names
            for (int c = 0; c < 300; ++c)
            {
                const std::vector<std::string> chain = namesOf(90, "c" + std::to_string(c) + "_");

                for (size_t depth = 1; depth <= chain.size(); ++depth)
                {
                    fills.emplace_back(chain.begin(), chain.begin() + static_cast<long>(depth));
                    tables.push_back(filled(vm, fills.back()));
                }
            }

            ASSERT_LE(vm.shapeTree.owned.size(), ShapeTree::maxShapes);

            size_t mostBuilt = 0;

            for (int round = 0; round < 2; ++round)
            {
                for (size_t i = 0; i < tables.size(); ++i)
                {
                    expectFilled(vm, tables[i], fills[i]);
                    mostBuilt = std::max(mostBuilt, vm.shapeTree.builtNames);
                }
            }

            EXPECT_LE(mostBuilt, ShapeTree::maxBuiltNames);

            // (and the budget was reached: the indexes were let go on the way)
            EXPECT_GT(mostBuilt, ShapeTree::maxBuiltNames / 2);
        }

        // A name a shape hands out (fieldName) is the shape tree's own, not its index's: it stays
        // good when every index is let go
        TEST(shape_budget_test, a_name_handed_out_outlives_the_index)
        {
            VM vm;

            const std::vector<std::string> names = namesOf(12, "field_name_long_enough_to_allocate_");
            Value v = filled(vm, names);
            ObjInstance* table = vm.asInstance(v);

            // Asked twice: built
            table->findField(names[3]);
            table->findField(names[4]);

            const std::string& held = table->fieldName(5);

            vm.shapeTree.unbuildAll();

            EXPECT_EQ(vm.shapeTree.builtNames, 0u);
            EXPECT_EQ(held, names[5]);

            expectFilled(vm, v, names);
        }
    }
}
