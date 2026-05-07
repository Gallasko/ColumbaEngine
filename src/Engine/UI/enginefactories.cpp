#include "enginefactories.h"

#include "prefabfactory.h"
#include "prefabbuilder.h"
#include "UI/ttftext.h"
#include "ECS/entitysystem.h"

namespace pg
{
namespace
{
    // Build the "Text" factory: a single TTFText entity at (x, y, z) with the
    // given font/text/scale/color/viewport. Returns the text entity directly,
    // not wrapped in a Prefab — callers position/show/hide it as one entity.
    EntityRef buildText(EntitySystem* ecs, const PrefabParams& p)
    {
        const float x = getParamFloat(p, "x", 0.0f);
        const float y = getParamFloat(p, "y", 0.0f);
        const float z = getParamFloat(p, "z", 100.0f);
        const float scale = getParamFloat(p, "scale", 1.0f);
        const std::string font = getParamString(p, "font", "");
        const std::string text = getParamString(p, "text", "");
        const constant::Vector4D color = {
            getParamFloat(p, "r", 255.0f),
            getParamFloat(p, "g", 255.0f),
            getParamFloat(p, "b", 255.0f),
            getParamFloat(p, "a", 255.0f),
        };

        auto cl = makeTTFText(ecs, x, y, z, font, text, scale, color);
        if (hasParam(p, "viewport"))
            cl.get<ViewportComponent>()->setViewport(static_cast<size_t>(getParamInt(p, "viewport")));
        if (hasParam(p, "visibility"))
            cl.get<PositionComponent>()->setVisibility(getParamBool(p, "visibility", true));
        return cl.entity;
    }

    // Build the "Panel" prefab: a single Shape2D backdrop. Optional centering
    // by passing centerInTarget = "<entity name>" — the panel anchors itself
    // centered within that target (e.g. "__MainWindow").
    EntityRef buildPanel(EntitySystem* ecs, const PrefabParams& p)
    {
        PrefabSpec spec;
        spec.mainNode.kind = "Shape2D";
        spec.mainNode.name = "bg";
        spec.mainNode.props = {
            {"shape",    getParamString(p, "shape", "Square")},
            {"width",    getParamFloat(p, "width",  100.0f)},
            {"height",   getParamFloat(p, "height", 100.0f)},
            {"r",        getParamFloat(p, "r",  20.0f)},
            {"g",        getParamFloat(p, "g",  20.0f)},
            {"b",        getParamFloat(p, "b",  30.0f)},
            {"a",        getParamFloat(p, "a", 220.0f)},
            {"z",        getParamFloat(p, "z",  97.0f)},
        };
        if (hasParam(p, "viewport"))
            spec.mainNode.props["viewport"] = getParamInt(p, "viewport");

        auto prefabEnt = buildPrefab(ecs, spec);

        // Optional: center the panel within a named target (e.g. __MainWindow).
        // The bg child is already anchored to the prefab container's top-left
        // by SetMainEntity, so we only need to center the container itself.
        const std::string centerTarget = getParamString(p, "centerInTarget", "");
        if (not centerTarget.empty() and prefabEnt and prefabEnt->has<UiAnchor>())
        {
            if (auto targetEnt = ecs->getEntity(centerTarget))
                if (targetEnt->has<UiAnchor>())
                    prefabEnt->get<UiAnchor>()->centeredIn(targetEnt->get<UiAnchor>());
        }

        return prefabEnt;
    }

