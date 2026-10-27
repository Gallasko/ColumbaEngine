#include "stdafx.h"

#include <gtest/gtest.h>

#include "Core/json.h"

using namespace arena;

TEST(arena_json, reads_nested_values_with_lines)
{
    const auto r = parseJson("{\n  \"a\": 1.5,\n  // a comment\n  \"b\": [true, \"x\\n\"],\n  \"c\": { \"d\": null }\n}");

    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_DOUBLE_EQ(r.value.num("a", 0), 1.5);
    EXPECT_EQ(r.value.find("a")->line, 2);

    const Json* b = r.value.find("b");
    ASSERT_TRUE(b and b->isArray());
    ASSERT_EQ(b->array.size(), 2u);
    EXPECT_TRUE(b->array[0].boolean);
    EXPECT_EQ(b->array[1].string, "x\n");
    EXPECT_EQ(b->line, 4);

    EXPECT_TRUE(r.value.find("c")->find("d")->isNull());
}

TEST(arena_json, reports_the_line_of_an_error)
{
    const auto r = parseJson("{\n  \"a\": 1,\n  \"b\": oops\n}");

    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.errorLine, 3);
    EXPECT_NE(r.error.find("line 3"), std::string::npos);
}

TEST(arena_json, rejects_duplicate_keys_and_trailing_text)
{
    EXPECT_FALSE(parseJson("{\"a\": 1, \"a\": 2}").ok);
    EXPECT_FALSE(parseJson("{} {}").ok);
    EXPECT_FALSE(parseJson("{\"a\": \"never closed}").ok);
}

TEST(arena_json, forgives_trailing_commas)
{
    const auto r = parseJson("{\"a\": [1, 2,], \"b\": 3,}");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(r.value.find("a")->array.size(), 2u);
}

TEST(arena_json, merge_patches_objects_and_replaces_the_rest)
{
    auto base = parseJson("{\"hp\": 10, \"stats\": {\"speed\": 2, \"armor\": 1}, \"tags\": [1, 2]}").value;
    const auto patch = parseJson("{\"stats\": {\"speed\": 5}, \"tags\": [3], \"new\": true}").value;

    mergeJson(base, patch);

    EXPECT_DOUBLE_EQ(base.num("hp", 0), 10);
    EXPECT_DOUBLE_EQ(base.find("stats")->num("speed", 0), 5);
    EXPECT_DOUBLE_EQ(base.find("stats")->num("armor", 0), 1);
    EXPECT_EQ(base.find("tags")->array.size(), 1u);
    EXPECT_TRUE(base.flag("new", false));

    EXPECT_EQ(toJsonString(parseJson("{\"a\": [1, 2.5, \"s\"]}").value), "{\"a\": [1, 2.5, \"s\"]}");
}
