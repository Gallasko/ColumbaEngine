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
        const float x = getParam(p, "x", 0.0f);
        const float y = getParam(p, "y", 0.0f);
        const float z = getParam(p, "z", 100.0f);
        const float scale = getParam(p, "scale", 1.0f);
        const std::string font = getParam(p, "font", "");
        const std::string text = getParam(p, "text", "");
        const constant::Vector4D color = {
            getParam(p, "r", 255.0f),
            getParam(p, "g", 255.0f),
            getParam(p, "b", 255.0f),
            getParam(p, "a", 255.0f),
        };

        auto cl = makeTTFText(ecs, x, y, z, font, text, scale, color);
        if (hasParam(p, "viewport"))
            cl.get<ViewportComponent>()->setViewport(static_cast<size_t>(getParamInt(p, "viewport")));
        if (hasParam(p, "visibility"))
            cl.get<PositionComponent>()->setVisibility(getParamBool(p, "visibility", true));
        return cl.entity;
    }

    // Build the "Panel" prefab: a single Shape2D backdrop wrapped in a Prefab container so
    // callers can do `panel->get<Prefab>()->getEntity("bg")` and toggle/center the wrapper as
    // a unit. Optional centering via centerInTarget = "<entity name>".
    EntityRef buildPanel(EntitySystem* ecs, const PrefabParams& p)
    {
        // Canonical wrap: kind="Prefab" always produces a container. children[0] becomes the
        // mainEntity, so the bg backdrop drives the container's size and the container can be
        // moved/centered/toggled as a unit.
        NodeSpec bg;
        bg.kind  = "Shape2D";
        bg.name  = "bg";
        bg.props = {
            {"shape",    getParam(p, "shape", "Square")},
            {"width",    getParam(p, "width",  100.0f)},
            {"height",   getParam(p, "height", 100.0f)},
            {"r",        getParam(p, "r",  20.0f)},
            {"g",        getParam(p, "g",  20.0f)},
            {"b",        getParam(p, "b",  30.0f)},
            {"a",        getParam(p, "a", 220.0f)},
            {"z",        getParam(p, "z",  97.0f)},
        };
        if (hasParam(p, "viewport"))
            bg.props["viewport"] = getParamInt(p, "viewport");

        NodeSpec spec;
        spec.kind = "Prefab";
        spec.children.push_back(std::move(bg));

        auto prefabEnt = buildNode(ecs, spec);

        // Optional: center the panel within a named target (e.g. __MainWindow).
        // The bg is anchored to the container top-left by SetMainEntity, so we only need to
        // center the container itself.
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

        NodeSpec spec;
        spec.kind  = "Shape2D";
        spec.name  = "bg";
        spec.props = {
            {"width",  getParam(p, "width",  200.0f)},
            {"height", getParam(p, "height",  32.0f)},
            {"r",      getParam(p, "r",  20.0f)},
            {"g",      getParam(p, "g",  20.0f)},
            {"b",      getParam(p, "b",  30.0f)},
            {"a",      getParam(p, "a", 220.0f)},
            {"z",      getParam(p, "z",  97.0f)},
        };
        if (hasParam(p, "viewport"))
            spec.props["viewport"] = getParamInt(p, "viewport");

        NodeSpec label;
        label.kind = "TTFText";
        label.name = "label";
        label.props = {
            {"font",  getParam(p, "font", "")},
            {"text",  getParam(p, "text", "")},
            {"scale", getParam(p, "scale", 1.0f)},
            {"r",     getParam(p, "textR", 255.0f)},
            {"g",     getParam(p, "textG", 255.0f)},
            {"b",     getParam(p, "textB", 255.0f)},
            {"a",     getParam(p, "textA", 255.0f)},
            {"z",     getParamFloat(p, "z", 97.0f) + 1.0f},
        };
        if (hasParam(p, "viewport"))
            label.props["viewport"] = getParamInt(p, "viewport");

        label.anchors = {
            AnchorSpec{"main", AnchorType::Left, padding},
            AnchorSpec{"main", AnchorType::Top,  padding},
        };

        spec.children.push_back(label);

        return buildNode(ecs, spec);
    }
}

void registerEnginePrefabFactories(PrefabFactoryRegistry* registry)
{
    if (not registry)
        return;

    {
        ParamSchema schema;
        schema.entries = {
            {"shape",          "Square"},
            {"width",          100.0f},
            {"height",         100.0f},
            {"r",              20.0f},
            {"g",              20.0f},
            {"b",              30.0f},
            {"a",              220.0f},
            {"z",              97.0f},
            {"viewport",       0,   ParamSchema::Requirement::Optional},
            {"centerInTarget", "",  ParamSchema::Requirement::Optional},
        };
        registry->registerFactory("Panel", std::move(schema), buildPanel);
    }

    {
        ParamSchema schema;
        schema.entries = {
            {"x",           0.0f},
            {"y",           0.0f},
            {"z",           100.0f},
            {"text",        ""},
            {"font",        ""},
            {"scale",       1.0f},
            {"r",           255.0f},
            {"g",           255.0f},
            {"b",           255.0f},
            {"a",           255.0f},
            {"viewport",    0, ParamSchema::Requirement::Optional},
            {"visibility",  true, ParamSchema::Requirement::Optional},
        };
        registry->registerFactory("Text", std::move(schema), buildText);
    }

    {
        ParamSchema schema;
        schema.entries = {
            {"width",    200.0f},
            {"height",   32.0f},
            {"text",     ""},
            {"font",     ""},
            {"scale",    1.0f},
            {"padding",  8.0f},
            {"r",        20.0f},
            {"g",        20.0f},
            {"b",        30.0f},
            {"a",        220.0f},
            {"textR",    255.0f},
            {"textG",    255.0f},
            {"textB",    255.0f},
            {"textA",    255.0f},
            {"z",        97.0f},
            {"viewport", 0, ParamSchema::Requirement::Optional},
        };
        registry->registerFactory("TitleBar", std::move(schema), buildTitleBar);
    }
}
}
