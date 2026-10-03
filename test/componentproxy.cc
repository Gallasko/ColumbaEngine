#include "stdafx.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"
#include "ECS/entitysystem.h"
#include "2D/position.h"

#include "mocklogger.h"

namespace pg
{
    // A plain serializable object, to check the copies between C++ objects and script tables
    struct ProxyTestRecord
    {
        int count = 0;
        float ratio = 0.0f;
        bool enabled = false;
        std::string label;
        std::vector<int> list;
        std::unordered_map<std::string, int> scores;
    };

    template <>
    void serialize(Archive& archive, const ProxyTestRecord& value)
    {
        archive.startSerialization("ProxyTestRecord");

        serialize(archive, "count", value.count);
        serialize(archive, "ratio", value.ratio);
        serialize(archive, "enabled", value.enabled);
        serialize(archive, "label", value.label);
        serialize(archive, "list", value.list);
        serialize(archive, "scores", value.scores);

        archive.endSerialization();
    }

    namespace test
    {
        namespace
        {
            // Stands for a C++ component: the proxy under test forwards to it
            struct ProxyTarget
            {
                float value = 1.5f;
                int count = 3;
                std::string label = "hello world";
                int locked = 42;

                int nbWrites = 0;
            };

            ComponentProxyMetadata makeTargetMetadata()
            {
                ComponentProxyMetadata metadata;

                metadata.componentTypeName = "ProxyTarget";

                metadata.addProperty(PropertyMetadata{"value", PropertyType::Float, true,
                    [](void* component, VM*) -> Value {
                        return makeFloatValue(static_cast<ProxyTarget*>(component)->value);
                    },
                    [](void* component, VM*, Value value) {
                        auto target = static_cast<ProxyTarget*>(component);

                        target->value = IS_DOUBLE(value) ? static_cast<float>(AS_DOUBLE(value)) : static_cast<float>(AS_INT(value));
                        target->nbWrites++;
                    },
                    nullptr,
                    nullptr});

                metadata.addProperty(PropertyMetadata{"count", PropertyType::Int, true,
                    [](void* component, VM*) -> Value {
                        return makeIntValue(static_cast<ProxyTarget*>(component)->count);
                    },
                    [](void* component, VM*, Value value) {
                        auto target = static_cast<ProxyTarget*>(component);

                        target->count = static_cast<int>(AS_INT(value));
                        target->nbWrites++;
                    },
                    nullptr,
                    nullptr});

                metadata.addProperty(PropertyMetadata{"label", PropertyType::String, true,
                    [](void* component, VM* vm) -> Value {
                        return vm->createString(static_cast<ProxyTarget*>(component)->label);
                    },
                    [](void* component, VM* vm, Value value) {
                        auto target = static_cast<ProxyTarget*>(component);

                        target->label = vm->asString(value);
                        target->nbWrites++;
                    },
                    nullptr,
                    nullptr});

                // Read only: no setter
                metadata.addProperty(PropertyMetadata{"locked", PropertyType::Int, false,
                    [](void* component, VM*) -> Value {
                        return makeIntValue(static_cast<ProxyTarget*>(component)->locked);
                    },
                    nullptr,
                    nullptr,
                    nullptr});

                return metadata;
            }

            // A VM with a proxy on a ProxyTarget stored in the global "target"
            struct ProxyRun
            {
                ProxyRun()
                {
                    ecs.setupVm(*vm);

                    ComponentProxyRegistry::instance().registerMetadata(makeTargetMetadata());

                    auto metadata = ComponentProxyRegistry::instance().findMetadata("ProxyTarget");

                    vm->defineGlobal("target", ComponentProxy::createProxy(vm.get(), metadata, &target));
                }

                InterpretResult run(const std::string& source)
                {
                    return vm->interpretFromText(source);
                }

                Value global(const std::string& name)
                {
                    const Value value = vm->findGlobal(name);

                    EXPECT_FALSE(IS_UNDEFINED(value)) << name;

                    return value;
                }

                EntitySystem ecs;
                std::unique_ptr<VM> vm = std::make_unique<VM>();   // The VM's stack is large: keep it off the test's

