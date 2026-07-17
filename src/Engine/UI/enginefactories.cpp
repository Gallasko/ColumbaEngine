#include "enginefactories.h"

#include "prefabfactory.h"
#include "prefabbuilder.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"
#include "ECS/entitysystem.h"

namespace pg
{
namespace
{
    // ----------------------------------------------------------------------------------------
    // Shared helpers — apply common props (x/y/z/visibility/viewport) to any factory result.
    // ----------------------------------------------------------------------------------------
    void applyShared(EntityRef ent, const PrefabParams& p)
    {
        if (not ent or not ent->has<PositionComponent>())
            return;

        auto pos = ent->get<PositionComponent>();

        if (hasParam(p, "x"))
            pos->setX(getParam(p, "x"));
        if (hasParam(p, "y"))
            pos->setY(getParam(p, "y"));
        if (hasParam(p, "z"))
            pos->setZ(getParam(p, "z"));
        if (hasParam(p, "visibility"))
            pos->setVisibility(getParam(p, "visibility", true));

        if (ent->has<ViewportComponent>() and hasParam(p, "viewport"))
            ent->get<ViewportComponent>()->setViewport(static_cast<size_t>(getParam(p, "viewport")));
    }

    Shape2D shapeFromString(const std::string& s)
    {
        if (s == "Circle")
            return Shape2D::Circle;
        return Shape2D::Square;
    }

    constant::Vector4D readColor(const PrefabParams& p,
        constant::Vector4D fallback = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        return {
            getParam(p, "r", fallback.x),
            getParam(p, "g", fallback.y),
            getParam(p, "b", fallback.z),
            getParam(p, "a", fallback.w),
        };
    }

    // ----------------------------------------------------------------------------------------
    // Built-in primitive factories — registered under their kind name (Shape2D / TTFText /
    // Texture). `buildNode`'s realiseLeaf path looks them up via `PrefabFactoryRegistry::build`
    // exactly the same way as user-registered factories.
    // ----------------------------------------------------------------------------------------
    EntityRef buildShape2D(EntitySystem* ecs, const PrefabParams& p)
    {
        const Shape2D shape = shapeFromString(getParam(p, "shape", "Square"));
        const float w = getParam(p, "width",  0.0f);
        const float h = getParam(p, "height", 0.0f);
        const auto color = readColor(p);
        auto cl = makeUiSimple2DShape(ecs, shape, w, h, color);
        applyShared(cl.entity, p);
        return cl.entity;
    }

    EntityRef buildTexture(EntitySystem* ecs, const PrefabParams& p)
    {
        const std::string texName = getParam(p, "texture", std::string("NoneIcon"));
        const float w = getParam(p, "width",  0.0f);
        const float h = getParam(p, "height", 0.0f);
        auto cl = makeUiTexture(ecs, w, h, texName);
        applyShared(cl.entity, p);
        return cl.entity;
    }

    // ----------------------------------------------------------------------------------------
    // Built-in / engine-level composite factories.
    // ----------------------------------------------------------------------------------------

    // The "Text" factory: a single TTFText entity at (x, y, z) with the given
    // font/text/scale/color/viewport.
    EntityRef buildText(EntitySystem* ecs, const PrefabParams& p)
    {
        const float x = getParam(p, "x", 0.0f);
        const float y = getParam(p, "y", 0.0f);
        const float z = getParam(p, "z", 100.0f);
        const float scale = getParam(p, "scale", 1.0f);
        const std::string font = getParam(p, "font", "");
        const std::string text = getParam(p, "text", "");
        const constant::Vector4D color = readColor(p);

        auto cl = makeTTFText(ecs, x, y, z, font, text, scale, color);
        if (hasParam(p, "viewport"))
            cl.get<ViewportComponent>()->setViewport(static_cast<size_t>(getParam(p, "viewport")));
        if (hasParam(p, "visibility"))
            cl.get<PositionComponent>()->setVisibility(getParam(p, "visibility", true));
        return cl.entity;
    }

