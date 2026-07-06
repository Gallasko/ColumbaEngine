#include "stdafx.h"

#include "gtest/gtest.h"

#include "ECS/entitysystem.h"
#include "ECS/scriptregistry.h"
#include "ECS/scriptwatchersystem.h"

#include "Systems/coresystems.h"

#include <filesystem>
#include <fstream>

namespace pg
{
    namespace test
    {
        namespace
        {
            namespace fs = std::filesystem;

            void writeFile(const fs::path& path, const std::string& content)
            {
                std::ofstream file(path, std::ios::trunc);
                file << content;
            }

            // Force a visible mtime change even on filesystems with coarse timestamps
            void bumpMtime(const fs::path& path)
            {
                auto mtime = fs::last_write_time(path);
                fs::last_write_time(path, mtime + std::chrono::seconds(2));
            }

            class ScriptRegistryTest : public ::testing::Test
            {
            protected:
                void SetUp() override
                {
                    dir = fs::temp_directory_path() / "pg_scriptregistry_test";
                    fs::create_directories(dir);
                }

                void TearDown() override
                {
                    std::error_code ec;
                    fs::remove_all(dir, ec);
                }

                fs::path scriptPath(const std::string& name) const { return dir / name; }

                fs::path dir;
            };
        }

        TEST_F(ScriptRegistryTest, LoadRejectsInvalidExtension)
        {
            EntitySystem ecs;

            auto path = scriptPath("notascript.txt");
            writeFile(path, "var x = 1");

            EXPECT_EQ(ecs.scripts().load(path.string()), nullptr);
            EXPECT_EQ(ecs.scripts().nbWatchedScripts(), 0u);
        }

        TEST_F(ScriptRegistryTest, LoadCompilesPgAndCachesBytecode)
        {
            EntitySystem ecs;

            auto path = scriptPath("simple.pg");
            writeFile(path, "var x = 1 + 2\n");

            auto handle = ecs.scripts().load(path.string());

            ASSERT_NE(handle, nullptr);
            ASSERT_NE(handle->bytecode(), nullptr);
            EXPECT_FALSE(handle->bytecode()->empty());

            EXPECT_EQ(handle->source(), path.string());
            EXPECT_EQ(handle->compiled(), path.string() + "c");

            // The .pgc cache must exist on disk
            EXPECT_TRUE(fs::exists(path.string() + "c"));

            EXPECT_EQ(ecs.scripts().nbWatchedScripts(), 1u);
        }

        TEST_F(ScriptRegistryTest, LoadPrecompiledPgcDirectly)
        {
            auto path = scriptPath("precompiled.pg");
            writeFile(path, "var x = 40 + 2\n");

            // First load generates the .pgc
            {
                EntitySystem ecs;
                ASSERT_NE(ecs.scripts().load(path.string()), nullptr);
            }

            // A fresh ECS loads the .pgc without the source
            EntitySystem ecs2;

            auto handle = ecs2.scripts().load(path.string() + "c");

            ASSERT_NE(handle, nullptr);
            ASSERT_NE(handle->bytecode(), nullptr);
            EXPECT_FALSE(handle->bytecode()->empty());
        }

        TEST_F(ScriptRegistryTest, LoadDedupesSameSource)
        {
            EntitySystem ecs;

            auto path = scriptPath("shared.pg");
            writeFile(path, "var x = 1\n");

            auto first = ecs.scripts().load(path.string());
            auto second = ecs.scripts().load(path.string());

            ASSERT_NE(first, nullptr);
            EXPECT_EQ(first, second);
            EXPECT_EQ(ecs.scripts().nbWatchedScripts(), 1u);

            // Both users see the same bytecode object
            EXPECT_EQ(first->bytecode(), second->bytecode());
        }

        TEST_F(ScriptRegistryTest, ReloadSwapsBytecodeOnlyAfterApply)
        {
            EntitySystem ecs;

            auto path = scriptPath("reload.pg");
            writeFile(path, "var x = 1\n");

            auto handle = ecs.scripts().load(path.string());
            ASSERT_NE(handle, nullptr);

            auto oldCode = handle->bytecode();
            ASSERT_NE(oldCode, nullptr);

            ScriptReloadedEvent received;
            size_t callbackCount = 0;
            ecs.scripts().setReloadCallback([&](const ScriptReloadedEvent& event) {
                received = event;
                callbackCount++;
            });

            // Change the script to something that compiles to different bytecode
            writeFile(path, "var x = 1\nvar y = 2\nvar z = x + y\n");

            EXPECT_TRUE(ecs.scripts().reloadNow(path.string()));
            EXPECT_TRUE(ecs.scripts().hasPendingSwaps());

            // Not applied yet: callers still pin the old version
            EXPECT_EQ(handle->bytecode(), oldCode);

            ecs.scripts().applyPendingSwaps();

            EXPECT_FALSE(ecs.scripts().hasPendingSwaps());

            auto newCode = handle->bytecode();
            ASSERT_NE(newCode, nullptr);
            EXPECT_NE(newCode, oldCode);
            EXPECT_NE(*newCode, *oldCode);

            EXPECT_EQ(callbackCount, 1u);
            EXPECT_TRUE(received.success);
            EXPECT_EQ(received.sourcePath, path.string());
            EXPECT_TRUE(received.error.empty());

            // A pinned copy taken before the swap stays intact (old VM runs are safe)
            EXPECT_FALSE(oldCode->empty());
        }

