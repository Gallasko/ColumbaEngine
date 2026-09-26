#include "prefabbuilder.h"

#include "ECS/entitysystem.h"
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
    constexpr std::string_view LAYOUT_PREFIX  = "Layout:";
    constexpr const char*      RESERVED_MAIN   = "main";
    constexpr const char*      RESERVED_PARENT = "parent";
    constexpr const char*      MAIN_ENTITY_KEY = "MainEntity";

    bool startsWith(const std::string& s, std::string_view prefix)
    {
        return s.size() >= prefix.size() and std::string_view(s).substr(0, prefix.size()) == prefix;
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

    // ---- Inheritance ----------------------------------------------------------------------

    // `inherited` (the enclosing factory's childDefaults) goes UNDER the child's own props:
    // an explicit child value always wins.
    void applyInherited(NodeSpec& child, const ElementMap& inherited)
    {
        for (const auto& kv : inherited)
        {
            if (child.props.find(kv.first) == child.props.end())
                child.props[kv.first] = kv.second;
        }
    }

    // What this node forwards to its children: its own childDefaults layered over what it
    // inherited itself, so a kind that declares none still passes its parent's down.
    ElementMap forwardedDefaults(const ElementMap& inherited, const ElementMap& own)
    {
        ElementMap out = inherited;

        for (const auto& kv : own)
            out[kv.first] = kv.second;

        return out;
    }

    // Realise the leaf by looking up the kind in the PrefabFactoryRegistry. Built-in
    // primitives (Shape2D / TTFText / Texture) are registered alongside user factories — there
    // is no separate hardcoded dispatch. Returns an empty result if kind is empty or unknown.
    FactoryResult realiseLeaf(EntitySystem* ecs, const NodeSpec& spec, BuildContext& ctx)
    {
        if (spec.kind.empty())
            return FactoryResult{};

        auto* registry = ecs->getSystem<PrefabFactoryRegistry>();
        if (not registry)
        {
            LOG_ERROR("Prefab Builder", "No PrefabFactoryRegistry available to realise kind: " << spec.kind);
            return FactoryResult{};
        }

        return registry->buildEx(spec, ctx);
    }

    // For a child that was itself wrapped in a Prefab, look up its inner mainEntity so the
    // parent's name map exposes the leaf (not the wrap). Falls back to the entity itself when
    // there's no mainEntity (e.g. layouts or empty-kind wraps).
    //
    // Only unwraps "trivial" wraps — those whose Prefab contains nothing but the mainEntity.
    // Composite wraps (mainEntity + sibling children) are kept whole so callers can traverse
    // the sub-tree via `wrap->get<Prefab>()->getEntity("childName")`. This makes nested-prefab
    // access work both ways: shapeNode("bg") in a parent exposes bg's Simple2DObject directly,
    // while a kind=Shape2D + children composite exposes its inner namedChildren via the wrap.
    EntityRef unwrapToMain(EntityRef ent)
    {
        if (not ent or not ent->has<Prefab>())
            return ent;
        auto p = ent->get<Prefab>();

        if (p->childrenIds.size() != 1)
            return ent;

        auto inner = p->getEntity(MAIN_ENTITY_KEY);
        return inner ? inner : ent;
    }

    // Forward declaration — recursive entry. `inherited` is what the parent forwards to this
    // node's children (already applied to `spec.props` by the caller).
    EntityRef buildNodeImpl(EntitySystem* ecs, const NodeSpec& spec, BuildContext& ctx,
                            const ElementMap& inherited, bool applyOwnAnchors);

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

    bool addToSlot(EntityRef slot, EntityRef child)
    {
        if (slot->has<VerticalLayout>())
        {
            slot->get<VerticalLayout>()->addEntity(child);
            return true;
        }

        if (slot->has<HorizontalLayout>())
        {
            slot->get<HorizontalLayout>()->addEntity(child);
            return true;
        }

        return false;
    }

    EntityRef buildLayoutNode(EntitySystem* ecs, const NodeSpec& spec, BuildContext& ctx,
                              const ElementMap& inherited, bool applyOwnAnchors)
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
            HorizontalLayout* layoutPtr = cl.get<HorizontalLayout>();
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

        std::unordered_map<std::string, EntityRef> nameToEntity;
        nameToEntity[RESERVED_PARENT] = layoutEnt;
        nameToEntity[RESERVED_MAIN]   = layoutEnt;   // for layouts, "main" == "parent"

        // A layout declares no childDefaults of its own; it forwards what it inherited.
        for (const auto& childSpec : spec.children)
        {
            NodeSpec child = childSpec;
            applyInherited(child, inherited);

            EntityRef childEnt = buildNodeImpl(ecs, child, ctx, inherited, /*applyOwnAnchors=*/false);
            if (not childEnt)
                continue;

            if (not child.name.empty())
                nameToEntity[child.name] = unwrapToMain(childEnt);

            if (orient == "Horizontal")
                layoutEnt->get<HorizontalLayout>()->addEntity(childEnt);
            else
                layoutEnt->get<VerticalLayout>()->addEntity(childEnt);
        }

        if (applyOwnAnchors)
            applyAnchorsToEntity(layoutEnt, spec.anchors, nameToEntity);

        return layoutEnt;
    }

    // Always-wrap path. `leaf` is what the factory produced for `spec` (its entity may be
    // empty if kind=="" — then the wrap has no mainEntity, just children as siblings).
    EntityRef buildPrefabWrapping(EntitySystem* ecs, const NodeSpec& spec, FactoryResult& leaf, BuildContext& ctx,
                                  const ElementMap& inherited, bool applyOwnAnchors)
    {
        auto container = makeAnchoredPrefab(ecs);
        auto prefab = container.get<Prefab>();

        // Placement: the leaf is anchored to the container's top-left, so `x` / `y` move the
        // container (moving the leaf alone would be overridden by that anchor).
        if (hasParam(spec.props, "x"))
            container.get<PositionComponent>()->setX(getParamFloat(spec.props, "x"));
        if (hasParam(spec.props, "y"))
            container.get<PositionComponent>()->setY(getParamFloat(spec.props, "y"));

        std::unordered_map<std::string, EntityRef> nameToEntity;
        nameToEntity[RESERVED_PARENT] = container.entity;

        EntityRef leafEnt = leaf.entity;

        if (leafEnt)
        {
            // Wire the leaf as the prefab's mainEntity (auto-anchored top-left of container,
            // container's size constrained to leaf size — see PrefabSystem::onEvent(SetMainEntityEvent)).
            prefab->setMainEntity(container.entity, leafEnt);
            nameToEntity[RESERVED_MAIN] = leafEnt;

            // Expose the leaf under spec.name so `container->get<Prefab>()->getEntity(spec.name)`
            // returns the leaf directly.
            if (not spec.name.empty())
            {
                prefab->addToPrefab(leafEnt, spec.name);
                nameToEntity[spec.name] = leafEnt;
            }
        }
        else
        {
            // No leaf — "main" resolves to the container itself so child anchors targeting "main"
            // still work (the container is the only thing they can reasonably mean).
            nameToEntity[RESERVED_MAIN] = container.entity;
        }

        // The factory's rich result, exposed by the node's name.
        if (ctx.handles and leaf.handle.has_value())
        {
            if (spec.name.empty())
                LOG_WARNING("Prefab Builder", "Kind '" << spec.kind << "' returned a handle but the node has no name; the handle is dropped");
            else
                (*ctx.handles)[spec.name] = std::move(leaf.handle);
        }

        // A slot routes every child into the factory's layout instead of the sibling path.
        EntityRef slot = leaf.slot;
        if (slot and not slot->has<VerticalLayout>() and not slot->has<HorizontalLayout>())
        {
            LOG_ERROR("Prefab Builder", "Kind '" << spec.kind << "' returned a slot without a layout component; children are anchored as siblings instead");
            slot = EntityRef{};
        }
        const bool useSlot = static_cast<bool>(slot);

        const ElementMap childInherited = forwardedDefaults(inherited, leaf.childDefaults);

        struct BuiltChild
        {
            EntityRef wrap;    // structural entity returned by buildNodeImpl (Prefab container or layout)
            std::vector<AnchorSpec> anchors;
        };

        std::vector<BuiltChild> built;
        built.reserve(spec.children.size());

        // Register a built child: the wrap goes into the parent prefab's childrenIds for
        // cleanup/visibility propagation; the inner leaf is exposed under `name` in both
        // `namedChildrenIds` and the local `nameToEntity` for ergonomic access by callers.
        auto registerNamed = [&](EntityRef wrap, const std::string& name) {
            if (name == RESERVED_MAIN or name == RESERVED_PARENT)
            {
                LOG_ERROR("Prefab Builder", "Reserved name used for child: '" << name << "'");
                prefab->addToPrefab(wrap);
                return;
            }

            prefab->addToPrefab(wrap);  // wrap tracked in childrenIds

            if (name.empty())
                return;

            if (nameToEntity.count(name))
                LOG_ERROR("Prefab Builder", "Name collision: '" << name << "'");

            EntityRef leafOfChild = unwrapToMain(wrap);
            prefab->namedChildrenIds[name] = leafOfChild;
            nameToEntity[name] = leafOfChild;
        };

        bool warnedSlotAnchors = false;

        for (const auto& childSpec : spec.children)
        {
            NodeSpec child = childSpec;
            applyInherited(child, childInherited);

            EntityRef ent = buildNodeImpl(ecs, child, ctx, childInherited, /*applyOwnAnchors=*/false);
            if (not ent)
                continue;

            registerNamed(ent, child.name);

            if (useSlot)
            {
                if (not child.anchors.empty() and not warnedSlotAnchors)
                {
                    LOG_WARNING("Prefab Builder", "Children of '" << spec.kind << "' go into its slot; their anchors are ignored");
                    warnedSlotAnchors = true;
                }

                addToSlot(slot, ent);
            }
            else
            {
                built.push_back({ent, child.anchors});
            }
        }

        // Flow synthesis — auto-anchor children with empty user-anchor lists. See prefabspec.h
        // doc comment for the rule (chains both axes through previous sibling for linear deps).
        if (not useSlot and spec.flow != Flow::None and leafEnt)
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

                prevFlow = b.wrap;
            }
        }

        // Apply child anchors (user-provided OR flow-synthesised) to the WRAP entities; targets
        // resolve via nameToEntity (sibling names map to inner leaves; geometrically equivalent).
        for (auto& b : built)
            applyAnchorsToEntity(b.wrap, b.anchors, nameToEntity);

        // Apply this node's own anchors to the container — top-level only; nested cases let the
        // parent apply them using ITS scope.
        if (applyOwnAnchors)
            applyAnchorsToEntity(container.entity, spec.anchors, nameToEntity);

        return container.entity;
    }

    EntityRef buildNodeImpl(EntitySystem* ecs, const NodeSpec& spec, BuildContext& ctx,
                            const ElementMap& inherited, bool applyOwnAnchors)
    {
        // Layout kinds: produce a single layout-bearing entity; children added via addEntity.
        if (startsWith(spec.kind, LAYOUT_PREFIX))
            return buildLayoutNode(ecs, spec, ctx, inherited, applyOwnAnchors);

        // Everything else: realise the kind's leaf (may be empty for kind=="") and wrap it.
        ctx.inherited = inherited;
        FactoryResult leaf = realiseLeaf(ecs, spec, ctx);
        return buildPrefabWrapping(ecs, spec, leaf, ctx, inherited, applyOwnAnchors);
    }
}

PrefabBuildResult buildTree(EntitySystem* ecs, const NodeSpec& spec)
{
    PrefabBuildResult out;

    BuildContext ctx;
    ctx.handles = &out.handles;

    out.root = buildNodeImpl(ecs, spec, ctx, ElementMap{}, /*applyOwnAnchors=*/true);
    return out;
}

EntityRef buildNode(EntitySystem* ecs, const NodeSpec& spec)
{
    return buildTree(ecs, spec).root;
}
}
