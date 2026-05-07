#include "prefabbuilder.h"

#include "ECS/entitysystem.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"
#include "UI/prefab.h"
#include "UI/prefabfactory.h"

#include <string_view>
#include <unordered_map>
#include <utility>

namespace pg
{
namespace
{
    constexpr std::string_view FACTORY_PREFIX  = "Factory:";
    constexpr const char*      RESERVED_MAIN   = "main";
    constexpr const char*      RESERVED_PARENT = "parent";

    Shape2D shapeFromString(const std::string& s)
    {
        if (s == "Circle") return Shape2D::Circle;
        return Shape2D::Square;
    }

    constant::Vector4D readColor(const ElementMap& p,
        constant::Vector4D fallback = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        return {
            getParamFloat(p, "r", fallback.x),
            getParamFloat(p, "g", fallback.y),
            getParamFloat(p, "b", fallback.z),
            getParamFloat(p, "a", fallback.w),
        };
    }

    void applyShared(EntityRef ent, const ElementMap& p)
    {
        if (not ent or not ent->has<PositionComponent>())
            return;

        auto pos = ent->get<PositionComponent>();

        if (hasParam(p, "x")) pos->setX(getParamFloat(p, "x"));
        if (hasParam(p, "y")) pos->setY(getParamFloat(p, "y"));
        if (hasParam(p, "z")) pos->setZ(getParamFloat(p, "z"));
        if (hasParam(p, "visibility"))
            pos->setVisibility(getParamBool(p, "visibility", true));

        if (ent->has<ViewportComponent>() and hasParam(p, "viewport"))
            ent->get<ViewportComponent>()->setViewport(static_cast<size_t>(getParamInt(p, "viewport")));
    }

    EntityRef makeShape2D(EntitySystem* ecs, const ElementMap& p)
    {
        const Shape2D shape = shapeFromString(getParamString(p, "shape", "Square"));
        const float w = getParamFloat(p, "width",  0.0f);
        const float h = getParamFloat(p, "height", 0.0f);
        const auto color = readColor(p);
        auto cl = makeUiSimple2DShape(ecs, shape, w, h, color);
        applyShared(cl.entity, p);
        return cl.entity;
    }

    EntityRef makeText(EntitySystem* ecs, const ElementMap& p)
    {
        const std::string font = getParamString(p, "font");
        const std::string text = getParamString(p, "text");
        const float scale = getParamFloat(p, "scale", 1.0f);
        const float x = getParamFloat(p, "x", 0.0f);
        const float y = getParamFloat(p, "y", 0.0f);
        const float z = getParamFloat(p, "z", 0.0f);
        const auto color = readColor(p);
        auto cl = makeTTFText(ecs, x, y, z, font, text, scale, color);
        if (hasParam(p, "viewport"))
            cl.get<ViewportComponent>()->setViewport(static_cast<size_t>(getParamInt(p, "viewport")));
        if (hasParam(p, "visibility"))
            cl.get<PositionComponent>()->setVisibility(getParamBool(p, "visibility", true));
        return cl.entity;
    }

    EntityRef makeTextureNode(EntitySystem* ecs, const ElementMap& p)
    {
        const std::string texName = getParamString(p, "texture", "NoneIcon");
        const float w = getParamFloat(p, "width",  0.0f);
        const float h = getParamFloat(p, "height", 0.0f);
        auto cl = makeUiTexture(ecs, w, h, texName);
        applyShared(cl.entity, p);
        return cl.entity;
    }

    EntityRef invokeFactory(EntitySystem* ecs, std::string_view factoryName, const ElementMap& p)
    {
        auto* registry = ecs->getSystem<PrefabFactoryRegistry>();
        if (not registry)
        {
            LOG_ERROR("Prefab Builder", "No PrefabFactoryRegistry available to invoke factory: " << factoryName);
            return EntityRef{};
        }
        return registry->build(std::string(factoryName), p);
    }

    PosAnchor pickSide(UiAnchor* a, _unique_id id, AnchorType side)
    {
        switch (side)
        {
            case AnchorType::Top:    return a->top;
            case AnchorType::Bottom: return a->bottom;
            case AnchorType::Left:   return a->left;
            case AnchorType::Right:  return a->right;
            case AnchorType::Width:  return PosAnchor{id, AnchorType::Width};
            case AnchorType::Height: return PosAnchor{id, AnchorType::Height};
            default:                 return PosAnchor{};
        }
    }

