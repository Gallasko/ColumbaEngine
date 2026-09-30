#include "stdafx.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Compiler/vm.h"
#include "Compiler/vmreaders.h"
#include "ECS/entitysystem.h"

#include "mocklogger.h"

// The readers the prefab loader and the rule scripts share: a script's globals into ElementTypes.
namespace pg
{
    namespace test
    {
        namespace
        {
            struct ScriptRun
            {
                EntitySystem ecs;
                std::unique_ptr<VM> vm = std::make_unique<VM>();   // The VM's stack is large: keep it off the test's

                explicit ScriptRun(const std::string& source)
                {
                    ecs.setupVm(*vm);
                    result = vm->interpretFromText(source);
                }

                Value global(const std::string& name)
                {
                    VM::GlobalCell* cell = vm->findGlobalCell(name);
                    EXPECT_NE(cell, nullptr) << name;
                    EXPECT_TRUE(cell and cell->defined) << name;

                    return cell ? cell->value : Value{};
                }

                InterpretResult result;
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(vmreaders_test, scalar_kinds)
        {
            MockLogger logger;
            ScriptRun run("var i = 12\nvar d = 12.5\nvar b = true\nvar s = \"Squire\"\nvar v = [1]\n");
            ASSERT_EQ(run.result, InterpretResult::OK);

            EXPECT_TRUE(vmread::isScalar(run.global("i")));
            EXPECT_TRUE(vmread::isScalar(run.global("s")));
            EXPECT_FALSE(vmread::isScalar(run.global("v")));

            const ElementType i = vmread::scalar(*run.vm, run.global("i"));
            ASSERT_EQ(i.type, UnionType::INT);
            EXPECT_EQ(i.get<int>(), 12);

            const ElementType d = vmread::scalar(*run.vm, run.global("d"));
            ASSERT_EQ(d.type, UnionType::FLOAT);
            EXPECT_FLOAT_EQ(d.get<float>(), 12.5f);

            const ElementType b = vmread::scalar(*run.vm, run.global("b"));
            ASSERT_EQ(b.type, UnionType::BOOL);
            EXPECT_TRUE(b.get<bool>());

            const ElementType s = vmread::scalar(*run.vm, run.global("s"));
            ASSERT_EQ(s.type, UnionType::STRING);
            EXPECT_EQ(s.get<std::string>(), "Squire");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(vmreaders_test, records_and_a_nested_scalar_list)
        {
            MockLogger logger;
            ScriptRun run(
                "var rows = [{stat: \"STR\", amount: 1}, {stat: \"VIT\", amount: -2}]\n"
                "var ages = [7, 14, 18.5]\n"
                "var squire = {name: \"Squire\", asks: [18, 4]}\n");
            ASSERT_EQ(run.result, InterpretResult::OK);

            RecordList rows;
            ASSERT_TRUE(vmread::records(*run.vm, run.global("rows"), rows, nullptr, "rows"));
            ASSERT_EQ(rows.size(), 2u);
            EXPECT_EQ(rows[0].at("stat").get<std::string>(), "STR");
            EXPECT_EQ(rows[1].at("amount").get<int>(), -2);

            std::vector<ElementType> ages;
            ASSERT_TRUE(vmread::scalars(*run.vm, run.global("ages"), ages, nullptr, "ages"));
            ASSERT_EQ(ages.size(), 3u);
            EXPECT_EQ(ages[1].get<int>(), 14);
            EXPECT_FLOAT_EQ(ages[2].get<float>(), 18.5f);

            // A record is flat: a list inside it is refused, and says where
            std::vector<std::string> errors;
            ElementMap squire;
            EXPECT_FALSE(vmread::record(*run.vm, run.global("squire"), squire, &errors, "squire"));
            ASSERT_EQ(errors.size(), 1u);
            EXPECT_EQ(errors[0], "squire.asks: only scalars are allowed inside a record");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(vmreaders_test, wrong_shapes_are_errors)
        {
            MockLogger logger;
            ScriptRun run("var n = 3\nvar mixed = [{a: 1}, 2]\nvar words = [\"a\", [1]]\n");
            ASSERT_EQ(run.result, InterpretResult::OK);

            std::vector<std::string> errors;

            ElementMap rec;
            EXPECT_FALSE(vmread::record(*run.vm, run.global("n"), rec, &errors, "n"));

            RecordList list;
            EXPECT_FALSE(vmread::records(*run.vm, run.global("n"), list, &errors, "n"));
            EXPECT_FALSE(vmread::records(*run.vm, run.global("mixed"), list, &errors, "mixed"));

            std::vector<ElementType> values;
            EXPECT_FALSE(vmread::scalars(*run.vm, run.global("words"), values, &errors, "words"));

            ASSERT_EQ(errors.size(), 4u);
            EXPECT_EQ(errors[0], "n: must be a table");
            EXPECT_EQ(errors[1], "n: must be a list of tables");
            EXPECT_EQ(errors[2], "mixed[1]: must be a table");
            EXPECT_EQ(errors[3], "words[1]: must be a scalar");
        }
    }
}