    // "TTFText" primitive — registered under the kind name. Identical to "Text" but follows the
    // primitive naming so users can write `kind = "TTFText"` in a NodeSpec.
    EntityRef buildTTFText(EntitySystem* ecs, const PrefabParams& p)
    {
        return buildText(ecs, p);
    }

    // The "Panel" prefab: a Shape2D backdrop wrapped in a Prefab container. Because every
    // non-Layout NodeSpec now wraps automatically, this is just `kind="Shape2D"` with the bg
    // props — buildNode does the wrap.
    EntityRef buildPanel(EntitySystem* ecs, const PrefabParams& p)
    {
        NodeSpec spec;
        spec.kind  = "Shape2D";
        spec.name  = "bg";
        spec.props = {
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
            spec.props["viewport"] = getParam(p, "viewport");

        auto prefabEnt = buildNode(ecs, spec);

        // Optional: center the panel within a named target (e.g. __MainWindow). The bg is
        // anchored to the container top-left by SetMainEntity, so only the container is centered.
        const std::string centerTarget = getParam(p, "centerInTarget", std::string{});
        if (not centerTarget.empty() and prefabEnt and prefabEnt->has<UiAnchor>())
        {
            if (auto targetEnt = ecs->getEntity(centerTarget))
                if (targetEnt->has<UiAnchor>())
                    prefabEnt->get<UiAnchor>()->centeredIn(targetEnt->get<UiAnchor>());
        }

        return prefabEnt;
    }

    // The "TitleBar" prefab: backdrop + title text anchored top-left with configurable padding.
    EntityRef buildTitleBar(EntitySystem* ecs, const PrefabParams& p)
    {
        const float padding = getParam(p, "padding", 8.0f);

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
            spec.props["viewport"] = getParam(p, "viewport");

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
            label.props["viewport"] = getParam(p, "viewport");

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

    // -------- Built-in primitives --------

    {
        ParamSchema schema;
        schema.entries = {
            {"shape",      "Square"},
            {"width",       0.0f},
            {"height",      0.0f},
            {"r",         255.0f},
            {"g",         255.0f},
            {"b",         255.0f},
            {"a",         255.0f},
            {"x",           0.0f, ParamSchema::Requirement::Optional},
            {"y",           0.0f, ParamSchema::Requirement::Optional},
            {"z",           0.0f, ParamSchema::Requirement::Optional},
            {"viewport",    0,    ParamSchema::Requirement::Optional},
            {"visibility",  true, ParamSchema::Requirement::Optional},
        };
        registry->registerFactory("Shape2D", std::move(schema), buildShape2D);
    }

    {
        ParamSchema schema;
        schema.entries = {
            {"texture",   "NoneIcon"},
            {"width",      0.0f},
            {"height",     0.0f},
            {"x",          0.0f, ParamSchema::Requirement::Optional},
            {"y",          0.0f, ParamSchema::Requirement::Optional},
            {"z",          0.0f, ParamSchema::Requirement::Optional},
            {"viewport",   0,    ParamSchema::Requirement::Optional},
            {"visibility", true, ParamSchema::Requirement::Optional},
        };
        registry->registerFactory("Texture", std::move(schema), buildTexture);
    }

    {
        ParamSchema schema;
        schema.entries = {
            {"x",           0.0f},
            {"y",           0.0f},
            {"z",         100.0f},
            {"text",       ""},
            {"font",       ""},
            {"scale",       1.0f},
            {"r",         255.0f},
            {"g",         255.0f},
            {"b",         255.0f},
            {"a",         255.0f},
            {"viewport",    0,    ParamSchema::Requirement::Optional},
            {"visibility",  true, ParamSchema::Requirement::Optional},
        };
        registry->registerFactory("TTFText", std::move(schema), buildTTFText);
    }

    // -------- Composite factories --------

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
            {"viewport",    0,    ParamSchema::Requirement::Optional},
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
