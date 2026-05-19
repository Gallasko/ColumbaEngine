#include "prefabbuilder.h"

#include "ECS/entitysystem.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"
#include "UI/ttftext.h"
#include "UI/prefab.h"
#include "UI/prefabfactory.h"
#include "UI/sizer.h"
#include "Systems/coresystems.h"

#include <string_view>
#include <unordered_map>
#include <utility>

namespace pg
{
namespace
{
    constexpr std::string_view FACTORY_PREFIX = "Factory:";
    constexpr std::string_view LAYOUT_PREFIX  = "Layout:";
    constexpr const char*      RESERVED_MAIN   = "main";
    constexpr const char*      RESERVED_PARENT = "parent";

    bool startsWith(const std::string& s, std::string_view prefix)
    {
        return s.size() >= prefix.size() and std::string_view(s).substr(0, prefix.size()) == prefix;
    }

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

    EntityRef makeShape2D(EntitySystem* ecs, const ElementMap& p)
    {
        const Shape2D shape = shapeFromString(getParam(p, "shape", "Square"));
        const float w = getParam(p, "width",  0.0f);
        const float h = getParam(p, "height", 0.0f);
        const auto color = readColor(p);
        auto cl = makeUiSimple2DShape(ecs, shape, w, h, color);
        applyShared(cl.entity, p);
        return cl.entity;
    }

    EntityRef makeText(EntitySystem* ecs, const ElementMap& p)
    {
        const std::string font = getParam(p, "font");
        const std::string text = getParam(p, "text");
        const float scale = getParam(p, "scale", 1.0f);
        const float x = getParam(p, "x", 0.0f);
        const float y = getParam(p, "y", 0.0f);
        const float z = getParam(p, "z", 0.0f);
        const auto color = readColor(p);
        auto cl = makeTTFText(ecs, x, y, z, font, text, scale, color);
        if (hasParam(p, "viewport"))
            cl.get<ViewportComponent>()->setViewport(getParam(p, "viewport"));
        if (hasParam(p, "visibility"))
            cl.get<PositionComponent>()->setVisibility(getParam(p, "visibility", true));
        return cl.entity;
    }

