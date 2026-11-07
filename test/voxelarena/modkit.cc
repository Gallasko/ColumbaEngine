#include "stdafx.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "Core/content.h"
#include "Core/modkit.h"
#include "Core/schema.h"

using namespace arena;
namespace fs = std::filesystem;

namespace
{
    std::string messages(const std::vector<ContentMessage>& list)
    {
        std::string out;
        for (const auto& m : list)
            out += m.str() + "\n";
        return out;
    }

    std::string readAll(const fs::path& path)
    {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    /** A player's mods folder next to the game's, with a mod of their own in it. */
    struct PlayerMods
    {
        fs::path root;

        PlayerMods()
        {
            root = fs::temp_directory_path() / ("voxelarena-modkit-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
            fs::remove_all(root);
            fs::create_directories(root / "mods");
        }

        ~PlayerMods() { std::error_code ec; fs::remove_all(root, ec); }

        ModSources sources() const { return ModSources{{VOXELARENA_MODS_DIR, (root / "mods").string()}, (root / "mods.json").string()}; }

        Content load() const { return loadContent(sources()); }
    };

    const ModInfo& modOf(const Content& c, const std::string& id)
    {
        const ModInfo* m = c.mod(id);
        EXPECT_TRUE(m) << id << " is not loaded";
        return *m;
    }
}

TEST(arena_modkit, every_shipped_definition_copies_into_a_mod_that_loads)
{
    PlayerMods p;
    ASSERT_TRUE(modkit::newMod((p.root / "mods").string(), "copies", "Copies").ok);
    {
        // It needs the other shipped mods too, to copy theirs
        std::ofstream((p.root / "mods" / "copies" / "mod.json")) << R"({ "id": "copies", "api": 1, "dependencies": ["base", "testmod", "wildfire"] })";
    }

    Content c = p.load();
    ASSERT_TRUE(c.report.errors.empty()) << messages(c.report.errors);

    std::vector<std::string> copied;

    for (const auto& kind : modkit::kinds())
    {
        if (not c.raw.count(kind.raw))
            continue;

        for (const auto& [id, def] : c.raw.at(kind.raw))
        {
            const auto [ns, name] = splitId(id);
            const auto done = modkit::addDefinition(c, modOf(c, "copies"), kind, ns + "_" + name, id);
            EXPECT_TRUE(done.ok) << id << ": " << done.message;
            copied.push_back(std::string(kind.raw) + " copies:" + ns + "_" + name);
        }
    }

    EXPECT_GT(copied.size(), 100u);

    // Every copy loads as its original did, pointing at the same models, scripts, statuses...
    const Content again = p.load();
    const auto problems = modkit::check(again, again.mod("copies"));
    EXPECT_TRUE(problems.errors.empty()) << messages(problems.errors);
    EXPECT_TRUE(problems.warnings.empty()) << messages(problems.warnings);
    EXPECT_TRUE(again.report.errors.empty()) << messages(again.report.errors);

    for (const auto& c2 : copied)
    {
        const std::string raw = c2.substr(0, c2.find(' ')), id = c2.substr(c2.find(' ') + 1);
        EXPECT_TRUE(again.raw.count(raw) and again.raw.at(raw).count(id)) << id;
    }

    EXPECT_EQ(again.enemies.at("copies:base_brute").health, again.enemies.at("base:brute").health);
}

TEST(arena_modkit, a_copy_keeps_its_comments_and_names_what_it_points_at)
{
    const std::string text = "// A sword\n{\n    \"model\": \"sword\", // its model\n    \"moveset\": { \"light\": \"a\", \"attacks\": { \"a\": { \"status\": \"burn\" } } }\n}\n";
    const std::string out = modkit::qualifyIds(text, "weapon", "base");

    EXPECT_EQ(out, "// A sword\n{\n    \"model\": \"base:sword\", // its model\n    \"moveset\": { \"light\": \"a\", \"attacks\": { \"a\": { \"status\": \"base:burn\" } } }\n}\n");
}

TEST(arena_modkit, a_written_patch_loads_and_changes_what_is_uncommented)
{
    PlayerMods p;
    ASSERT_TRUE(modkit::newMod((p.root / "mods").string(), "tweaks", "").ok);

    Content c = p.load();
    const auto done = modkit::writePatch(c, modOf(c, "tweaks"), "brute");
    ASSERT_TRUE(done.ok) << done.message;

    // As written, all in comments: nothing changes
    c = p.load();
    EXPECT_TRUE(modkit::check(c, c.mod("tweaks")).errors.empty()) << messages(modkit::check(c, c.mod("tweaks")).errors);
    const float health = c.enemies.at("base:brute").health;

    std::string text = readAll(done.path);
    const auto at = text.find("// \"health\": ");
    ASSERT_NE(at, std::string::npos) << text;
    text.replace(at, text.find(',', at) - at, "\"health\": 999");
    std::ofstream(done.path) << text;

    c = p.load();
    EXPECT_TRUE(c.report.errors.empty()) << messages(c.report.errors);
    EXPECT_NE(health, 999.0f);
    EXPECT_EQ(c.enemies.at("base:brute").health, 999.0f);
}

TEST(arena_modkit, an_entry_goes_into_a_file_that_has_some_already)
{
    PlayerMods p;
    ASSERT_TRUE(modkit::newMod((p.root / "mods").string(), "blocky", "").ok);
    std::ofstream(p.root / "mods" / "blocky" / "blocks.json") << "// My blocks\n{\n    \"glow\": { \"color\": [250, 240, 120] }\n    // more to come\n}\n";

    Content c = p.load();
    const auto done = modkit::addDefinition(c, modOf(c, "blocky"), *modkit::findKind("blocks"), "slab", "stone");
    ASSERT_TRUE(done.ok) << done.message;

    c = p.load();
    EXPECT_TRUE(c.report.errors.empty()) << messages(c.report.errors);
    EXPECT_TRUE(c.blockIds.count("blocky:glow"));
    EXPECT_TRUE(c.blockIds.count("blocky:slab"));
    EXPECT_FALSE(modkit::addDefinition(c, modOf(c, "blocky"), *modkit::findKind("block"), "slab", "stone").ok);
    EXPECT_FALSE(modkit::addDefinition(c, modOf(c, "blocky"), *modkit::findKind("enemy"), "rock", "stone").ok);
}

TEST(arena_modkit, the_tool_finds_files_the_game_never_reads)
{
    PlayerMods p;
    ASSERT_TRUE(modkit::newMod((p.root / "mods").string(), "typos", "").ok);
    fs::create_directories(p.root / "mods" / "typos" / "weapon");
    std::ofstream(p.root / "mods" / "typos" / "weapon" / "axe.json") << "{}";

    const Content c = p.load();
    const auto problems = modkit::check(c, c.mod("typos"));
    ASSERT_EQ(problems.warnings.size(), 1u) << messages(problems.warnings);
    EXPECT_NE(problems.warnings[0].text.find("does not read"), std::string::npos);
}

TEST(arena_modkit, mods_turn_off_and_on)
{
    PlayerMods p;
    const std::string settings = (p.root / "mods.json").string();

    EXPECT_TRUE(modkit::setModOn(settings, "wildfire", false).ok);
    Content c = p.load();
    EXPECT_FALSE(c.mod("wildfire"));

    EXPECT_TRUE(modkit::setModOn(settings, "wildfire", true).ok);
    c = p.load();
    EXPECT_TRUE(c.mod("wildfire"));
}

TEST(arena_modkit, pretty_json_reads_back_the_same)
{
    for (const auto& entry : fs::recursive_directory_iterator(std::string(VOXELARENA_MODS_DIR) + "/base"))
    {
        if (entry.path().extension() != ".json" or entry.path().string().find(".vxl.") != std::string::npos)
            continue;

        const JsonParseResult before = parseJson(readAll(entry.path()));
        ASSERT_TRUE(before.ok) << entry.path();
        const JsonParseResult after = parseJson(modkit::pretty(before.value));
        ASSERT_TRUE(after.ok) << entry.path() << ": " << after.error;
        EXPECT_EQ(toJsonString(after.value), toJsonString(before.value)) << entry.path();
    }
}

namespace
{
    uint32_t le32(const std::string& s, size_t at)
    {
        return static_cast<uint32_t>(static_cast<unsigned char>(s[at])) | static_cast<uint32_t>(static_cast<unsigned char>(s[at + 1])) << 8 |
               static_cast<uint32_t>(static_cast<unsigned char>(s[at + 2])) << 16 | static_cast<uint32_t>(static_cast<unsigned char>(s[at + 3])) << 24;
    }

    uint32_t le16(const std::string& s, size_t at)
    {
        return static_cast<uint32_t>(static_cast<unsigned char>(s[at])) | static_cast<uint32_t>(static_cast<unsigned char>(s[at + 1])) << 8;
    }
}

TEST(arena_modkit, a_packed_mod_unzips_into_a_mod_that_loads)
{
    PlayerMods p;
    ASSERT_TRUE(modkit::newMod((p.root / "mods").string(), "packed", "Packed").ok);
    Content c = p.load();
    ASSERT_TRUE(modkit::addDefinition(c, modOf(c, "packed"), *modkit::findKind("weapon"), "halberd", "base:sword").ok);
    ASSERT_TRUE(modkit::addDefinition(c, modOf(c, "packed"), *modkit::findKind("block"), "glow", "base:stone").ok);
    std::ofstream(p.root / "mods" / "packed" / ".hidden") << "not shared";

    c = p.load();
    const std::string zip = (p.root / "packed.zip").string();
    const auto done = modkit::pack(c, modOf(c, "packed"), zip);
    ASSERT_TRUE(done.ok) << done.message;

    // Read it back: each stored file, then the table of contents at the end
    const std::string data = readAll(zip);
    std::map<std::string, std::string> files;
    size_t at = 0;

    while (at + 30 <= data.size() and le32(data, at) == 0x04034b50)
    {
        EXPECT_EQ(le16(data, at + 8), 0u);   // stored
        const uint32_t size = le32(data, at + 18);
        const uint32_t nameLength = le16(data, at + 26);
        const std::string name = data.substr(at + 30, nameLength);
        files[name] = data.substr(at + 30 + nameLength + le16(data, at + 28), size);
        at += 30 + nameLength + le16(data, at + 28) + size;
    }

    const size_t end = data.size() - 22;
    ASSERT_EQ(le32(data, end), 0x06054b50u);
    EXPECT_EQ(le16(data, end + 10), files.size());
    EXPECT_EQ(le32(data, end + 16), at);   // the table of contents follows the files

    EXPECT_EQ((std::set<std::string>{"packed/blocks.json", "packed/mod.json", "packed/weapons/halberd.json"}), [&] {
        std::set<std::string> names;
        for (const auto& [name, contents] : files)
            names.insert(name);
        return names;
    }());

    // Unzipped in someone else's mods folder, it loads as it did here
    PlayerMods other;
    for (const auto& [name, contents] : files)
    {
        fs::create_directories((other.root / "mods" / name).parent_path());
        std::ofstream(other.root / "mods" / name, std::ios::binary) << contents;
    }

    const Content theirs = other.load();
    EXPECT_TRUE(theirs.report.errors.empty()) << messages(theirs.report.errors);
    EXPECT_TRUE(theirs.weapons.count("packed:halberd"));
    EXPECT_TRUE(theirs.blockIds.count("packed:glow"));
}

TEST(arena_modkit, a_mod_with_a_mistake_is_not_packed)
{
    PlayerMods p;
    ASSERT_TRUE(modkit::newMod((p.root / "mods").string(), "broken", "").ok);
    std::ofstream(p.root / "mods" / "broken" / "blocks.json") << "{ \"glow\": { \"color\": [1, 2, 3] ";

    const Content c = p.load();
    const auto done = modkit::pack(c, modOf(c, "broken"), (p.root / "broken.zip").string());
    EXPECT_FALSE(done.ok);
    EXPECT_NE(done.message.find("mistake"), std::string::npos) << done.message;
    EXPECT_FALSE(fs::exists(p.root / "broken.zip"));
}

TEST(arena_modkit, the_guide_has_every_object_and_its_links_lead_somewhere)
{
    const Content c = loadContent(VOXELARENA_MODS_DIR);
    const std::string page = modkit::guide(c);

    std::set<std::string> anchors;
    for (size_t at = page.find(" id=\""); at != std::string::npos; at = page.find(" id=\"", at + 1))
        anchors.insert(page.substr(at + 5, page.find('"', at + 5) - at - 5));

    for (const auto& o : schema::objects())
        EXPECT_TRUE(anchors.count("o-" + std::string(o.name))) << o.name;

    for (size_t at = page.find("href=\"#"); at != std::string::npos; at = page.find("href=\"#", at + 1))
    {
        const std::string target = page.substr(at + 7, page.find('"', at + 7) - at - 7);
        EXPECT_TRUE(anchors.count(target)) << "a link to #" << target;
    }

    EXPECT_NE(page.find("<code>base:sword</code>"), std::string::npos);
    EXPECT_NE(page.find("<code>player.walkSpeed</code>"), std::string::npos);
    EXPECT_EQ(page.find("<script"), std::string::npos);
}