    void applyAnchorsToEntity(EntityRef ent, const NodeSpec& node,
                              std::unordered_map<std::string, EntityRef>& nameToEntity)
    {
        if (not ent or not ent->has<UiAnchor>())
            return;

        auto anchor = ent->get<UiAnchor>();

        for (const auto& a : node.anchors)
        {
            auto it = nameToEntity.find(a.target);
            if (it == nameToEntity.end())
            {
                LOG_ERROR("Prefab Builder", "Anchor target not found: '" << a.target << "'");
                continue;
            }
            auto target = it->second;
            if (not target->has<UiAnchor>())
                continue;

            auto tAnchor = target->get<UiAnchor>();
            const AnchorType ts = (a.targetSide == AnchorType::None) ? a.side : a.targetSide;
            const PosAnchor pa = pickSide(tAnchor, target->id, ts);

            switch (a.side)
            {
                case AnchorType::Top:
                    anchor->setTopAnchor(pa);
                    if (a.margin != 0.0f) anchor->setTopMargin(a.margin);
                    break;
                case AnchorType::Bottom:
                    anchor->setBottomAnchor(pa);
                    if (a.margin != 0.0f) anchor->setBottomMargin(a.margin);
                    break;
                case AnchorType::Left:
                    anchor->setLeftAnchor(pa);
                    if (a.margin != 0.0f) anchor->setLeftMargin(a.margin);
                    break;
                case AnchorType::Right:
                    anchor->setRightAnchor(pa);
                    if (a.margin != 0.0f) anchor->setRightMargin(a.margin);
                    break;
                case AnchorType::Width:
                    anchor->setWidthConstrain(PosConstrain{target->id, AnchorType::Width});
                    break;
                case AnchorType::Height:
                    anchor->setHeightConstrain(PosConstrain{target->id, AnchorType::Height});
                    break;
                default:
                    break;
            }
        }

        for (const auto& c : node.centerIn)
        {
            auto it = nameToEntity.find(c.target);
            if (it == nameToEntity.end())
            {
                LOG_ERROR("Prefab Builder", "centeredIn target not found: '" << c.target << "'");
                continue;
            }
            auto target = it->second;
            if (target->has<UiAnchor>())
                anchor->centeredIn(target->get<UiAnchor>());
        }
    }
}

EntityRef buildNode(EntitySystem* ecs, const NodeSpec& spec)
{
    if (spec.kind.size() >= FACTORY_PREFIX.size() and
        std::string_view(spec.kind).substr(0, FACTORY_PREFIX.size()) == FACTORY_PREFIX)
    {
        return invokeFactory(ecs,
            std::string_view(spec.kind).substr(FACTORY_PREFIX.size()),
            spec.props);
    }

    if (spec.kind == "Shape2D") return makeShape2D(ecs, spec.props);
    if (spec.kind == "TTFText") return makeText(ecs, spec.props);
    if (spec.kind == "Texture") return makeTextureNode(ecs, spec.props);

    LOG_ERROR("Prefab Builder", "Unknown node kind: '" << spec.kind << "'");
    return EntityRef{};
}

EntityRef buildPrefab(EntitySystem* ecs, const PrefabSpec& spec)
{
    auto container = makeAnchoredPrefab(ecs);
    auto prefab = container.get<Prefab>();

    std::unordered_map<std::string, EntityRef> nameToEntity;
    nameToEntity[RESERVED_PARENT] = container.entity;

    auto mainEnt = buildNode(ecs, spec.mainNode);
    if (mainEnt)
    {
        // Engine registers the main entity under the hardcoded name
        // "MainEntity" (see PrefabSystem::onEvent(SetMainEntityEvent)).
        // Also expose it under the spec's chosen name so callers can look it
        // up by the same key they wrote in the spec — otherwise getEntity("bg")
        // misses for a Panel built from a spec with mainNode.name = "bg".
        prefab->setMainEntity(mainEnt);
        nameToEntity[RESERVED_MAIN] = mainEnt;
        if (not spec.mainNode.name.empty())
        {
            prefab->addToPrefab(mainEnt, spec.mainNode.name);
            nameToEntity[spec.mainNode.name] = mainEnt;
        }
    }

    std::vector<std::pair<EntityRef, const NodeSpec*>> built;
    built.reserve(spec.children.size());

    for (const auto& child : spec.children)
    {
        auto ent = buildNode(ecs, child);
        if (not ent)
            continue;

        prefab->addToPrefab(ent, child.name);

        if (not child.name.empty())
            nameToEntity[child.name] = ent;

        built.push_back({ent, &child});
    }

    if (mainEnt)
        applyAnchorsToEntity(mainEnt, spec.mainNode, nameToEntity);

    for (auto& kv : built)
        applyAnchorsToEntity(kv.first, *kv.second, nameToEntity);

    return container.entity;
}
}
