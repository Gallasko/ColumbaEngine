#include "gtest/gtest.h"

#include "ECS/uniqueid.h"
#include "ECS/entity.h"
#include "ECS/system.h"
#include "ECS/componentregistry.h"
#include "ECS/entitysystem.h"

#include "mocklogger.h"

#include <iostream>
#include <string>

#include <chrono>

namespace pg
{
    namespace test
    {
        namespace
        {
            constexpr size_t NUMBEROFENTITYTRIES = 10000;

            struct A
            {
                A(int arg1, int arg2)
                {
                    value = arg1 + arg2;
                }

                int value;
            };

            struct B
            {
                B(int arg1, int arg2)
                {
                    value = arg1 - arg2;
                }

                int value;
            };

            struct ASystem : public System<Own<A>>
            {
                virtual void execute() { }

                size_t getNbComponents() const { return view<A>().nbComponents(); }
            };

            struct ABSystem : public System<Ref<A>, Own<B>>
            {
                virtual void execute() { }
            };

            struct C
            {
                C(_unique_id value, const std::string& text) : value(value), text(text) {}

                _unique_id value;
                std::string text;
            };

            struct CSystem : public System<Own<C>>
            {
                CSystem(const size_t count) : System<Own<C>>(), count(count) {}

                virtual void execute()
                {
                    size_t sum = 0;

                    auto list = view<C>();

                    for (size_t i = 1; i < list.nbComponents(); i++)
                    {
                        sum += list[i]->value;
                    }

                    LOG_TEST("CSystem", sum);
                }

                size_t count = 1;
            };

            struct D
            {
                std::string text;
            };

            struct DSystem : public System<Own<D>>
            {
                virtual void execute() { }
            };

            struct EEvent
            {
                std::string payload;
            };

            struct E
            {
                std::string message;
            };

            struct ESystem : public System<Own<E>, Listener<EEvent>, StoragePolicy>
            {
                virtual void execute() { LOG_INFO("Event", "Execute E System"); }

                virtual void onEvent(const EEvent& e) { LOG_TEST("Event", e.payload); }
            };

            struct F
            {
                int value = 2;
            };

            struct G
            {
                int value = 5;
            };

            struct FGSystem : public System<Own<F>, Own<G>, InitSys, StoragePolicy>
            {
                void init() override
                {
                    auto group = registerGroup<F, G>();

                    group->addOnGroup([this](EntityRef entity) {
                        LOG_TEST("FG System", "Adding in F G group entity id: " << entity.id);
                        auto f = entity.get<F>();
                        EXPECT_EQ(f->value, 2);

                        auto g = entity.get<G>();
                        EXPECT_EQ(g->value, 5);

                        calculate(f, g);
                    });
                }

                void calculate(F* val1, G* val2)
                {
                    EXPECT_EQ(val1->value + val2->value, 7);

                    LOG_TEST("FG System", "Calculated value: " << val1->value + val2->value);
                }
            };

            struct CreateDummy
            {
                int i;
            };

            struct CreateSystem : public System<Own<CreateDummy>>
            {
                CreateSystem(EntitySystem *ecs) : ecsRef(ecs) {}

                virtual void execute()
                {
                    LOG_INFO("Event", "Execute CreateSystem");

                    auto entity = ecsRef->createEntity();

                    LOG_TEST("Create System", "Created entity " << entity->id);
                }

                EntitySystem *ecsRef;
            };

            struct DeleteDummy {};

            struct DeleteTestSystem : public System<Own<DeleteDummy>, StoragePolicy>
            {
                virtual ~DeleteTestSystem() { LOG_TEST("Delete Test System", "System deleted"); }
            };

            struct TaskTestSystem : public System<>
            {
                virtual void execute() { }
            };

            struct ListenDummy {};

