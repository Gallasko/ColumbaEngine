#include "stdafx.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Core/content.h"
#include "Core/tuning.h"

#include "voxelserializer.h"   // examples/VoxelTest

using namespace arena;
namespace fs = std::filesystem;

namespace
{
    std::string allMessages(const ContentReport& r)
    {
        std::string out;
        for (const auto& e : r.errors)
            out += "error " + e.str() + "\n";
        for (const auto& w : r.warnings)
            out += "warning " + w.str() + "\n";
        return out;
    }

    /** A throwaway mods folder. */
    struct TempMods
    {
        fs::path root;

        TempMods()
        {
            root = fs::temp_directory_path() / ("voxelarena-test-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
            fs::remove_all(root);
            fs::create_directories(root);
        }

        ~TempMods() { std::error_code ec; fs::remove_all(root, ec); }

        void write(const std::string& rel, const std::string& text)
        {
            const fs::path p = root / rel;
            fs::create_directories(p.parent_path());
            std::ofstream(p) << text;
        }

        /** The smallest mod that loads: a block, an arena, a tuning and one weapon. */
        void minimalBase()
        {
            write("base/mod.json", R"({ "id": "base", "api": 1 })");
            write("base/blocks.json", R"({ "stone": { "color": [100, 100, 100] } })");
            write("base/tuning.json", R"({ "player": { "walkSpeed": 4 } })");
            write("base/arena.json", R"({ "size": [8, 8, 8], "shapes": [ { "block": "stone", "from": [0,0,0], "to": [7,0,7] } ] })");
            write("base/weapons/stick.json", R"({ "moveset": { "light": "poke", "attacks": { "poke": { "startup": 2, "active": 2, "recovery": 2 } } } })");
        }

        std::string str() const { return root.string(); }
    };
}

TEST(arena_content, the_shipped_mods_load_without_errors)
{
    const Content c = loadContent(VOXELARENA_MODS_DIR);

    EXPECT_TRUE(c.report.errors.empty()) << allMessages(c.report);
    EXPECT_TRUE(c.report.warnings.empty()) << allMessages(c.report);

    ASSERT_GE(c.mods.size(), 2u);
    EXPECT_EQ(c.mods[0].id, "base");
    EXPECT_EQ(c.mods[1].id, "testmod");

    EXPECT_TRUE(c.weapons.count("base:sword"));
    EXPECT_TRUE(c.weapons.count("testmod:spear"));
    ASSERT_EQ(c.weaponOrder.size(), 2u);
    EXPECT_EQ(c.weaponOrder[0], "base:sword");

    EXPECT_TRUE(c.enemies.count("base:brute"));
    EXPECT_TRUE(c.enemies.count("base:dummy"));
    EXPECT_TRUE(c.scripts.count("base:brute"));
    EXPECT_TRUE(c.models.count("base:player"));
    EXPECT_TRUE(c.models.count("testmod:spear"));

    EXPECT_NE(c.block("base:stone"), AIR);
    EXPECT_EQ(c.blockTable.size(), c.blocks.size());
    EXPECT_FALSE(c.blockTable[AIR].solid);

    // The sword's combo is linked through the data
    const auto& sword = c.weapons.at("base:sword").moveset;
    ASSERT_TRUE(sword.find("light1"));
    EXPECT_EQ(sword.find("light1")->nextLight, "light2");
    EXPECT_EQ(sword.find("light2")->nextLight, "light3");
    EXPECT_TRUE(sword.find("light3")->nextLight.empty());

    const auto& spear = c.weapons.at("testmod:spear").moveset;
    EXPECT_EQ(spear.light, "thrust1");
    EXPECT_GT(spear.find("thrust1")->hitboxes[0].size.z, 2.0f);
}

TEST(arena_content, the_shipped_tuning_has_every_key)
{
    Content c = loadContent(VOXELARENA_MODS_DIR);
    ContentReport report;
    const Tuning t = readTuning(c.tuning, c.tuningFile, report);

    EXPECT_TRUE(report.errors.empty()) << allMessages(report);
    EXPECT_TRUE(report.warnings.empty()) << allMessages(report);
    EXPECT_FLOAT_EQ(t.player.walkSpeed, 5.6f);
    EXPECT_EQ(t.dodge.frames, 24);
    EXPECT_EQ(t.startWeapon, "base:sword");
}

TEST(arena_content, models_keep_their_cells_and_find_their_feet)
{
    VoxelModel m;
    ContentReport r;
    ASSERT_TRUE(loadVoxelModel(std::string(VOXELARENA_MODS_DIR) + "/base/models/player.vxl.json", m, r)) << allMessages(r);
    EXPECT_EQ(m.grid.width(), 7);
    EXPECT_EQ(m.grid.height(), 15);
    EXPECT_FLOAT_EQ(m.pivot.y, 0.0f);
    EXPECT_FLOAT_EQ(m.pivot.x, 3.5f);
    EXPECT_NE(m.grid.get(3, 12, 2), AIR);   // the head
}

TEST(arena_content, a_mod_without_its_dependency_is_skipped)
{
    TempMods mods;
    mods.minimalBase();
    mods.write("addon/mod.json", R"({ "id": "addon", "api": 1, "dependencies": ["missing"] })");

    const Content c = loadContent(mods.str());

    ASSERT_EQ(c.mods.size(), 1u);
    EXPECT_EQ(c.mods[0].id, "base");
    ASSERT_EQ(c.report.errors.size(), 1u);
    EXPECT_NE(c.report.errors[0].text.find("missing"), std::string::npos);
}

TEST(arena_content, dependencies_load_first_whatever_their_name)
{
    TempMods mods;
    mods.minimalBase();
    mods.write("aaa/mod.json", R"({ "id": "aaa", "api": 1, "dependencies": ["zzz"] })");
    mods.write("zzz/mod.json", R"({ "id": "zzz", "api": 1, "dependencies": ["base"] })");

    const Content c = loadContent(mods.str());

    ASSERT_EQ(c.mods.size(), 3u);
    EXPECT_EQ(c.mods[0].id, "base");
    EXPECT_EQ(c.mods[1].id, "zzz");
    EXPECT_EQ(c.mods[2].id, "aaa");
}

TEST(arena_content, a_cycle_is_reported_not_hung_on)
{
    TempMods mods;
    mods.minimalBase();
    mods.write("a/mod.json", R"({ "id": "a", "api": 1, "dependencies": ["b"] })");
    mods.write("b/mod.json", R"({ "id": "b", "api": 1, "dependencies": ["a"] })");

    const Content c = loadContent(mods.str());

    EXPECT_EQ(c.mods.size(), 1u);
    EXPECT_EQ(c.report.errors.size(), 2u);
}

TEST(arena_content, a_later_mod_replaces_or_patches_a_definition)
{
    TempMods mods;
    mods.minimalBase();
    mods.write("addon/mod.json", R"({ "id": "addon", "api": 1, "dependencies": ["base"] })");
    // Replace the stick outright
    mods.write("addon/weapons/stick.json", R"({ "id": "base:stick", "name": "Better stick", "moveset": { "light": "jab", "attacks": { "jab": { "damage": 50 } } } })");
    // Patch the tuning and a block
    mods.write("addon/patches/tweaks.json", R"([
        { "kind": "tuning", "set": { "player": { "walkSpeed": 9 } } },
        { "kind": "block", "target": "base:stone", "set": { "color": [255, 0, 0] } }
    ])");

