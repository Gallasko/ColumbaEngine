#include "stdafx.h"

#include "gtest/gtest.h"

#include "UI/prefabloader.h"
#include "UI/prefabspec.h"
#include "ECS/entitysystem.h"

#include "mocklogger.h"

// loadNodeSpec: a .yaml file, parsed by tools/yaml_parser.pg on the embedded VM, mapped
// onto a NodeSpec. The fixtures live in testdeps/ui and are copied next to the binary.
namespace pg
{
    namespace test
    {
        namespace
        {
            struct Loaded
            {
                std::vector<std::string> errors;
                std::optional<NodeSpec> spec;
            };

            Loaded load(const std::string& file)
            {
                EntitySystem ecs;
                Loaded out;

                PrefabLoadOptions options;
                options.errors = &out.errors;

                out.spec = loadNodeSpec(&ecs, file, options);
                return out;
            }

            std::string joined(const std::vector<std::string>& v)
            {
                std::string s;
                for (const auto& e : v)
                    s += e + "\n";
                return s;
            }
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_loader_test, basic_file_maps_onto_nodespec)
        {
            Loaded l = load("ui/loader_basic.yaml");

            ASSERT_TRUE(l.spec.has_value()) << joined(l.errors);
            EXPECT_TRUE(l.errors.empty()) << joined(l.errors);

            const NodeSpec& spec = *l.spec;

            EXPECT_EQ(spec.kind, "Panel");
            EXPECT_EQ(spec.name, "root");

            // Scalars keep their type.
            ASSERT_TRUE(spec.props.count("x"));
            EXPECT_EQ(spec.props.at("x").type, UnionType::INT);
            EXPECT_EQ(spec.props.at("x").get<int>(), 48);

            ASSERT_TRUE(spec.props.count("y"));
            EXPECT_EQ(spec.props.at("y").type, UnionType::FLOAT);
            EXPECT_FLOAT_EQ(spec.props.at("y").get<float>(), 56.5f);

            EXPECT_EQ(spec.props.at("width").get<int>(), 300);
            EXPECT_EQ(spec.props.at("heading").get<std::string>(), "Parts: the list");
            EXPECT_EQ(spec.props.at("enabled").type, UnionType::BOOL);
            EXPECT_TRUE(spec.props.at("enabled").get<bool>());

            // Reserved keys land in their fields (spacing also stays in props for layouts).
            EXPECT_EQ(spec.flow, Flow::Vertical);
            EXPECT_FLOAT_EQ(spec.spacing, 8.0f);
            EXPECT_EQ(spec.props.at("spacing").get<int>(), 8);

            // Anchors: inline and block forms.
            ASSERT_EQ(spec.anchors.size(), 2u);
            EXPECT_EQ(spec.anchors[0].side, AnchorType::Left);
            EXPECT_EQ(spec.anchors[0].target, "window");
            EXPECT_FLOAT_EQ(spec.anchors[0].margin, 48.0f);
            EXPECT_EQ(spec.anchors[1].side, AnchorType::Top);
            EXPECT_EQ(spec.anchors[1].targetSide, AnchorType::Bottom);
            EXPECT_FLOAT_EQ(spec.anchors[1].margin, 2.0f);

            // A list of maps -> records.
            ASSERT_TRUE(spec.records.count("items"));
            ASSERT_EQ(spec.records.at("items").size(), 2u);
            EXPECT_EQ(spec.records.at("items")[0].at("label").get<std::string>(), "Strength");
            EXPECT_EQ(spec.records.at("items")[0].at("current").get<int>(), 12);
            EXPECT_EQ(spec.records.at("items")[1].at("label").get<std::string>(), "Has the Guild's letter");
            EXPECT_EQ(spec.records.at("items")[1].at("met").type, UnionType::BOOL);
            EXPECT_FALSE(spec.records.at("items")[1].at("met").get<bool>());

            // A single nested map -> a one-record list.
            ASSERT_TRUE(spec.records.count("label"));
            ASSERT_EQ(spec.records.at("label").size(), 1u);
            EXPECT_EQ(spec.records.at("label")[0].at("style").get<std::string>(), "body");

            // Children, in order, recursively.
            ASSERT_EQ(spec.children.size(), 2u);
            EXPECT_EQ(spec.children[0].kind, "Label");
            EXPECT_EQ(spec.children[0].name, "a");
            EXPECT_EQ(spec.children[0].props.at("text").get<std::string>(), "Hello world");
            EXPECT_EQ(spec.children[0].props.at("note").get<std::string>(), "multi\nline");

            EXPECT_EQ(spec.children[1].kind, "Layout:Vertical");
            EXPECT_EQ(spec.children[1].props.at("spacing").get<int>(), 4);
            ASSERT_EQ(spec.children[1].children.size(), 1u);
            EXPECT_EQ(spec.children[1].children[0].props.at("text").get<std::string>(), "inner");
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_loader_test, top_level_list_is_rejected)
        {
            Loaded l = load("ui/loader_list.yaml");

            EXPECT_FALSE(l.spec.has_value());
            EXPECT_FALSE(l.errors.empty());
        }

        // ----------------------------------------------------------------------------------------
        TEST(prefab_loader_test, scalar_list_under_a_prop_is_reported_and_skipped)
        {
            Loaded l = load("ui/loader_scalarlist.yaml");

            ASSERT_TRUE(l.spec.has_value());
            EXPECT_FALSE(l.errors.empty());
            EXPECT_EQ(l.spec->records.count("tags"), 0u);
            EXPECT_EQ(l.spec->props.count("tags"), 0u);

            // The rest of the file still loads.
            EXPECT_EQ(l.spec->kind, "Label");
            EXPECT_EQ(l.spec->props.at("text").get<std::string>(), "still here");
        }

        // ----------------------------------------------------------------------------------------
        // The same parser serves the component generator: a .pgcomp file comes through as a
        // node whose sections are records (the scalar-list sections are reported, by design).
        TEST(prefab_loader_test, pgcomp_shaped_file_parses_as_records)
        {
            Loaded l = load("ui/loader_pgcomp.yaml");

            ASSERT_TRUE(l.spec.has_value()) << joined(l.errors);

            EXPECT_EQ(l.spec->name, "PositionComponent");   // `name` is a reserved key, not a prop
            EXPECT_EQ(l.spec->props.at("namespace").get<std::string>(), "pg");

            ASSERT_TRUE(l.spec->records.count("fields"));
            const auto& fields = l.spec->records.at("fields");
            ASSERT_EQ(fields.size(), 8u);
            EXPECT_EQ(fields[0].at("name").get<std::string>(), "x");
            EXPECT_EQ(fields[0].at("type").get<std::string>(), "float");
            EXPECT_EQ(fields[0].at("default").type, UnionType::STRING);   // "0.0f" is not a number
            EXPECT_EQ(fields[0].at("default").get<std::string>(), "0.0f");
            EXPECT_EQ(fields[0].at("setter").get<std::string>(), "event");
            EXPECT_EQ(fields[6].at("default").type, UnionType::BOOL);

            // `forwards`, `methods`, `events` are lists of scalars: not a NodeSpec concept.
            EXPECT_FALSE(l.errors.empty());
            EXPECT_EQ(l.spec->records.count("methods"), 0u);
        }
    }
}
