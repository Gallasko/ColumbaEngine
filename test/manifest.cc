#include "stdafx.h"

#include <filesystem>
namespace fs = std::filesystem;

#include "gtest/gtest.h"

#include "Versioning/manifest.h"
#include "ECS/savemanager.h"
#include "Files/filemanager.h"

#include "mocklogger.h"

namespace pg
{
    namespace test
    {
        namespace
        {
            const std::string TEMP_MANIFEST = "tmpManifestTest.json";

            void writeManifestFile(const std::string& content)
            {
                TextFile file;
                file.filepath = TEMP_MANIFEST;
                UniversalFileAccessor::writeToFile(file, content, true);
            }

            void cleanupManifestFile()
            {
                fs::remove(TEMP_MANIFEST);
            }
        }

        // ========================================================================================
        // ========================    Manifest Loading Tests    ===================================
        // ========================================================================================

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, default_construction)
        {
            Manifest m;

            EXPECT_FALSE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(1, 0, 0));
            EXPECT_EQ(m.getDescription(), "");
            EXPECT_TRUE(m.getChangelog().empty());
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_valid_full_manifest)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({
                "version": "2.3.1",
                "description": "Test game",
                "changelog": [
                    {
                        "version": "2.3.1",
                        "changes": ["Fixed crash", "Improved performance"]
                    },
                    {
                        "version": "2.0.0",
                        "changes": ["Major rewrite"]
                    }
                ]
            })");

            Manifest m;
            bool result = m.loadFromFile(TEMP_MANIFEST);

            EXPECT_TRUE(result);
            EXPECT_TRUE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(2, 3, 1));
            EXPECT_EQ(m.getDescription(), "Test game");
            EXPECT_EQ(m.getChangelog().size(), 2u);
            EXPECT_EQ(m.getChangelog()[0].version, "2.3.1");
            EXPECT_EQ(m.getChangelog()[0].changes.size(), 2u);
            EXPECT_EQ(m.getChangelog()[0].changes[0], "Fixed crash");
            EXPECT_EQ(m.getChangelog()[1].version, "2.0.0");
            EXPECT_EQ(m.getChangelog()[1].changes[0], "Major rewrite");

            EXPECT_EQ(logger.getNbError(), 0u);
            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_missing_file)
        {
            MockLogger logger;

            Manifest m;
            bool result = m.loadFromFile("does_not_exist.json");

            EXPECT_FALSE(result);
            EXPECT_FALSE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(1, 0, 0));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_empty_file)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile("");

            Manifest m;
            bool result = m.loadFromFile(TEMP_MANIFEST);

            EXPECT_FALSE(result);
            EXPECT_FALSE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(1, 0, 0));

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_malformed_json)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile("{ this is not valid json }");

            Manifest m;
            bool result = m.loadFromFile(TEMP_MANIFEST);

            EXPECT_FALSE(result);
            EXPECT_FALSE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(1, 0, 0));
            EXPECT_GT(logger.getNbError(), 0u);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_missing_version_field)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"description": "no version here"})");

            Manifest m;
            bool result = m.loadFromFile(TEMP_MANIFEST);

            EXPECT_TRUE(result);
            EXPECT_TRUE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(1, 0, 0));
            EXPECT_EQ(m.getDescription(), "no version here");

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_invalid_version_string)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "not_a_version"})");

            Manifest m;
            bool result = m.loadFromFile(TEMP_MANIFEST);

            EXPECT_FALSE(result);
            EXPECT_FALSE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(1, 0, 0));

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_missing_changelog)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "1.0.0", "description": "test"})");

            Manifest m;
            bool result = m.loadFromFile(TEMP_MANIFEST);

            EXPECT_TRUE(result);
            EXPECT_TRUE(m.isLoaded());
            EXPECT_TRUE(m.getChangelog().empty());

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_overwrites_previous)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "1.0.0", "description": "first"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            EXPECT_EQ(m.getDescription(), "first");

            writeManifestFile(R"({"version": "2.0.0", "description": "second"})");
            m.loadFromFile(TEMP_MANIFEST);

            EXPECT_EQ(m.getVersion(), SemanticVersion(2, 0, 0));
            EXPECT_EQ(m.getDescription(), "second");

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, load_failure_resets_after_previous_load)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "3.0.0", "description": "loaded"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);
            EXPECT_TRUE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(3, 0, 0));

            cleanupManifestFile();

            bool result = m.loadFromFile("does_not_exist.json");

            EXPECT_FALSE(result);
            EXPECT_FALSE(m.isLoaded());
            EXPECT_EQ(m.getVersion(), SemanticVersion(1, 0, 0));
            EXPECT_TRUE(m.getChangelog().empty());
        }

        // ========================================================================================
        // ========================    Version Check Tests    =====================================
        // ========================================================================================

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_new_install)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion(SemanticVersion(0, 0, 0));

            EXPECT_TRUE(result.isNewInstall);
            EXPECT_FALSE(result.isMajorBump);
            EXPECT_FALSE(result.isMinorBump);
            EXPECT_FALSE(result.isPatchBump);
            EXPECT_FALSE(result.isSameVersion);
            EXPECT_FALSE(result.isDowngrade);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_new_install_empty_string)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion("");

            EXPECT_TRUE(result.isNewInstall);
            EXPECT_EQ(result.oldVersion, SemanticVersion(0, 0, 0));
            EXPECT_EQ(result.newVersion, SemanticVersion(2, 0, 0));

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_same)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion(SemanticVersion(2, 0, 0));

            EXPECT_TRUE(result.isSameVersion);
            EXPECT_FALSE(result.isNewInstall);
            EXPECT_FALSE(result.isMajorBump);
            EXPECT_FALSE(result.isMinorBump);
            EXPECT_FALSE(result.isPatchBump);
            EXPECT_FALSE(result.isDowngrade);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_major_bump)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion(SemanticVersion(1, 5, 3));

            EXPECT_TRUE(result.isMajorBump);
            EXPECT_FALSE(result.isMinorBump);
            EXPECT_FALSE(result.isPatchBump);
            EXPECT_FALSE(result.isSameVersion);
            EXPECT_FALSE(result.isNewInstall);
            EXPECT_FALSE(result.isDowngrade);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_minor_bump)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "1.3.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion(SemanticVersion(1, 2, 0));

            EXPECT_TRUE(result.isMinorBump);
            EXPECT_FALSE(result.isMajorBump);
            EXPECT_FALSE(result.isPatchBump);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_patch_bump)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "1.2.5"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion(SemanticVersion(1, 2, 3));

            EXPECT_TRUE(result.isPatchBump);
            EXPECT_FALSE(result.isMajorBump);
            EXPECT_FALSE(result.isMinorBump);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_downgrade)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "1.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion(SemanticVersion(2, 0, 0));

            EXPECT_TRUE(result.isDowngrade);
            EXPECT_FALSE(result.isMajorBump);
            EXPECT_FALSE(result.isMinorBump);
            EXPECT_FALSE(result.isPatchBump);
            EXPECT_FALSE(result.isSameVersion);
            EXPECT_FALSE(result.isNewInstall);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, check_version_relevant_changelog)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({
                "version": "2.0.0",
                "changelog": [
                    {"version": "1.0.0", "changes": ["Initial"]},
                    {"version": "1.1.0", "changes": ["Feature A"]},
                    {"version": "1.2.0", "changes": ["Feature B"]},
                    {"version": "2.0.0", "changes": ["Major update"]}
                ]
            })");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.checkVersion(SemanticVersion(1, 1, 0));

            // Entries strictly greater than 1.1.0 and up to 2.0.0
            EXPECT_EQ(result.relevantChangelog.size(), 2u);
            EXPECT_EQ(result.relevantChangelog[0].version, "1.2.0");
            EXPECT_EQ(result.relevantChangelog[1].version, "2.0.0");

            cleanupManifestFile();
        }

        // ========================================================================================
        // ========================    Changelog Query Tests    ====================================
        // ========================================================================================

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, get_changelog_for_version_found)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({
                "version": "2.0.0",
                "changelog": [
                    {"version": "1.1.0", "changes": ["Feature A", "Feature B"]},
                    {"version": "2.0.0", "changes": ["Major update"]}
                ]
            })");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto* entry = m.getChangelogForVersion(SemanticVersion(1, 1, 0));

            ASSERT_NE(entry, nullptr);
            EXPECT_EQ(entry->version, "1.1.0");
            EXPECT_EQ(entry->changes.size(), 2u);
            EXPECT_EQ(entry->changes[0], "Feature A");

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, get_changelog_for_version_not_found)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({
                "version": "2.0.0",
                "changelog": [
                    {"version": "1.1.0", "changes": ["Feature A"]}
                ]
            })");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto* entry = m.getChangelogForVersion(SemanticVersion(9, 9, 9));

            EXPECT_EQ(entry, nullptr);

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, get_changelog_between_full_range)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({
                "version": "2.0.0",
                "changelog": [
                    {"version": "1.0.0", "changes": ["Initial"]},
                    {"version": "1.1.0", "changes": ["Feature A"]},
                    {"version": "1.2.0", "changes": ["Feature B"]},
                    {"version": "2.0.0", "changes": ["Major"]}
                ]
            })");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.getChangelogBetween(SemanticVersion(1, 0, 0), SemanticVersion(2, 0, 0));

            // 1.0.0 excluded (strict >), 1.1.0, 1.2.0, 2.0.0 included
            EXPECT_EQ(result.size(), 3u);
            EXPECT_EQ(result[0].version, "1.1.0");
            EXPECT_EQ(result[1].version, "1.2.0");
            EXPECT_EQ(result[2].version, "2.0.0");

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, get_changelog_between_partial_range)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({
                "version": "2.0.0",
                "changelog": [
                    {"version": "1.0.0", "changes": ["Initial"]},
                    {"version": "1.1.0", "changes": ["Feature A"]},
                    {"version": "1.2.0", "changes": ["Feature B"]},
                    {"version": "2.0.0", "changes": ["Major"]}
                ]
            })");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.getChangelogBetween(SemanticVersion(1, 1, 0), SemanticVersion(1, 2, 0));

            EXPECT_EQ(result.size(), 1u);
            EXPECT_EQ(result[0].version, "1.2.0");

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, get_changelog_between_empty_range)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({
                "version": "2.0.0",
                "changelog": [
                    {"version": "1.0.0", "changes": ["Initial"]},
                    {"version": "2.0.0", "changes": ["Major"]}
                ]
            })");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.getChangelogBetween(SemanticVersion(2, 0, 0), SemanticVersion(3, 0, 0));

            EXPECT_TRUE(result.empty());

            cleanupManifestFile();
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_test, get_changelog_between_is_sorted)
        {
            MockLogger logger;
            cleanupManifestFile();

            // Changelog entries deliberately in reverse order
            writeManifestFile(R"({
                "version": "2.0.0",
                "changelog": [
                    {"version": "2.0.0", "changes": ["Major"]},
                    {"version": "1.2.0", "changes": ["Feature B"]},
                    {"version": "1.1.0", "changes": ["Feature A"]}
                ]
            })");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            auto result = m.getChangelogBetween(SemanticVersion(1, 0, 0), SemanticVersion(2, 0, 0));

            ASSERT_EQ(result.size(), 3u);
            EXPECT_EQ(result[0].version, "1.1.0");
            EXPECT_EQ(result[1].version, "1.2.0");
            EXPECT_EQ(result[2].version, "2.0.0");

            cleanupManifestFile();
        }

        // ========================================================================================
        // ========================    Migration Tests    =========================================
        // ========================================================================================

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, no_migrations_registered)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            SaveManager saveManager("tmpMigrationSave.dat");
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 0u);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, single_callback_in_range)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            bool called = false;
            m.registerMigration(SemanticVersion(1, 5, 0), [&called](SaveManager&) {
                called = true;
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 1u);
            EXPECT_TRUE(called);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, callback_out_of_range_too_old)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            bool called = false;
            m.registerMigration(SemanticVersion(0, 5, 0), [&called](SaveManager&) {
                called = true;
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 0u);
            EXPECT_FALSE(called);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, callback_out_of_range_too_new)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            bool called = false;
            m.registerMigration(SemanticVersion(3, 0, 0), [&called](SaveManager&) {
                called = true;
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 0u);
            EXPECT_FALSE(called);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, callback_at_old_version_excluded)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            bool called = false;
            m.registerMigration(SemanticVersion(1, 0, 0), [&called](SaveManager&) {
                called = true;
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            // targetVersion (1.0.0) is NOT > oldVersion (1.0.0), so it should be skipped
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 0u);
            EXPECT_FALSE(called);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, callback_at_current_version_included)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            bool called = false;
            m.registerMigration(SemanticVersion(2, 0, 0), [&called](SaveManager&) {
                called = true;
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            // targetVersion (2.0.0) <= version (2.0.0), so it should run
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 1u);
            EXPECT_TRUE(called);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, multiple_ordered)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            std::vector<std::string> order;
            m.registerMigration(SemanticVersion(1, 2, 0), [&order](SaveManager&) {
                order.push_back("1.2.0");
            });
            m.registerMigration(SemanticVersion(1, 5, 0), [&order](SaveManager&) {
                order.push_back("1.5.0");
            });
            m.registerMigration(SemanticVersion(1, 1, 0), [&order](SaveManager&) {
                order.push_back("1.1.0");
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 3u);
            // std::map iterates in sorted key order
            ASSERT_EQ(order.size(), 3u);
            EXPECT_EQ(order[0], "1.1.0");
            EXPECT_EQ(order[1], "1.2.0");
            EXPECT_EQ(order[2], "1.5.0");

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, multiple_callbacks_same_version)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            int callCount = 0;
            m.registerMigration(SemanticVersion(1, 5, 0), [&callCount](SaveManager&) {
                callCount++;
            });
            m.registerMigration(SemanticVersion(1, 5, 0), [&callCount](SaveManager&) {
                callCount++;
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 2u);
            EXPECT_EQ(callCount, 2);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(manifest_migration_test, register_migration_string_overload)
        {
            MockLogger logger;
            cleanupManifestFile();

            writeManifestFile(R"({"version": "2.0.0"})");

            Manifest m;
            m.loadFromFile(TEMP_MANIFEST);

            bool called = false;
            m.registerMigration("1.5.0", [&called](SaveManager&) {
                called = true;
            });

            SaveManager saveManager("tmpMigrationSave.dat");
            size_t count = m.runMigrations(SemanticVersion(1, 0, 0), saveManager);

            EXPECT_EQ(count, 1u);
            EXPECT_TRUE(called);

            cleanupManifestFile();
            fs::remove("tmpMigrationSave.dat");
        }
    }
}