    const Content c = loadContent(mods.str());
    EXPECT_TRUE(c.report.errors.empty()) << allMessages(c.report);

    const auto& stick = c.weapons.at("base:stick");
    EXPECT_EQ(stick.name, "Better stick");
    EXPECT_FLOAT_EQ(stick.moveset.find("jab")->damage, 50.0f);
    EXPECT_FALSE(stick.moveset.find("poke"));

    EXPECT_DOUBLE_EQ(c.tuning.find("player")->num("walkSpeed", 0), 9.0);
    EXPECT_FLOAT_EQ(c.blockTable[c.block("base:stone")].color.r, 1.0f);
}

TEST(arena_content, errors_name_the_file_and_line)
{
    TempMods mods;
    mods.minimalBase();
    mods.write("base/weapons/broken.json", "{\n  \"moveset\": {\n    \"light\": \"a\",\n    \"attacks\": { \"a\": { \"startup\": \"fast\" } }\n  }\n}");
    mods.write("base/weapons/typo.json", "{\n  \"moveset\": { \"light\": \"a\", \"attacks\": { \"a\": {\n    \"startpu\": 3\n  } } }\n}");
    mods.write("base/weapons/syntax.json", "{\n  \"moveset\": {\n    \"light\": \"a\"\n    \"attacks\": {}\n  }\n}");

    const Content c = loadContent(mods.str());

    bool typeError = false, syntaxError = false, typoWarning = false;

    for (const auto& e : c.report.errors)
    {
        if (e.file.find("broken.json") != std::string::npos and e.line == 4 and e.text.find("startup") != std::string::npos)
            typeError = true;
        if (e.file.find("syntax.json") != std::string::npos and e.line == 4)
            syntaxError = true;
    }

    for (const auto& w : c.report.warnings)
    {
        if (w.file.find("typo.json") != std::string::npos and w.line == 3 and w.text.find("startpu") != std::string::npos)
            typoWarning = true;
    }

    EXPECT_TRUE(typeError) << allMessages(c.report);
    EXPECT_TRUE(syntaxError) << allMessages(c.report);
    EXPECT_TRUE(typoWarning) << allMessages(c.report);

    // The broken weapon is left out, the good one still loads
    EXPECT_FALSE(c.weapons.count("base:broken"));
    EXPECT_TRUE(c.weapons.count("base:stick"));
}