            struct ListenTestSystem : public System<Listener<ListenDummy>, StoragePolicy>
            {
                void onEvent(const ListenDummy&) override
                {
                    LOG_TEST("Listen Test System", "Dummy event received");
                }
            };

        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_registry_test, initialization)
        {
            MockLogger logger;

            ComponentRegistry reg(nullptr);

            ASystem  sys1;
            ABSystem sys2;

            sys1.addToRegistry(&reg);
            sys2.addToRegistry(&reg);

            EXPECT_EQ(reg.getTypeId<A>(), 3);
            EXPECT_EQ(reg.getTypeId<B>(), 4);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_registry_test, removing_from_registry)
        {
            MockLogger logger;

            ComponentRegistry reg(nullptr);

            ASystem  sys1;
            ABSystem sys2;

            sys1.addToRegistry(&reg);
            sys2.addToRegistry(&reg);

            EXPECT_EQ(reg.getTypeId<A>(), 3);
            EXPECT_EQ(reg.getTypeId<B>(), 4);

            EXPECT_EQ(reg.componentStorageMapSize(), 2);

            sys2.removeFromRegistry();

            EXPECT_EQ(reg.componentStorageMapSize(), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_registry_test, removing_listener_from_registry)
        {
            MockLogger logger;

            ComponentRegistry reg(nullptr);

            ASystem  sys1;
            ABSystem sys2;
            ListenTestSystem sys3;

            sys1.addToRegistry(&reg);
            sys2.addToRegistry(&reg);
            sys3.addToRegistry(&reg);

            EXPECT_EQ(reg.getTypeId<A>(), 3);
            EXPECT_EQ(reg.getTypeId<B>(), 4);

            EXPECT_EQ(reg.componentStorageMapSize(), 2);
            EXPECT_EQ(reg.eventStorageMapSize<ListenDummy>(), 1);

            sys2.removeFromRegistry();

            EXPECT_EQ(reg.componentStorageMapSize(), 1);
            EXPECT_EQ(reg.eventStorageMapSize<ListenDummy>(), 1);

            sys3.removeFromRegistry();

            EXPECT_EQ(reg.componentStorageMapSize(), 1);
            EXPECT_EQ(reg.eventStorageMapSize<ListenDummy>(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(component_registry_test, multiple_registy)
        {
            MockLogger logger;
            ComponentRegistry reg(nullptr), reg2(nullptr);

            ASystem  sys1;
            ABSystem sys2; // AbSystem sys2bis // Todo here we get a cryptic map_base::at thrown exception when it is cause by A not being own by anything when creating AB <-- make a better compiler error ! or even catch it
            CSystem  sys3(1), sys3bis(1);
            DSystem  sys4;

            sys1.addToRegistry(&reg);
            sys2.addToRegistry(&reg);
            sys3.addToRegistry(&reg);

            sys3bis.addToRegistry(&reg2);
            sys4.addToRegistry(&reg2);

            EXPECT_EQ(reg.getTypeId<A>(), 3);
            EXPECT_EQ(reg.getTypeId<B>(), 4);
            EXPECT_EQ(reg.getTypeId<C>(), 5);

            EXPECT_EQ(reg2.getTypeId<C>(), 3);
            EXPECT_EQ(reg2.getTypeId<D>(), 4);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, fake_start)
        {
            EntitySystem ecs;

            EXPECT_FALSE(ecs.isRunning());

            ecs.fakeStart();

            EXPECT_TRUE(ecs.isRunning());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, execute_once_helper)
        {
            EntitySystem ecs;

            EXPECT_FALSE(ecs.isRunning());

            EXPECT_EQ(ecs.getTotalNbOfExecution(), 0);

            ecs.executeOnce();

            EXPECT_EQ(ecs.getTotalNbOfExecution(), 1);
            EXPECT_FALSE(ecs.isRunning());

            ecs.fakeStart();

            EXPECT_TRUE(ecs.isRunning());
            ecs.executeOnce();
            EXPECT_TRUE(ecs.isRunning());
            EXPECT_EQ(ecs.getTotalNbOfExecution(), 2);

            ecs.stop();

            EXPECT_FALSE(ecs.isRunning());
            ecs.executeOnce();
            EXPECT_FALSE(ecs.isRunning());
            EXPECT_EQ(ecs.getTotalNbOfExecution(), 3);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, single_entity_creation)
        {
            EntitySystem ecs;

            EXPECT_EQ(ecs.getNbEntities(), 0);

            auto entity = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 1);

            ecs.removeEntity(entity);

            EXPECT_EQ(ecs.getNbEntities(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, multiple_single_entity_creation_same_order)
        {
            EntitySystem ecs;

            EXPECT_EQ(ecs.getNbEntities(), 0);

            auto entity0 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 1);

            // Here resize should occur !
            auto entity1 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 2);

            ecs.removeEntity(entity1);

            EXPECT_EQ(ecs.getNbEntities(), 1);

            ecs.removeEntity(entity0);

            EXPECT_EQ(ecs.getNbEntities(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, multiple_single_entity_creation_reverse_order)
        {
            EntitySystem ecs;

            EXPECT_EQ(ecs.getNbEntities(), 0);

            auto entity0 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 1);

            // Here resize should occur !
            auto entity1 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 2);

            ecs.removeEntity(entity0);

            EXPECT_EQ(ecs.getNbEntities(), 1);

            ecs.removeEntity(entity1);

            EXPECT_EQ(ecs.getNbEntities(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, multiple_single_entity_creation_with_deletion_of_the_last)
        {
            EntitySystem ecs;

            auto entity0 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 1);

            auto entity1 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 2);

            auto entity2 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 3);

            // Delete the last entity
            ecs.removeEntity(entity2);

            EXPECT_EQ(ecs.getNbEntities(), 2);

            auto entity2bis = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 3);

            // Here resize should occur !
            auto entity3 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 4);

            ecs.removeEntity(entity0);
            ecs.removeEntity(entity1);
            ecs.removeEntity(entity2bis);
            ecs.removeEntity(entity3);

            EXPECT_EQ(ecs.getNbEntities(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, multiple_single_entity_creation_with_deletion_in_the_middle)
        {
            EntitySystem ecs;

            auto entity0 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 1);

            auto entity1 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 2);

            auto entity2 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 3);