        TEST_F(ScriptRegistryTest, FailedReloadKeepsOldBytecode)
        {
            EntitySystem ecs;

            auto path = scriptPath("broken.pg");
            writeFile(path, "var x = 1\n");

            auto handle = ecs.scripts().load(path.string());
            ASSERT_NE(handle, nullptr);

            auto oldCode = handle->bytecode();
            ASSERT_NE(oldCode, nullptr);

            ScriptReloadedEvent received;
            ecs.scripts().setReloadCallback([&](const ScriptReloadedEvent& event) { received = event; });

            // Break the script mid-edit
            writeFile(path, ")))((( this is not valid pgscript\n");

            EXPECT_FALSE(ecs.scripts().reloadNow(path.string()));

            ecs.scripts().applyPendingSwaps();

            // The last good version keeps running
            EXPECT_EQ(handle->bytecode(), oldCode);

            EXPECT_FALSE(received.success);
            EXPECT_EQ(received.sourcePath, path.string());
            EXPECT_FALSE(received.error.empty());
        }

        TEST_F(ScriptRegistryTest, ReloadNowOnUnknownScriptFails)
        {
            EntitySystem ecs;

            EXPECT_FALSE(ecs.scripts().reloadNow("never/loaded.pg"));
            EXPECT_FALSE(ecs.scripts().hasPendingSwaps());
        }

        TEST_F(ScriptRegistryTest, PollForChangesDetectsAndDebouncesMtimeChange)
        {
            EntitySystem ecs;

            auto path = scriptPath("watched.pg");
            writeFile(path, "var x = 1\n");

            auto handle = ecs.scripts().load(path.string());
            ASSERT_NE(handle, nullptr);

            auto oldCode = handle->bytecode();

            ecs.scripts().setDebounceMs(0);

            // No change on disk: nothing staged
            ecs.scripts().pollForChanges();
            ecs.scripts().pollForChanges();
            EXPECT_FALSE(ecs.scripts().hasPendingSwaps());

            writeFile(path, "var x = 1\nvar y = 2\nvar z = x + y\n");
            bumpMtime(path);

            // First poll observes the change, second poll sees it stable and reloads
            ecs.scripts().pollForChanges();
            EXPECT_FALSE(ecs.scripts().hasPendingSwaps());

            ecs.scripts().pollForChanges();
            EXPECT_TRUE(ecs.scripts().hasPendingSwaps());

            ecs.scripts().applyPendingSwaps();

            auto newCode = handle->bytecode();
            ASSERT_NE(newCode, nullptr);
            EXPECT_NE(*newCode, *oldCode);

            // A reload resets the watch state: no further swaps without a new edit
            ecs.scripts().pollForChanges();
            ecs.scripts().pollForChanges();
            EXPECT_FALSE(ecs.scripts().hasPendingSwaps());
        }

        TEST_F(ScriptRegistryTest, ExecuteOnceAppliesStagedSwaps)
        {
            EntitySystem ecs;

            auto path = scriptPath("frameswap.pg");
            writeFile(path, "var x = 1\n");

            auto handle = ecs.scripts().load(path.string());
            ASSERT_NE(handle, nullptr);

            auto oldCode = handle->bytecode();

            writeFile(path, "var x = 1\nvar y = 2\nvar z = x + y\n");

            ASSERT_TRUE(ecs.scripts().reloadNow(path.string()));
            ASSERT_TRUE(ecs.scripts().hasPendingSwaps());

            // The BasicTask applies swaps between frames
            ecs.executeOnce();

            EXPECT_FALSE(ecs.scripts().hasPendingSwaps());
            EXPECT_NE(*handle->bytecode(), *oldCode);
        }

        TEST_F(ScriptRegistryTest, WatcherSystemHotReloadsEndToEnd)
        {
            EntitySystem ecs;

            auto* watcher = ecs.createSystem<ScriptWatcherSystem>();
            ASSERT_NE(watcher, nullptr);

            auto path = scriptPath("endtoend.pg");
            writeFile(path, "var x = 1\n");

            auto handle = ecs.scripts().load(path.string());
            ASSERT_NE(handle, nullptr);

            auto oldCode = handle->bytecode();

            ecs.scripts().setDebounceMs(0);

            writeFile(path, "var x = 1\nvar y = 2\nvar z = x + y\n");
            bumpMtime(path);

            // Each tick exceeds the poll interval, so every frame runs one poll:
            // frame 1 observes the change, frame 2 debounces + stages the swap,
            // frame 3's BasicTask applies it
            for (size_t i = 0; i < 3; i++)
            {
                ecs.sendEvent(TickEvent{500.0f});
                ecs.executeOnce();
            }

            auto newCode = handle->bytecode();
            ASSERT_NE(newCode, nullptr);
            EXPECT_NE(*newCode, *oldCode);
        }
    }
}
