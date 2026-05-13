#include "prefabbuilder.h"

#include "ECS/entitysystem.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"
#include "UI/prefab.h"
#include "UI/prefabfactory.h"
#include "Systems/coresystems.h"

#include <string_view>
#include <type_traits>
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
        if (s == "Circle")
            return Shape2D::Circle;
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

        if (hasParam(p, "x"))
            pos->setX(getParamFloat(p, "x"));
        if (hasParam(p, "y"))
            pos->setY(getParamFloat(p, "y"));
        if (hasParam(p, "z"))
            pos->setZ(getParamFloat(p, "z"));
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
            case AnchorType::Top:              return a->top;
            case AnchorType::Bottom:           return a->bottom;
            case AnchorType::Left:             return a->left;
            case AnchorType::Right:            return a->right;
            case AnchorType::Width:            return PosAnchor{id, AnchorType::Width};
            case AnchorType::Height:           return PosAnchor{id, AnchorType::Height};
            case AnchorType::VerticalCenter:   return a->verticalCenter;
            case AnchorType::HorizontalCenter: return a->horizontalCenter;
            default:                           return PosAnchor{};
        }
    }

    // Three-tier resolution: explicit id -> local sibling scope -> global EntityNameSystem.
    EntityRef resolveByIdOrName(EntitySystem* ecs, _unique_id targetId, const std::string& targetName,
                                const std::unordered_map<std::string, EntityRef>& scope)
    {
        if (targetId != 0)
            return ecs->getEntity(targetId);

        if (not targetName.empty())
        {
            auto it = scope.find(targetName);
            if (it != scope.end())
                return it->second;

            if (auto* names = ecs->getSystem<EntityNameSystem>())
            {
                const _unique_id id = names->getEntityId(targetName);
                if (id != 0)
                    return ecs->getEntity(id);
            }
        }

        return EntityRef{};
    }

    void applyAnchorsToEntity(EntityRef ent,
                              const std::vector<AnchorSpec>& anchors,
                              std::unordered_map<std::string, EntityRef>& nameToEntity)
    {
        if (not ent or not ent->has<UiAnchor>())
            return;

        auto* ecs = ent.ecsRef;
        auto anchor = ent->get<UiAnchor>();

        for (const auto& a : anchors)
        {
            // Empty target (no id, no name) means the anchor is unspecified — skip silently.
            if (a.targetId == 0 and a.target.empty())
                continue;

            EntityRef target = resolveByIdOrName(ecs, a.targetId, a.target, nameToEntity);
            if (not target)
            {
                LOG_ERROR("Prefab Builder", "Anchor target not found: id=" << a.targetId << " name='" << a.target << "'");
                continue;
            }
            if (not target->has<UiAnchor>())
                continue;

            auto tAnchor = target->get<UiAnchor>();
            const AnchorType ts = (a.targetSide == AnchorType::None) ? a.side : a.targetSide;
            const PosAnchor pa = pickSide(tAnchor, target->id, ts);

            switch (a.side)
            {
                case AnchorType::Top:
                    anchor->setTopAnchor(pa);
                    anchor->setTopMargin(a.margin);
                    break;
                case AnchorType::Bottom:
                    anchor->setBottomAnchor(pa);
                    anchor->setBottomMargin(a.margin);
                    break;
                case AnchorType::Left:
                    anchor->setLeftAnchor(pa);
                    anchor->setLeftMargin(a.margin);
                    break;
                case AnchorType::Right:
                    anchor->setRightAnchor(pa);
                    anchor->setRightMargin(a.margin);
                    break;
                case AnchorType::Width:
                    anchor->setWidthConstrain(PosConstrain{target->id, AnchorType::Width});
                    break;
                case AnchorType::Height:
                    anchor->setHeightConstrain(PosConstrain{target->id, AnchorType::Height});
                    break;
                case AnchorType::VerticalCenter:
                    anchor->setVerticalCenter(pa);
                    break;
                case AnchorType::HorizontalCenter:
                    anchor->setHorizontalCenter(pa);
                    break;
                default:
                    break;
            }
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

    if (spec.kind == "Shape2D")
        return makeShape2D(ecs, spec.props);
    if (spec.kind == "TTFText")
        return makeText(ecs, spec.props);
    if (spec.kind == "Texture")
        return makeTextureNode(ecs, spec.props);

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

    struct BuiltChild
    {
        EntityRef ent;
        const std::vector<AnchorSpec>* anchors;
    };
    std::vector<BuiltChild> built;
    built.reserve(spec.children.size());

    auto registerNamed = [&](EntityRef ent, const std::string& name) {
        if (name == RESERVED_MAIN or name == RESERVED_PARENT)
        {
            LOG_ERROR("Prefab Builder", "Reserved name used for child: '" << name << "'");
            prefab->addToPrefab(ent);
            return;
        }
        if (not name.empty())
        {
            if (nameToEntity.count(name))
                LOG_ERROR("Prefab Builder", "Name collision: '" << name << "'");
            prefab->addToPrefab(ent, name);
            nameToEntity[name] = ent;
        }
        else
        {
            prefab->addToPrefab(ent);
        }
    };

    for (const auto& child : spec.children)
    {
        std::visit([&](const auto& c) {
            using T = std::decay_t<decltype(c)>;
            EntityRef ent;
            if constexpr (std::is_same_v<T, NodeSpec>)
            {
                ent = buildNode(ecs, c);
            }
            else  // PrefabSpec
            {
                ent = buildPrefab(ecs, c);
            }
            if (not ent)
                return;

            registerNamed(ent, c.name);
            built.push_back({ent, &c.anchors});
        }, child);
    }

    if (mainEnt)
        applyAnchorsToEntity(mainEnt, spec.mainNode.anchors, nameToEntity);

    for (auto& b : built)
        applyAnchorsToEntity(b.ent, *b.anchors, nameToEntity);

    // Root-level anchors — applied to the container itself. Targets are looked up first in
    // the local scope (the prefab's own named children), then in the global EntityNameSystem.
    applyAnchorsToEntity(container.entity, spec.anchors, nameToEntity);

    return container.entity;
}
}