            // Delete an in between entity
            ecs.removeEntity(entity1);

            EXPECT_EQ(ecs.getNbEntities(), 2);

            auto entity1bis = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 3);

            // Here resize should occur !
            auto entity3 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 4);

            const auto& allEntities = ecs.view();

            EXPECT_EQ(allEntities[1]->id, 4);
            EXPECT_EQ(allEntities[2]->id, 6);
            EXPECT_EQ(allEntities[3]->id, 7);
            EXPECT_EQ(allEntities[4]->id, 8);

            ecs.removeEntity(entity0);
            ecs.removeEntity(entity1bis);
            ecs.removeEntity(entity2);
            ecs.removeEntity(entity3);

            EXPECT_EQ(ecs.getNbEntities(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, more_single_entity_creation_with_deletion_in_the_middle)
        {
            EntitySystem ecs;

            auto entity0 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 1);

            auto entity1 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 2);

            auto entity2 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 3);

            // Here resize should occur !
            auto entity3 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 4);

            // Delete an in between entity
            ecs.removeEntity(entity1);

            EXPECT_EQ(ecs.getNbEntities(), 3);

            auto entity1bis = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 4);

            auto entity4 = ecs.createEntity();

            EXPECT_EQ(ecs.getNbEntities(), 5);

            const auto& allEntities = ecs.view();

            EXPECT_EQ(allEntities[1]->id, 4);
            EXPECT_EQ(allEntities[2]->id, 7);
            EXPECT_EQ(allEntities[3]->id, 6);
            EXPECT_EQ(allEntities[4]->id, 8);
            EXPECT_EQ(allEntities[5]->id, 9);

            ecs.removeEntity(entity0);
            ecs.removeEntity(entity1bis);
            ecs.removeEntity(entity2);
            ecs.removeEntity(entity3);
            ecs.removeEntity(entity4);

            EXPECT_EQ(ecs.getNbEntities(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, component_attach_not_running)
        {
            EntitySystem ecs;

            auto sys = ecs.createSystem<ASystem>();

            EXPECT_EQ(ecs.getNbEntities(), 0);

            auto entity = ecs.createEntity();

            EXPECT_TRUE(entity.initialized);

            EXPECT_EQ(ecs.getNbEntities(), 1);

            EXPECT_FALSE(ecs.isRunning());

            auto attachedComp = ecs.attachGeneric<A>(entity, 5, 4);

            EXPECT_EQ(sys->getNbComponents(), 2);

            auto compList = sys->view<A>();

            EXPECT_EQ(compList[0], nullptr);
            EXPECT_EQ(compList[1], attachedComp.component);

            EXPECT_TRUE(attachedComp.initialized);

            EXPECT_EQ(attachedComp->value, 9);

            EXPECT_TRUE(entity.has<A>());

            auto comp = entity->get<A>();

            EXPECT_TRUE(comp.initialized);

            EXPECT_EQ(comp->value, 9);

            EXPECT_EQ(attachedComp.component, comp.component);

            ecs.detach<A>(entity);

            EXPECT_EQ(sys->getNbComponents(), 1);

            EXPECT_FALSE(entity.has<A>());

            ecs.removeEntity(entity);

            EXPECT_EQ(ecs.getNbEntities(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, add_on_group)
        {
            MockLogger logger;

            EntitySystem ecs;

            auto sys = ecs.createSystem<FGSystem>();

            EXPECT_EQ(ecs.getNbEntities(), 0);

            auto entity = ecs.createEntity();

            EXPECT_TRUE(entity.initialized);

            EXPECT_EQ(ecs.getNbEntities(), 1);

            EXPECT_FALSE(ecs.isRunning());

            auto attachedComp = ecs.attachGeneric<F>(entity);

            EXPECT_EQ(sys->view<F>().nbComponents(), 2);

            auto nbGroupComp = sys->viewGroup<F, G>().nbComponents();

            EXPECT_EQ(nbGroupComp, 1);

            EXPECT_TRUE(attachedComp.initialized);

            EXPECT_EQ(attachedComp->value, 2);

            EXPECT_TRUE(entity.has<F>());

            auto comp = entity->get<F>();

            EXPECT_TRUE(comp.initialized);

            EXPECT_EQ(comp->value, 2);

            EXPECT_EQ(attachedComp.component, comp.component);

            auto attachedComp2 = ecs.attachGeneric<G>(entity);

            EXPECT_TRUE(attachedComp2.initialized);

            EXPECT_EQ(attachedComp2->value, 5);

            EXPECT_TRUE(entity.has<G>());

            auto nbGroupComp2 = sys->viewGroup<F, G>().nbComponents();

            EXPECT_EQ(nbGroupComp2, 2);

            auto group = sys->viewGroup<F, G>();

            EXPECT_EQ(group.nbComponents(), 2);

            auto g1 = group[0];
            auto g2 = group[1];

            EXPECT_EQ(g1, nullptr);

            EXPECT_EQ(g2->entityId, entity.id);

            ecs.removeEntity(entity);

            auto nbGroupComp3 = sys->viewGroup<F, G>().nbComponents();

            EXPECT_EQ(nbGroupComp3, 1);

            EXPECT_EQ(ecs.getNbEntities(), 0);

            EXPECT_EQ(ecs.getNbEntities(), 0);

            EXPECT_EQ(logger.getNbTest(),  2);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, delete_ecs_with_system)
        {
            MockLogger logger;

            EntitySystem *ecs = new EntitySystem();

            ecs->createSystem<DeleteTestSystem>();

            delete ecs;

            EXPECT_EQ(logger.getNbTest(), 1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, task_registration)
        {
            MockLogger logger;

            EntitySystem ecs;

            EXPECT_EQ(ecs.getNbSystems(), 0);
            EXPECT_EQ(ecs.getNbTasks(), 0);

            ecs.createSystem<TaskTestSystem>();

            EXPECT_EQ(ecs.getNbSystems(), 1);
            EXPECT_EQ(ecs.getNbTasks(), 1);

            ecs.deleteSystem<TaskTestSystem>();

            EXPECT_EQ(ecs.getNbSystems(), 0);
            EXPECT_EQ(ecs.getNbTasks(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, delete_system)
        {
            MockLogger logger;

            EntitySystem ecs;

            EXPECT_EQ(logger.getNbTest(), 0);
            EXPECT_EQ(ecs.getNbSystems(), 0);
            EXPECT_EQ(ecs.getNbTasks(), 0);

            ecs.createSystem<DeleteTestSystem>();

            EXPECT_EQ(logger.getNbTest(), 0);
            EXPECT_EQ(ecs.getNbSystems(), 1);

            // No task generated as delete test system is a storage one
            EXPECT_EQ(ecs.getNbTasks(), 0);

            ecs.deleteSystem<DeleteTestSystem>();

            EXPECT_EQ(logger.getNbTest(), 1);
            EXPECT_EQ(ecs.getNbSystems(), 0);
            EXPECT_EQ(ecs.getNbTasks(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, delete_system_with_id)
        {
            MockLogger logger;

            EntitySystem ecs;

            EXPECT_EQ(logger.getNbTest(), 0);
            EXPECT_EQ(ecs.getNbSystems(), 0);
            EXPECT_EQ(ecs.getNbTasks(), 0);

            auto sys = ecs.createSystem<DeleteTestSystem>();

            EXPECT_EQ(logger.getNbTest(), 0);
            EXPECT_EQ(ecs.getNbSystems(), 1);
            EXPECT_EQ(ecs.getNbTasks(), 0);

            ecs.deleteSystem(sys->_id);

            EXPECT_EQ(logger.getNbTest(), 1);
            EXPECT_EQ(ecs.getNbSystems(), 0);
            EXPECT_EQ(ecs.getNbTasks(), 0);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, initialization)
        {
            constexpr size_t nbComps = 1000;

            MockLogger<FileSink> logger;

            auto start = std::chrono::steady_clock::now();

            EntitySystem ecs;

            auto end = std::chrono::steady_clock::now();

            auto system = ecs.createSystem<ASystem>();

            auto system2 = ecs.createSystem<ABSystem>();
            auto system3 = ecs.createSystem<CSystem>(nbComps / 50);

            auto entity1 = ecs.createEntity();
            auto entity2 = ecs.createEntity();
            auto entity3 = ecs.createEntity();

            system->createComponent<A>(entity1, 2, 5);
            system2->createRefferedComponent<A>(entity2, 10, 5);
            system2->createOwnedComponent<B>(entity3, 12, 4);

            std::vector<EntityRef> entities;
            entities.reserve(nbComps + 1);

            start = std::chrono::steady_clock::now();

            for (size_t i = 0; i < nbComps + 1; i++)
            {
                entities.emplace_back(ecs.createEntity());
                system->createOwnedComponent<A>(entities[i], i, 15);
            }

            end = std::chrono::steady_clock::now();

            start = std::chrono::steady_clock::now();

            for (size_t i = 0; i < nbComps + 1; i++)
            {
                system3->createOwnedComponent<C>(entities[i], entities[i]->id, "Value of: " + std::to_string(i));
            }

            end = std::chrono::steady_clock::now();

            start = std::chrono::steady_clock::now();
            system3->execute();
            system3->execute();
            system3->execute();
            system3->execute();
            end = std::chrono::steady_clock::now();

            LOG_TEST("ECS", "System C execution took: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << " ns");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, basic_event_handling)
        {
            MockLogger logger;

            EntitySystem ecs;

            ecs.createSystem<ESystem>();

            EEvent event {"New E event !"};

            ecs.sendEvent(event);

            EXPECT_EQ(logger.getNbTest(),  1);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, entity_creation_during_runtime)
        {
            MockLogger logger;

            EntitySystem ecs;

            ecs.createSystem<CreateSystem>(&ecs);

            EXPECT_EQ(ecs.getNbEntities(),  0);

            ecs.executeOnce();

            EXPECT_EQ(ecs.getNbEntities(),  0);

            ecs.executeOnce();

            EXPECT_EQ(ecs.getNbEntities(),  1);
        }

        TEST(system_test, system_perf_owned)
        {
            EntitySystem ecs;

            auto system = ecs.createSystem<ASystem>();

            auto entity = ecs.createEntity();

            for (size_t i = 20; i < NUMBEROFENTITYTRIES; i++)
            {
                system->createOwnedComponent<A>(entity, i, 15);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, system_perf_create)
        {
            EntitySystem ecs;

            auto system = ecs.createSystem<ASystem>();

            auto entity = ecs.createEntity();

            for (size_t i = 20; i < NUMBEROFENTITYTRIES; i++)
            {
                system->createComponent<A>(entity, i, 15);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, system_reattach)
        {
            EntitySystem ecs;

            ecs.createSystem<ASystem>();

            auto entity = ecs.createEntity();

            for (size_t i = 0; i < NUMBEROFENTITYTRIES; i++)
            {
                ecs.attachGeneric<A>(entity, static_cast<int>(i), 15);
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(system_test, entity_serialization)
        {
            // Todo !
            EntitySystem ecs;

            // ecs.createSystem<ASystem>();

            // auto entity = ecs.createEntity();

            // for (size_t i = 20; i < NUMBEROFENTITYTRIES; i++)
            // {
            //     ecs.attach<A>(entity, static_cast<int>(i), 15);
            // }
        }
    }
}