    EntityRef makeTextureNode(EntitySystem* ecs, const ElementMap& p)
    {
        const std::string texName = getParam(p, "texture", "NoneIcon");
        const float w = getParam(p, "width",  0.0f);
        const float h = getParam(p, "height", 0.0f);
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

    // Realise the leaf entity for a primitive `kind`. Returns empty for kind=="" (no leaf).
    EntityRef realiseLeaf(EntitySystem* ecs, const NodeSpec& spec)
    {
        if (spec.kind.empty())
            return EntityRef{};

        if (startsWith(spec.kind, FACTORY_PREFIX))
            return invokeFactory(ecs,
                std::string_view(spec.kind).substr(FACTORY_PREFIX.size()),
                spec.props);

        if (spec.kind == "Shape2D") return makeShape2D(ecs, spec.props);
        if (spec.kind == "TTFText") return makeText(ecs, spec.props);
        if (spec.kind == "Texture") return makeTextureNode(ecs, spec.props);

        LOG_ERROR("Prefab Builder", "Unknown node kind: '" << spec.kind << "'");
        return EntityRef{};
    }

    // Forward declaration — recursive entry.
    EntityRef buildNodeImpl(EntitySystem* ecs, const NodeSpec& spec, bool applyOwnAnchors);

    // Configure layout knobs from props.
    template<typename LayoutComp>
    void configureLayout(LayoutComp* layout, const ElementMap& p)
    {
        if (hasParam(p, "spacing"))
            layout->spacing = getParam(p, "spacing");
        if (hasParam(p, "fitToAxis"))
            layout->fitToAxis = getParam(p, "fitToAxis", false);
        if (hasParam(p, "spaced"))
            layout->spaced = getParam(p, "spaced", false);
        if (hasParam(p, "stickToEnd"))
            layout->stickToEnd = getParam(p, "stickToEnd", false);
    }

    EntityRef buildLayoutNode(EntitySystem* ecs, const NodeSpec& spec, bool applyOwnAnchors)
    {
        const std::string_view orient = std::string_view(spec.kind).substr(LAYOUT_PREFIX.size());

        const float x = getParam(spec.props, "x", 0.0f);
        const float y = getParam(spec.props, "y", 0.0f);
        const float w = getParam(spec.props, "width",  0.0f);
        const float h = getParam(spec.props, "height", 0.0f);
        const bool scrollable = getParam(spec.props, "scrollable", false);

        EntityRef layoutEnt;

        if (orient == "Horizontal")
        {
            auto cl = makeHorizontalLayout(ecs, x, y, w, h, scrollable);
            HorizontalLayout* layoutPtr = cl.get<HorizontalLayout>();   // implicit CompRef -> T*
            configureLayout(layoutPtr, spec.props);
            layoutEnt = cl.entity;
        }
        else if (orient == "Vertical")
        {
            auto cl = makeVerticalLayout(ecs, x, y, w, h, scrollable);
            VerticalLayout* layoutPtr = cl.get<VerticalLayout>();
            configureLayout(layoutPtr, spec.props);
            layoutEnt = cl.entity;
        }
        else
        {
            LOG_ERROR("Prefab Builder", "Unknown layout kind: '" << spec.kind << "'");
            return EntityRef{};
        }

        if (hasParam(spec.props, "z"))
            layoutEnt->get<PositionComponent>()->setZ(getParam(spec.props, "z"));
        if (hasParam(spec.props, "visibility"))
            layoutEnt->get<PositionComponent>()->setVisibility(getParam(spec.props, "visibility", true));

        // Local scope so this layout's own spec.anchors can reference its children.
        std::unordered_map<std::string, EntityRef> nameToEntity;
        nameToEntity[RESERVED_PARENT] = layoutEnt;
        nameToEntity[RESERVED_MAIN]   = layoutEnt;   // for layouts, "main" == "parent" (no separate leaf)

        // Build children and register them via the layout's addEntity (NOT anchored to siblings —
        // the LayoutSystem reflows them each tick based on the layout's orientation/spacing/etc.).
        for (const auto& child : spec.children)
        {
            EntityRef childEnt = buildNodeImpl(ecs, child, /*applyOwnAnchors=*/false);
            if (not childEnt)
                continue;

            if (not child.name.empty())
                nameToEntity[child.name] = childEnt;

            if (orient == "Horizontal")
                layoutEnt->get<HorizontalLayout>()->addEntity(childEnt);
            else
                layoutEnt->get<VerticalLayout>()->addEntity(childEnt);
        }

        if (applyOwnAnchors)
            applyAnchorsToEntity(layoutEnt, spec.anchors, nameToEntity);

        return layoutEnt;
    }

    // Wrap `leafEnt` (the mainEntity, may be empty) in a Prefab container, then build/register
    // spec.children[siblingStartIdx..] as siblings. `mainName` is the name to register the leaf
    // under inside the Prefab (use spec.name for leaf-kind shorthand, children[0].name for
    // kind="Prefab" canonical).
    EntityRef buildPrefabWrapping(EntitySystem* ecs, const NodeSpec& spec, EntityRef leafEnt,
                                  const std::string& mainName, size_t siblingStartIdx, bool applyOwnAnchors)
    {
        auto container = makeAnchoredPrefab(ecs);
        auto prefab = container.get<Prefab>();

        std::unordered_map<std::string, EntityRef> nameToEntity;
        nameToEntity[RESERVED_PARENT] = container.entity;

        if (leafEnt)
        {
            // Wire the leaf as the prefab's mainEntity (auto-anchored top-left of container,
            // container's size constrained to leaf size — see PrefabSystem::onEvent(SetMainEntityEvent)).
            prefab->setMainEntity(leafEnt);
            nameToEntity[RESERVED_MAIN] = leafEnt;

            // Also expose the leaf under its name so `container->get<Prefab>()->getEntity(name)`
            // returns the leaf.
            if (not mainName.empty())
            {
                prefab->addToPrefab(leafEnt, mainName);
                nameToEntity[mainName] = leafEnt;
            }
        }
        else
        {
            // No leaf — "main" resolves to the container itself so child anchors targeting "main"
            // still work (the container is the only thing they can reasonably mean).
            nameToEntity[RESERVED_MAIN] = container.entity;
        }

        // Track each built child + its anchor list (anchors are applied AFTER all children are
        // built so that anchors can reference siblings declared later in `spec.children`).
        struct BuiltChild
        {
            EntityRef ent;
            // Owned by value so the flow pass below can swap in synthesised anchors for children
            // whose user-supplied anchor list was empty.
            std::vector<AnchorSpec> anchors;
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

        for (size_t i = siblingStartIdx; i < spec.children.size(); ++i)
        {
            const auto& child = spec.children[i];
            EntityRef ent = buildNodeImpl(ecs, child, /*applyOwnAnchors=*/false);
            if (not ent)
                continue;

            registerNamed(ent, child.name);
            built.push_back({ent, child.anchors});
        }

        // Flow synthesis — auto-anchor children with empty user-anchor lists. Children with any
        // user anchors are transparent to the chain. Cross-axis chains through the previous
        // in-flow sibling (not always to `main`) so the dependency graph stays linear, which keeps
        // the PositionSystem resolution order deterministic and lets hidden flow children collapse.
        if (spec.flow != Flow::None and leafEnt)
        {
            EntityRef prevFlow{};
            for (auto& b : built)
            {
                if (not b.anchors.empty())
                    continue;

                if (spec.flow == Flow::Horizontal)
                {
                    if (prevFlow)
                    {
                        b.anchors.emplace_back(prevFlow.id, AnchorType::Top,  AnchorType::Top,   0.0f);
                        b.anchors.emplace_back(prevFlow.id, AnchorType::Left, AnchorType::Right, spec.spacing);
                    }
                    else
                    {
                        b.anchors.emplace_back(std::string(RESERVED_MAIN), AnchorType::Top,  spec.padding);
                        b.anchors.emplace_back(std::string(RESERVED_MAIN), AnchorType::Left, spec.padding);
                    }
                }
                else  // Flow::Vertical
                {
                    if (prevFlow)
                    {
                        b.anchors.emplace_back(prevFlow.id, AnchorType::Left, AnchorType::Left,   0.0f);
                        b.anchors.emplace_back(prevFlow.id, AnchorType::Top,  AnchorType::Bottom, spec.spacing);
                    }
                    else
                    {
                        b.anchors.emplace_back(std::string(RESERVED_MAIN), AnchorType::Left, spec.padding);
                        b.anchors.emplace_back(std::string(RESERVED_MAIN), AnchorType::Top,  spec.padding);
                    }
                }

                prevFlow = b.ent;
            }
        }

        // Apply child anchors (user-provided OR flow-synthesised) using local scope.
        for (auto& b : built)
            applyAnchorsToEntity(b.ent, b.anchors, nameToEntity);

        // Apply this node's own anchors to the container — top-level only; for nested cases
        // the parent applies them using ITS scope (siblings of this node).
        if (applyOwnAnchors)
            applyAnchorsToEntity(container.entity, spec.anchors, nameToEntity);

        return container.entity;
    }

    EntityRef buildNodeImpl(EntitySystem* ecs, const NodeSpec& spec, bool applyOwnAnchors)
    {
        // Layout kinds: produce a single layout-bearing entity; children added via addEntity.
        if (startsWith(spec.kind, LAYOUT_PREFIX))
            return buildLayoutNode(ecs, spec, applyOwnAnchors);

        // Canonical wrap: kind="Prefab" always produces a Prefab container, regardless of
        // children count. children[0] (if present) becomes the mainEntity; rest are siblings.
        if (spec.kind == "Prefab")
        {
            EntityRef leafEnt;
            std::string mainName;
            size_t siblingStartIdx = 0;
            if (not spec.children.empty())
            {
                const auto& mainChild = spec.children[0];
                leafEnt = buildNodeImpl(ecs, mainChild, /*applyOwnAnchors=*/false);
                mainName = mainChild.name;
                siblingStartIdx = 1;
            }
            return buildPrefabWrapping(ecs, spec, leafEnt, mainName, siblingStartIdx, applyOwnAnchors);
        }

        // Primitive realisation (may be empty if kind=="").
        EntityRef leaf = realiseLeaf(ecs, spec);

        if (spec.children.empty())
        {
            // Pure leaf — anchors applied here only for top-level. Nested leaves get their anchors
            // applied by the parent prefab's `applyAnchorsToEntity` pass using the parent's scope.
            if (applyOwnAnchors and leaf)
            {
                std::unordered_map<std::string, EntityRef> emptyScope;
                applyAnchorsToEntity(leaf, spec.anchors, emptyScope);
            }
            return leaf;
        }

        // Shorthand wrap: leaf-kind + children -> wrap with leaf as mainEntity, all children siblings.
        // `leaf` may be empty (kind=="" -> bare container with children).
        return buildPrefabWrapping(ecs, spec, leaf, spec.name, /*siblingStartIdx=*/0, applyOwnAnchors);
    }
}

EntityRef buildNode(EntitySystem* ecs, const NodeSpec& spec)
{
    return buildNodeImpl(ecs, spec, /*applyOwnAnchors=*/true);
}
}