TEST(arena_content, a_combo_link_to_a_missing_attack_is_an_error)
{
    TempMods mods;
    mods.minimalBase();
    mods.write("base/weapons/bad.json", R"({ "moveset": { "light": "a", "attacks": { "a": { "next": { "light": "b" } } } } })");

    const Content c = loadContent(mods.str());
    EXPECT_FALSE(c.weapons.count("base:bad"));
    ASSERT_FALSE(c.report.errors.empty());
    EXPECT_NE(c.report.errors[0].text.find("\"b\""), std::string::npos);
}

TEST(arena_content, ids_are_namespaced)
{
    EXPECT_EQ(resolveId("stone", "base"), "base:stone");
    EXPECT_EQ(resolveId("other:stone", "base"), "other:stone");
    EXPECT_EQ(splitId("mymod:lava_sword").first, "mymod");
    EXPECT_EQ(splitId("mymod:lava_sword").second, "lava_sword");
}

TEST(arena_content, the_voxeltest_editor_reads_every_shipped_model)
{
    // The models are made to be edited in examples/VoxelTest: its own loader must read them
    const Content c = loadContent(VOXELARENA_MODS_DIR);
    ASSERT_FALSE(c.models.empty());

    for (const auto& [id, model] : c.models)
    {
        pg::ProjectData project;
        ASSERT_TRUE(pg::loadProjectFromFile(model->file, project)) << model->file;

        EXPECT_EQ(project.W, model->grid.width()) << id;
        EXPECT_EQ(project.H, model->grid.height()) << id;
        EXPECT_EQ(project.D, model->grid.depth()) << id;
        EXPECT_EQ(project.palette.size() + 1, model->table.size()) << id;

        size_t cells = 0;
        for (auto b : model->grid.raw())
            cells += b != AIR;

        EXPECT_EQ(project.cells.size(), cells) << id;

        for (const auto& cell : project.cells)
            EXPECT_EQ(model->grid.get(cell.pos.x, cell.pos.y, cell.pos.z), cell.colorIndex + 1) << id;
    }
}
