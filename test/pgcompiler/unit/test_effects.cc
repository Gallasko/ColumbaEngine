#include "gtest/gtest.h"

#include "Compiler/ast/analysis/locations.h"

namespace pg
{
    namespace test
    {
        using namespace ast::analysis;

        TEST(EffectLocations, PathsInternAndOverlap)
        {
            Locations l;
            const LocId a = l.global("a");
            const LocId ax = l.child(a, l.key("x")), axs = l.child(ax, Locations::AnyKey);
            EXPECT_EQ(l.text(axs), "g:a.x.*");
            EXPECT_EQ(l.child(a, l.key("x")), ax);  // interned
            const LocId amountAny = l.child(l.child(l.child(a, Locations::AnyKey), l.key("gains")), l.key("amount"));
            const LocId amountGains = l.child(l.child(l.child(a, l.key("gains")), l.key("gains")), l.key("amount"));
            EXPECT_TRUE(l.overlaps(amountAny, amountGains));
            EXPECT_FALSE(l.overlaps(ax, l.child(a, l.key("y"))));
            EXPECT_TRUE(l.overlaps(a, axs));  // a prefix overlaps below it
            EXPECT_TRUE(l.within(l.child(ax, l.key("y")), l.child(a, Locations::AnyKey)));
            EXPECT_FALSE(l.within(l.child(a, Locations::AnyKey), ax));
            EXPECT_FALSE(l.overlaps(ax, l.child(l.global("b"), l.key("x"))));
            LocId deepest = a;
            for (int i = 0; i < 8; ++i) deepest = l.child(deepest, l.key("k"));
            EXPECT_EQ(l.depth(deepest), MaxDepth);
            EXPECT_TRUE(l.deep(deepest));
            EXPECT_EQ(l.child(deepest, l.key("z")), deepest);
            EXPECT_EQ(l.text(l.child(l.param(0), l.paramKey(1))), "#0.[#1]");
            EXPECT_TRUE(l.overlaps(l.unknown(), ax));
        }

        TEST(EffectLocations, RootsAndText)
        {
            Locations l;
            int s1, s2;
            EXPECT_EQ(l.text(l.input("cfg")), "in:cfg");
            EXPECT_EQ(l.text(l.site(&s1)), "@0");
            EXPECT_EQ(l.text(l.site(&s2)), "@1");
            EXPECT_EQ(l.site(&s1), l.site(&s1));
            EXPECT_EQ(l.text(l.unknown()), "?");
            EXPECT_EQ(l.rootKind(l.param(2)), RootKind::Param);
            EXPECT_NE(l.global("a"), l.input("a"));
            const LocId p = l.child(l.child(l.child(l.param(0), l.paramKey(1)), Locations::AnyKey), l.key("amount"));
            EXPECT_EQ(l.text(p), "#0.[#1].*.amount");
            EXPECT_TRUE(l.isParamKey(l.keyOf(l.parent(l.parent(p)))));
            EXPECT_EQ(l.paramOfKey(l.keyOf(l.parent(l.parent(p)))), 1u);
            EXPECT_EQ(l.rootOf(p), l.param(0));
            EXPECT_TRUE(l.within(p, l.param(0)));
            EXPECT_FALSE(l.within(p, l.unknown()));
            EXPECT_FALSE(l.within(l.unknown(), l.unknown()));
        }

        TEST(EffectLocations, DeepFromCreationAndSeededAnyKey)
        {
            Locations l;
            LocId n = l.global("a");
            for (int i = 0; i < MaxDepth - 1; ++i) n = l.child(n, l.key("k"));
            EXPECT_FALSE(l.deep(n));
            n = l.child(n, l.key("k"));
            EXPECT_TRUE(l.deep(n));  // deep before any child is asked of it
            Locations fresh;
            EXPECT_EQ(fresh.text(fresh.child(fresh.param(0), Locations::AnyKey)), "#0.*");
        }
    }
}