                ProxyTarget target;
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, registry_lookup)
        {
            MockLogger logger;

            ComponentProxyRegistry::instance().registerMetadata(makeTargetMetadata());

            auto metadata = ComponentProxyRegistry::instance().findMetadata("ProxyTarget");

            ASSERT_NE(metadata, nullptr);

            EXPECT_EQ(metadata->componentTypeName, "ProxyTarget");
            EXPECT_EQ(metadata->properties.size(), 4);

            ASSERT_NE(metadata->findProperty("count"), nullptr);
            EXPECT_EQ(metadata->findProperty("count")->type, PropertyType::Int);
            EXPECT_EQ(metadata->findProperty("missing"), nullptr);

            EXPECT_TRUE(ComponentProxyRegistry::instance().hasMetadata("ProxyTarget"));
            EXPECT_EQ(ComponentProxyRegistry::instance().findMetadata("NoSuchComponent"), nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, read_properties)
        {
            MockLogger logger;

            ProxyRun run;

            auto result = run.run("var a = target.value\nvar b = target.count\nvar c = target.label\nvar d = target.locked\n");

            ASSERT_EQ(result, InterpretResult::OK);

            ASSERT_TRUE(IS_DOUBLE(run.global("a")));
            EXPECT_DOUBLE_EQ(AS_DOUBLE(run.global("a")), 1.5);

            ASSERT_TRUE(IS_INT(run.global("b")));
            EXPECT_EQ(AS_INT(run.global("b")), 3);

            ASSERT_TRUE(IS_STRING(run.global("c")));
            EXPECT_EQ(run.vm->asString(run.global("c")), "hello world");

            EXPECT_EQ(AS_INT(run.global("d")), 42);

            EXPECT_EQ(run.target.nbWrites, 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, read_follows_the_component)
        {
            MockLogger logger;

            ProxyRun run;

            // Changed after the proxy was created: a proxy holds no copy of the component
            run.target.count = 99;

            auto result = run.run("var b = target.count\n");

            ASSERT_EQ(result, InterpretResult::OK);

            EXPECT_EQ(AS_INT(run.global("b")), 99);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, write_goes_through_the_setter)
        {
            MockLogger logger;

            ProxyRun run;

            auto result = run.run("target.value = 4.25\ntarget.count = 7\ntarget.label = \"a longer label\"\nvar back = target.count\n");

            ASSERT_EQ(result, InterpretResult::OK);

            EXPECT_FLOAT_EQ(run.target.value, 4.25f);
            EXPECT_EQ(run.target.count, 7);
            EXPECT_EQ(run.target.label, "a longer label");
            EXPECT_EQ(run.target.nbWrites, 3);

            EXPECT_EQ(AS_INT(run.global("back")), 7);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, write_in_a_loop)
        {
            MockLogger logger;

            ProxyRun run;

            auto result = run.run("for (var i = 0; i < 100; i = i + 1)\n{\n    target.count = target.count + 1\n}\n");

            ASSERT_EQ(result, InterpretResult::OK);

            EXPECT_EQ(run.target.count, 103);
            EXPECT_EQ(run.target.nbWrites, 100);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, read_only_property_is_an_error)
        {
            MockLogger logger;

            ProxyRun run;

            auto result = run.run("target.locked = 1\n");

            EXPECT_EQ(result, InterpretResult::RUNTIME_ERROR);

            EXPECT_EQ(run.target.locked, 42);
            EXPECT_EQ(run.target.nbWrites, 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, unknown_property)
        {
            MockLogger logger;

            ProxyRun run;

            // Reads as -1, and a write is ignored
            auto result = run.run("var u = target.missing\ntarget.missing = 5\n");

            ASSERT_EQ(result, InterpretResult::OK);

            ASSERT_TRUE(IS_INT(run.global("u")));
            EXPECT_EQ(AS_INT(run.global("u")), -1);

            EXPECT_EQ(run.target.nbWrites, 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, index_access)
        {
            MockLogger logger;

            ProxyRun run;

            auto result = run.run("var a = target[\"value\"]\ntarget[\"count\"] = 5\n");

            ASSERT_EQ(result, InterpretResult::OK);

            EXPECT_DOUBLE_EQ(AS_DOUBLE(run.global("a")), 1.5);
            EXPECT_EQ(run.target.count, 5);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, class_name)
        {
            MockLogger logger;

            ProxyRun run;

            auto result = run.run("var n = target.__className\n");

            ASSERT_EQ(result, InterpretResult::OK);

            ASSERT_TRUE(IS_STRING(run.global("n")));
            EXPECT_EQ(run.vm->asString(run.global("n")), "ProxyTarget");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, standard_component_properties_are_dynamic)
        {
            MockLogger logger;

            EntitySystem ecs;
            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            StandardComponent component("Health");

            component.set("hp", 10);

            auto metadata = ComponentProxyRegistry::instance().findMetadata("StandardComponent");

            ASSERT_NE(metadata, nullptr);

            vm->defineGlobal("health", ComponentProxy::createProxy(vm.get(), metadata, &component));

            auto result = vm->interpretFromText("var hp = health.hp\nvar missing = health.mana\n");

            ASSERT_EQ(result, InterpretResult::OK);

            EXPECT_EQ(AS_INT(vm->findGlobal("hp")), 10);
            EXPECT_EQ(AS_INT(vm->findGlobal("missing")), -1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, entity_table_exposes_components_as_proxies)
        {
            MockLogger logger;

            EntitySystem ecs;
            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            ecs.createSystem<PositionComponentSystem>();

            auto entity = ecs.createEntity();
            auto pos = entity->attach<PositionComponent>();

            pos->x = 10.0f;
            pos->y = 20.0f;

            vm->defineGlobal("ent", serializeEntityToTable(vm.get(), &ecs, entity.entity));

            auto result = vm->interpretFromText(
                "var pos = ent[\"PositionComponent\"]\n"
                "var id = ent.__entityId\n"
                "var x = pos.x\n"
                "var hasPos = ent.has(\"PositionComponent\")\n"
                "var hasOther = ent.has(\"NoSuchComponent\")\n"
                "var name = pos.__className\n"
                "pos.y = 50\n");

            ASSERT_EQ(result, InterpretResult::OK);

            EXPECT_EQ(static_cast<_unique_id>(AS_INT(vm->findGlobal("id"))), entity.id);
            EXPECT_DOUBLE_EQ(AS_DOUBLE(vm->findGlobal("x")), 10.0);
            EXPECT_TRUE(AS_BOOL(vm->findGlobal("hasPos")));
            EXPECT_FALSE(AS_BOOL(vm->findGlobal("hasOther")));
            EXPECT_EQ(vm->asString(vm->findGlobal("name")), "PositionComponent");

            // The write reached the C++ component
            EXPECT_FLOAT_EQ(pos->y, 50.0f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_proxy_test, entity_view_only_holds_requested_components)
        {
            MockLogger logger;

            EntitySystem ecs;
            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            ecs.createSystem<PositionComponentSystem>();

            auto entity = ecs.createEntity();

            entity->attach<PositionComponent>();

            Value requested = serializeEntityViewToTable(vm.get(), &ecs, entity.id, {"PositionComponent"});
            Value other = serializeEntityViewToTable(vm.get(), &ecs, entity.id, {"NoSuchComponent"});

            auto requestedTable = vm->asInstance(requested);
            auto otherTable = vm->asInstance(other);

            EXPECT_EQ(static_cast<_unique_id>(AS_INT(requestedTable->getField("__entityId"))), entity.id);
            EXPECT_TRUE(requestedTable->hasField("PositionComponent"));
            EXPECT_FALSE(requestedTable->hasField("attachComp"));

            EXPECT_TRUE(otherTable->hasField("__entityId"));
            EXPECT_FALSE(otherTable->hasField("PositionComponent"));

            // A view gives a proxy too, not a copy
            ASSERT_TRUE(IS_INSTANCE(requestedTable->getField("PositionComponent")));
            EXPECT_NE(vm->asInstance(requestedTable->getField("PositionComponent"))->proxyMeta, nullptr);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(table_serialization_test, object_to_table)
        {
            MockLogger logger;

            EntitySystem ecs;
            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            ProxyTestRecord record;

            record.count = 7;
            record.ratio = 0.5f;
            record.enabled = true;
            record.label = "";
            record.list = {4, 5, 6};
            record.scores = {{"alpha", 1}};

            Value tableValue = serializeToTable(vm.get(), record);

            ASSERT_TRUE(IS_INSTANCE(tableValue));

            auto table = vm->asInstance(tableValue);

            EXPECT_EQ(vm->asString(table->getField("__className")), "ProxyTestRecord");
            EXPECT_EQ(AS_INT(table->getField("count")), 7);
            EXPECT_DOUBLE_EQ(AS_DOUBLE(table->getField("ratio")), 0.5);
            EXPECT_TRUE(AS_BOOL(table->getField("enabled")));

            // An empty string is still a field of the table
            ASSERT_TRUE(table->hasField("label"));
            ASSERT_TRUE(IS_STRING(table->getField("label")));
            EXPECT_EQ(vm->asString(table->getField("label")), "");

            ASSERT_TRUE(IS_INSTANCE(table->getField("list")));

            auto list = vm->asInstance(table->getField("list"));

            EXPECT_EQ(list->fieldValues.size(), 3);
            EXPECT_EQ(AS_INT(list->getField("0")), 4);
            EXPECT_EQ(AS_INT(list->getField("1")), 5);
            EXPECT_EQ(AS_INT(list->getField("2")), 6);

            ASSERT_TRUE(IS_INSTANCE(table->getField("scores")));

            auto scores = vm->asInstance(table->getField("scores"));

            EXPECT_EQ(scores->fieldValues.size(), 1);
            EXPECT_EQ(AS_INT(scores->getField("alpha")), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(table_serialization_test, component_round_trip)
        {
            MockLogger logger;

            EntitySystem ecs;
            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            PositionComponent component;

            component.x = 3.5f;
            component.y = -12.25f;
            component.width = 100.0f;
            component.visible = false;

            Value tableValue = serializeToTable(vm.get(), component);

            auto table = vm->asInstance(tableValue);

            EXPECT_EQ(vm->asString(table->getField("__className")), "PositionComponent");
            EXPECT_DOUBLE_EQ(AS_DOUBLE(table->getField("x")), 3.5);

            auto result = deserializeTo<PositionComponent>(vm.get(), tableValue);

            EXPECT_FLOAT_EQ(result.x, 3.5f);
            EXPECT_FLOAT_EQ(result.y, -12.25f);
            EXPECT_FLOAT_EQ(result.width, 100.0f);
            EXPECT_EQ(result.visible, false);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(table_serialization_test, not_a_table_gives_a_default_object)
        {
            MockLogger logger;

            EntitySystem ecs;
            auto vm = std::make_unique<VM>();

            ecs.setupVm(*vm);

            auto result = deserializeTo<PositionComponent>(vm.get(), makeIntValue(3));

            EXPECT_FLOAT_EQ(result.x, 0.0f);
            EXPECT_EQ(result.visible, true);
        }
    }
}