    // Build the "TitleBar" prefab: backdrop + title text anchored top-left
    // with configurable padding.
    EntityRef buildTitleBar(EntitySystem* ecs, const PrefabParams& p)
    {
        const float padding = getParamFloat(p, "padding", 8.0f);

        PrefabSpec spec;
        spec.mainNode.kind = "Shape2D";
        spec.mainNode.name = "bg";
        spec.mainNode.props = {
            {"width",  getParamFloat(p, "width",  200.0f)},
            {"height", getParamFloat(p, "height",  32.0f)},
            {"r",      getParamFloat(p, "r",  20.0f)},
            {"g",      getParamFloat(p, "g",  20.0f)},
            {"b",      getParamFloat(p, "b",  30.0f)},
            {"a",      getParamFloat(p, "a", 220.0f)},
            {"z",      getParamFloat(p, "z",  97.0f)},
        };
        if (hasParam(p, "viewport"))
            spec.mainNode.props["viewport"] = getParamInt(p, "viewport");

        NodeSpec label;
        label.kind = "TTFText";
        label.name = "label";
        label.props = {
            {"font",  getParamString(p, "font", "")},
            {"text",  getParamString(p, "text", "")},
            {"scale", getParamFloat(p, "scale", 1.0f)},
            {"r",     getParamFloat(p, "textR", 255.0f)},
            {"g",     getParamFloat(p, "textG", 255.0f)},
            {"b",     getParamFloat(p, "textB", 255.0f)},
            {"a",     getParamFloat(p, "textA", 255.0f)},
            {"z",     getParamFloat(p, "z", 97.0f) + 1.0f},
        };
        if (hasParam(p, "viewport"))
            label.props["viewport"] = getParamInt(p, "viewport");

        AnchorSpec leftAnchor; leftAnchor.target = "main"; leftAnchor.side = AnchorType::Left; leftAnchor.margin = padding;
        AnchorSpec topAnchor;  topAnchor.target  = "main"; topAnchor.side  = AnchorType::Top;  topAnchor.margin  = padding;
        label.anchors = {leftAnchor, topAnchor};

        spec.children.push_back(label);

        return buildPrefab(ecs, spec);
    }
}

void registerEnginePrefabFactories(PrefabFactoryRegistry* registry)
{
    if (not registry)
        return;

    {
        ParamSchema schema;
        schema.entries = {
            {"shape",          ElementType::UnionType::STRING, ElementType{std::string("Square")}},
            {"width",          ElementType::UnionType::FLOAT,  ElementType{100.0f}},
            {"height",         ElementType::UnionType::FLOAT,  ElementType{100.0f}},
            {"r",              ElementType::UnionType::FLOAT,  ElementType{ 20.0f}},
            {"g",              ElementType::UnionType::FLOAT,  ElementType{ 20.0f}},
            {"b",              ElementType::UnionType::FLOAT,  ElementType{ 30.0f}},
            {"a",              ElementType::UnionType::FLOAT,  ElementType{220.0f}},
            {"z",              ElementType::UnionType::FLOAT,  ElementType{ 97.0f}},
            {"viewport",       ElementType::UnionType::INT,    ElementType{0}},
            {"centerInTarget", ElementType::UnionType::STRING, ElementType{std::string("")}},
        };
        registry->registerFactory("Panel", std::move(schema), buildPanel);
    }

    {
        ParamSchema schema;
        schema.entries = {
            {"x",        ElementType::UnionType::FLOAT,  ElementType{0.0f}},
            {"y",        ElementType::UnionType::FLOAT,  ElementType{0.0f}},
            {"z",        ElementType::UnionType::FLOAT,  ElementType{100.0f}},
            {"text",     ElementType::UnionType::STRING, ElementType{std::string("")}},
            {"font",     ElementType::UnionType::STRING, ElementType{std::string("")}},
            {"scale",    ElementType::UnionType::FLOAT,  ElementType{1.0f}},
            {"r",        ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"g",        ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"b",        ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"a",        ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"viewport", ElementType::UnionType::INT,    ElementType{0}},
        };
        registry->registerFactory("Text", std::move(schema), buildText);
    }

    {
        ParamSchema schema;
        schema.entries = {
            {"width",    ElementType::UnionType::FLOAT,  ElementType{200.0f}},
            {"height",   ElementType::UnionType::FLOAT,  ElementType{ 32.0f}},
            {"text",     ElementType::UnionType::STRING, ElementType{std::string("")}},
            {"font",     ElementType::UnionType::STRING, ElementType{std::string("")}},
            {"scale",    ElementType::UnionType::FLOAT,  ElementType{1.0f}},
            {"padding",  ElementType::UnionType::FLOAT,  ElementType{8.0f}},
            {"r",        ElementType::UnionType::FLOAT,  ElementType{ 20.0f}},
            {"g",        ElementType::UnionType::FLOAT,  ElementType{ 20.0f}},
            {"b",        ElementType::UnionType::FLOAT,  ElementType{ 30.0f}},
            {"a",        ElementType::UnionType::FLOAT,  ElementType{220.0f}},
            {"textR",    ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"textG",    ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"textB",    ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"textA",    ElementType::UnionType::FLOAT,  ElementType{255.0f}},
            {"z",        ElementType::UnionType::FLOAT,  ElementType{ 97.0f}},
            {"viewport", ElementType::UnionType::INT,    ElementType{0}},
        };
        registry->registerFactory("TitleBar", std::move(schema), buildTitleBar);
    }
}
}
