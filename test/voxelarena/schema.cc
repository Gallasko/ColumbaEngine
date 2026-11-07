#include "stdafx.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "Core/schema.h"

using namespace arena;
using namespace arena::schema;
namespace fs = std::filesystem;

namespace
{
    std::string problems(const std::vector<Problem>& list)
    {
        std::string out;
        for (const auto& p : list)
            out += std::to_string(p.line) + ": " + p.text + "\n";
        return out;
    }

    std::vector<Problem> check(const std::string& text, const std::string& type)
    {
        const JsonParseResult parsed = parseJson(text);
        EXPECT_TRUE(parsed.ok) << parsed.error;
        std::vector<Problem> out;
        validate(parsed.value, type, "it", out);
        return out;
    }

    // "weapons/*.json" against "weapons/sword.json"; a pattern without * names one file
    bool matches(const std::string& pattern, const std::string& path)
    {
        const auto star = pattern.find('*');

        if (star == std::string::npos)
            return pattern == path;

        const std::string before = pattern.substr(0, star), after = pattern.substr(star + 1);
        return path.size() > before.size() + after.size() and path.compare(0, before.size(), before) == 0 and
               path.compare(path.size() - after.size(), after.size(), after) == 0 and
               path.find('/', before.size()) == std::string::npos;
    }
}

TEST(arena_schema, every_type_of_the_table_reads)
{
    std::set<std::string> names;

    for (const auto& o : objects())
    {
        EXPECT_TRUE(names.insert(o.name).second) << "two objects named " << o.name;
        EXPECT_NE(std::string(o.title), "") << o.name;
        EXPECT_NE(std::string(o.text), "") << o.name;

        std::set<std::string> keys;

        for (const auto& f : o.fields)
        {
            EXPECT_TRUE(keys.insert(f.name).second) << o.name << " has " << f.name << " twice";
            EXPECT_NE(std::string(f.text), "") << o.name << "." << f.name;

            Type t;
            std::string error;
            EXPECT_TRUE(parseType(f.type, t, error)) << o.name << "." << f.name << ": " << f.type << ": " << error;
        }
    }

    for (const auto& f : files())
    {
        Type t;
        std::string error;
        EXPECT_TRUE(std::string(f.type).empty() or parseType(f.type, t, error)) << f.path << ": " << error;
    }
}

TEST(arena_schema, the_notation_reads_and_says_what_is_wrong)
{
    Type t;
    std::string error;

    ASSERT_TRUE(parseType("list:(id:status|statusRef)", t, error)) << error;
    EXPECT_EQ(t.kind, Type::Kind::List);
    ASSERT_EQ(t.of.size(), 1u);
    EXPECT_EQ(t.of[0].kind, Type::Kind::Either);

    ASSERT_TRUE(parseType("list:id:status|statusRef", t, error)) << error;
    EXPECT_EQ(t.kind, Type::Kind::Either);

    ASSERT_TRUE(parseType("map:list:enum:a,b", t, error)) << error;
    EXPECT_EQ(t.of[0].of[0].words, (std::vector<std::string>{"a", "b"}));

    EXPECT_FALSE(parseType("list:(number", t, error));
    EXPECT_FALSE(parseType("id:spoon", t, error));
    EXPECT_FALSE(parseType("nothing", t, error));
    EXPECT_FALSE(parseType("number)", t, error));
    EXPECT_EQ(describe(Type{Type::Kind::Id, "element", {}, {}}), "the id of an element");
}

TEST(arena_schema, values_are_checked_all_the_way_down)
{
    EXPECT_TRUE(check(R"({ "offset": [0, 1, 1], "size": [1, 1, 1] })", "hitbox").empty());
    EXPECT_TRUE(check(R"([ "base:burn", { "id": "base:chill", "stacks": 2 } ])", "list:(id:status|statusRef)").empty());

    auto out = check("{\n \"offset\": [0, 1],\n \"sise\": [1, 1, 1]\n}", "hitbox");
    ASSERT_EQ(out.size(), 2u) << problems(out);
    EXPECT_EQ(out[0].line, 2);
    EXPECT_NE(out[0].text.find("it.offset should be three numbers"), std::string::npos) << out[0].text;
    EXPECT_EQ(out[1].line, 3);
    EXPECT_NE(out[1].text.find("unknown key \"sise\""), std::string::npos) << out[1].text;

    // An object given where an id or an object goes: the object's own problems, not "should be either"
    out = check(R"({ "id": "base:burn", "stack": 2 })", "id:status|statusRef");
    ASSERT_EQ(out.size(), 1u) << problems(out);
    EXPECT_NE(out[0].text.find("unknown key \"stack\""), std::string::npos);

    EXPECT_EQ(check("2.5", "integer").size(), 1u);
    EXPECT_EQ(check(R"("cube")", "enum:cube,cross").size(), 0u);
    EXPECT_EQ(check(R"("ball")", "enum:cube,cross").size(), 1u);
}

TEST(arena_schema, the_shipped_mods_follow_the_table)
{
    for (const auto& mod : fs::directory_iterator(VOXELARENA_MODS_DIR))
    {
        if (not mod.is_directory())
            continue;

        for (const auto& entry : fs::recursive_directory_iterator(mod.path()))
        {
            if (not entry.is_regular_file())
                continue;

            const std::string rel = fs::relative(entry.path(), mod.path()).generic_string();
            const File* file = nullptr;

            for (const auto& f : files())
                if (matches(f.path, rel))
                    file = &f;

            ASSERT_TRUE(file) << rel << " is not a file the table knows";

            if (std::string(file->type).empty())
                continue;

            std::ifstream in(entry.path());
            std::stringstream text;
            text << in.rdbuf();
            const JsonParseResult parsed = parseJson(text.str());
            ASSERT_TRUE(parsed.ok) << rel << ": " << parsed.error;

            std::vector<Problem> out;
            validate(parsed.value, file->type, rel, out);
            EXPECT_TRUE(out.empty()) << mod.path().filename().string() << "/" << problems(out);
        }
    }
}
