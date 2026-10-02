#include "stdafx.h"

#include "prefabbuilder.h"

#include "logger.h"
#include "ECS/entitysystem.h"
#include "UI/prefab.h"
#include "UI/prefabfactory.h"
#include "UI/sizer.h"
#include "Systems/coresystems.h"
#include "Components/ThemeComponent.generated.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Prefab Builder";

        const std::string LayoutPrefix = "Layout:";
        const std::string ReservedMain = "main";
        const std::string ReservedParent = "parent";
        const std::string MainEntityKey = "MainEntity";

        using NameMap = std::unordered_map<std::string, EntityRef>;

        bool startsWith(const std::string& s, const std::string& prefix)
        {
            return s.size() >= prefix.size() and s.compare(0, prefix.size(), prefix) == 0;
        }

        bool isReservedName(const std::string& name)
        {
            return name == ReservedMain or name == ReservedParent;
        }

        PosAnchor pickSide(UiAnchor* a, _unique_id id, AnchorType side)
        {
            switch (side)
            {
            case AnchorType::Top:
                return a->top;

            case AnchorType::Bottom:
                return a->bottom;

            case AnchorType::Left:
                return a->left;

            case AnchorType::Right:
                return a->right;

            case AnchorType::Width:
                return PosAnchor{id, AnchorType::Width};

            case AnchorType::Height:
                return PosAnchor{id, AnchorType::Height};

            case AnchorType::VerticalCenter:
                return a->verticalCenter;

            case AnchorType::HorizontalCenter:
                return a->horizontalCenter;

            default:
                return PosAnchor{};
            }
        }

        // Three-tier resolution: explicit id, then local sibling scope, then global EntityNameSystem
        EntityRef resolveByIdOrName(EntitySystem* ecs, _unique_id targetId, const std::string& targetName, const NameMap& scope)
        {
            if (targetId != 0)
                return ecs->getEntity(targetId);

            if (not targetName.empty())
            {
                auto it = scope.find(targetName);

                if (it != scope.end())
                    return it->second;

                auto names = ecs->getSystem<EntityNameSystem>();

                if (names)
                {
                    const _unique_id id = names->getEntityId(targetName);

                    if (id != 0)
                        return ecs->getEntity(id);
                }
            }

            return EntityRef{};
        }

        void applyAnchorsToEntity(EntityRef ent, const std::vector<AnchorSpec>& anchors, NameMap& nameToEntity)
        {
            if (not ent or not ent->has<UiAnchor>())
                return;

            auto ecs = ent.ecsRef;
            auto anchor = ent->get<UiAnchor>();

            for (const auto& a : anchors)
            {
                // Empty target (no id, no name) means the anchor is unspecified, skip silently
                if (a.targetId == 0 and a.target.empty())
                    continue;

                EntityRef target = resolveByIdOrName(ecs, a.targetId, a.target, nameToEntity);

                if (not target)
                {
                    LOG_ERROR(DOM, "Anchor target not found: id=" << a.targetId << " name='" << a.target << "'");

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

        // The inherited props (the enclosing factory's childDefaults) go under the child's own props:
        // an explicit child value always wins
        void applyInherited(NodeSpec& child, const ElementMap& inherited)
        {
            for (const auto& kv : inherited)
            {
                if (child.props.find(kv.first) == child.props.end())
                    child.props[kv.first] = kv.second;
            }
        }

        // What a node forwards to its children: its own childDefaults layered over what it inherited,
        // so a kind that declares none still passes its parent's down
        ElementMap forwardedDefaults(const ElementMap& inherited, const ElementMap& own)
        {
            ElementMap out = inherited;

            for (const auto& kv : own)
                out[kv.first] = kv.second;

            return out;
        }

        // Realise the leaf through the PrefabFactoryRegistry (built-in primitives are registered there too).
        // Returns an empty result if the kind is empty or unknown
        FactoryResult realiseLeaf(EntitySystem* ecs, const NodeSpec& spec)
        {
            if (spec.kind.empty())
                return FactoryResult{};

            auto registry = ecs->getSystem<PrefabFactoryRegistry>();

            if (not registry)
            {
                LOG_ERROR(DOM, "No PrefabFactoryRegistry available to realise kind: " << spec.kind);

                return FactoryResult{};
            }

            FactoryResult result = registry->build(spec);

            // The node's theme element keys the leaf into the theme: painted now, repainted on every switch
            if (result.entity and not spec.theme.empty())
                ecs->attach<ThemeComponent>(result.entity, spec.theme);

            return result;
        }

        // The entity a parent exposes under a child's name: the child's main entity rather than its wrap.
        // A wrap unwraps when it holds nothing but the main entity, or when the main entity carries a
        // Prefab of its own (a composite piece, which received the wrap's names in buildPrefabWrapping).
        // Layouts, empty-kind wraps and primitive + children composites stay as they are
        EntityRef unwrapToMain(EntityRef ent)
        {
            if (not ent or not ent->has<Prefab>())
                return ent;

            auto p = ent->get<Prefab>();

            auto it = p->namedChildrenIds.find(MainEntityKey);

            if (it == p->namedChildrenIds.end() or it->second.id == 0)
                return ent;

            EntityRef inner = it->second;

            if (p->childrenIds.size() == 1 or inner->has<Prefab>())
                return inner;

            return ent;
        }

        // Recursive entry. The inherited props are what the parent forwards to this node's children
        // (already applied to spec.props by the caller). A layout node has no Prefab of its own, so it
        // hands the names of its children up through liftedNames for the nearest enclosing Prefab
        EntityRef buildNodeImpl(EntitySystem* ecs, const NodeSpec& spec, const ElementMap& inherited, bool applyOwnAnchors, NameMap* liftedNames);

        template <typename LayoutComp>
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

        void addToSlot(EntityRef slot, EntityRef child)
        {
            if (slot->has<VerticalLayout>())
                slot->get<VerticalLayout>()->addEntity(child);
            else if (slot->has<HorizontalLayout>())
                slot->get<HorizontalLayout>()->addEntity(child);
        }

        EntityRef buildLayoutNode(EntitySystem* ecs, const NodeSpec& spec, const ElementMap& inherited, NameMap& names)
        {
            const std::string orient = spec.kind.substr(LayoutPrefix.size());

            const float x = getParam(spec.props, "x", 0.0f);
            const float y = getParam(spec.props, "y", 0.0f);
            const float w = getParam(spec.props, "width", 0.0f);
            const float h = getParam(spec.props, "height", 0.0f);
            const bool scrollable = getParam(spec.props, "scrollable", false);

            EntityRef layoutEnt;

            if (orient == "Horizontal")
            {
                auto cl = makeHorizontalLayout(ecs, x, y, w, h, scrollable);

                HorizontalLayout* layout = cl.get<HorizontalLayout>();

                configureLayout(layout, spec.props);

                layoutEnt = cl.entity;
            }
            else if (orient == "Vertical")
            {
                auto cl = makeVerticalLayout(ecs, x, y, w, h, scrollable);

                VerticalLayout* layout = cl.get<VerticalLayout>();

                configureLayout(layout, spec.props);

                layoutEnt = cl.entity;
            }
            else
            {
                LOG_ERROR(DOM, "Unknown layout kind: '" << spec.kind << "'");

                return EntityRef{};
            }

            if (hasParam(spec.props, "z"))
                layoutEnt->get<PositionComponent>()->setZ(getParam(spec.props, "z"));

            if (hasParam(spec.props, "visibility"))
                layoutEnt->get<PositionComponent>()->setVisibility(getParam(spec.props, "visibility", true));

            bool warnedAnchors = false;

            // A layout declares no childDefaults of its own, it forwards what it inherited
            for (const auto& childSpec : spec.children)
            {
                NodeSpec child = childSpec;
                applyInherited(child, inherited);

                EntityRef childEnt = buildNodeImpl(ecs, child, inherited, false, &names);

                if (not childEnt)
                    continue;

                if (not child.anchors.empty() and not warnedAnchors)
                {
                    LOG_WARNING(DOM, "Children of '" << spec.kind << "' are placed by the layout; their anchors are ignored");

                    warnedAnchors = true;
                }

                // The first node to take a name keeps it
                if (isReservedName(child.name))
                    LOG_ERROR(DOM, "Reserved name used for child: '" << child.name << "'");
                else if (names.count(child.name))
                    LOG_ERROR(DOM, "Name collision: '" << child.name << "'");
                else if (not child.name.empty())
                    names[child.name] = unwrapToMain(childEnt);

                if (orient == "Horizontal")
                    layoutEnt->get<HorizontalLayout>()->addEntity(childEnt);
                else
                    layoutEnt->get<VerticalLayout>()->addEntity(childEnt);
            }

            return layoutEnt;
        }

        // The names a layout lifted, registered on the enclosing prefab and its anchor scope
        void adoptLiftedNames(CompRef<Prefab> prefab, NameMap& nameToEntity, const NameMap& lifted)
        {
            for (const auto& [name, ent] : lifted)
            {
                // Already reported by the layout that lifted it
                if (isReservedName(name))
                    continue;

                if (nameToEntity.count(name))
                {
                    LOG_ERROR(DOM, "Name collision: '" << name << "'");

                    continue;
                }

                prefab->namedChildrenIds[name] = ent;
                nameToEntity[name] = ent;
            }
        }

        // Always-wrap path. The leaf is what the factory produced for the spec; its entity is empty
        // for an empty kind, the wrap then has no main entity, only children as siblings
        EntityRef buildPrefabWrapping(EntitySystem* ecs, const NodeSpec& spec, FactoryResult& leaf, const ElementMap& inherited, bool applyOwnAnchors)
        {
            auto container = makeAnchoredPrefab(ecs);
            auto prefab = container.get<Prefab>();

            // The leaf is anchored to the container's top-left, so x / y move the container
            // (moving the leaf alone would be overridden by that anchor)
            if (hasParam(spec.props, "x"))
                container.get<PositionComponent>()->setX(getParamFloat(spec.props, "x"));

            if (hasParam(spec.props, "y"))
                container.get<PositionComponent>()->setY(getParamFloat(spec.props, "y"));

            NameMap nameToEntity;
            nameToEntity[ReservedParent] = container.entity;

            EntityRef leafEnt = leaf.entity;

            if (leafEnt)
            {
                // The main entity follows the container's z from here on, so the container takes the z
                // the factory gave its leaf. Left at 0 it would flatten every node of the tree onto one depth
                if (leafEnt->has<PositionComponent>())
                    container.get<PositionComponent>()->setZ(leafEnt->get<PositionComponent>()->z);

                // The leaf becomes the main entity: anchored top-left of the container, which takes its size
                // (see PrefabSystem::onEvent(SetMainEntityEvent))
                prefab->setMainEntity(container.entity, leafEnt);
                nameToEntity[ReservedMain] = leafEnt;

                // Expose the leaf under spec.name so getEntity(spec.name) on the container returns it directly
                if (not spec.name.empty())
                {
                    prefab->addToPrefab(leafEnt, spec.name);
                    nameToEntity[spec.name] = leafEnt;
                }
            }
            else
            {
                // No leaf: main resolves to the container itself so child anchors targeting main still work
                nameToEntity[ReservedMain] = container.entity;
            }

            // A slot routes every child into the factory's layout instead of the sibling path
            EntityRef slot = leaf.slot;

            if (slot and not slot->has<VerticalLayout>() and not slot->has<HorizontalLayout>())
            {
                LOG_ERROR(DOM, "Kind '" << spec.kind << "' returned a slot without a layout component; children are anchored as siblings instead");

                slot = EntityRef{};
            }

            const bool useSlot = slot.id != 0;

            const ElementMap childInherited = forwardedDefaults(inherited, leaf.childDefaults);

            struct BuiltChild
            {
                // Structural entity returned by buildNodeImpl (Prefab container or layout)
                EntityRef wrap;

                std::vector<AnchorSpec> anchors;
            };

            std::vector<BuiltChild> built;
            built.reserve(spec.children.size());

            // The wrap goes into the parent prefab's childrenIds for cleanup and visibility propagation,
            // the inner leaf is exposed under its name in namedChildrenIds and in the local anchor scope
            auto registerNamed = [&](EntityRef wrap, const std::string& name) {
                prefab->addToPrefab(wrap);

                if (isReservedName(name))
                {
                    LOG_ERROR(DOM, "Reserved name used for child: '" << name << "'");

                    return;
                }

                if (name.empty())
                    return;

                // The first node to take a name keeps it, the node's own name included
                if (nameToEntity.count(name))
                {
                    LOG_ERROR(DOM, "Name collision: '" << name << "'");

                    return;
                }

                EntityRef leafOfChild = unwrapToMain(wrap);

                prefab->namedChildrenIds[name] = leafOfChild;
                nameToEntity[name] = leafOfChild;
            };

            bool warnedSlotAnchors = false;

            for (const auto& childSpec : spec.children)
            {
                NodeSpec child = childSpec;
                applyInherited(child, childInherited);

                NameMap lifted;
                EntityRef ent = buildNodeImpl(ecs, child, childInherited, false, &lifted);

                if (not ent)
                    continue;

                registerNamed(ent, child.name);
                adoptLiftedNames(prefab, nameToEntity, lifted);

                if (useSlot)
                {
                    if (not child.anchors.empty() and not warnedSlotAnchors)
                    {
                        LOG_WARNING(DOM, "Children of '" << spec.kind << "' go into its slot; their anchors are ignored");

                        warnedSlotAnchors = true;
                    }

                    addToSlot(slot, ent);
                }
                else
                {
                    built.push_back({ent, child.anchors});
                }
            }

            // Flow synthesis: auto-anchor the children with no anchors of their own, chained through
            // the previous in-flow sibling (see prefabspec.h for the rule). Without a leaf the chain
            // starts from the container, which is what main resolves to
            if (not useSlot and spec.flow != Flow::None)
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
                            b.anchors.emplace_back(prevFlow.id, AnchorType::Top, AnchorType::Top, 0.0f);
                            b.anchors.emplace_back(prevFlow.id, AnchorType::Left, AnchorType::Right, spec.spacing);
                        }
                        else
                        {
                            b.anchors.emplace_back(ReservedMain, AnchorType::Top, spec.padding);
                            b.anchors.emplace_back(ReservedMain, AnchorType::Left, spec.padding);
                        }
                    }
                    else
                    {
                        if (prevFlow)
                        {
                            b.anchors.emplace_back(prevFlow.id, AnchorType::Left, AnchorType::Left, 0.0f);
                            b.anchors.emplace_back(prevFlow.id, AnchorType::Top, AnchorType::Bottom, spec.spacing);
                        }
                        else
                        {
                            b.anchors.emplace_back(ReservedMain, AnchorType::Left, spec.padding);
                            b.anchors.emplace_back(ReservedMain, AnchorType::Top, spec.padding);
                        }
                    }

                    prevFlow = b.wrap;
                }
            }

            // Child anchors (user-provided or flow-synthesised) go on the wrap entities; sibling names
            // resolve to the inner leaves, which is geometrically equivalent
            for (auto& b : built)
                applyAnchorsToEntity(b.wrap, b.anchors, nameToEntity);

            // A composite piece (a leaf with a Prefab of its own) takes the wrap's names too, so the
            // name a parent exposes for this node resolves to the piece and still reaches its nested
            // nodes: getEntity("squire")->get<Prefab>()->getEntity("fed")
            if (leafEnt and leafEnt->has<Prefab>())
            {
                auto leafPrefab = leafEnt->get<Prefab>();

                for (const auto& [name, ent] : prefab->namedChildrenIds)
                {
                    if (name == MainEntityKey or name == spec.name)
                        continue;

                    if (leafPrefab->namedChildrenIds.count(name))
                        LOG_ERROR(DOM, "Name collision on '" << spec.kind << "': '" << name << "' is already a part of the piece");
                    else
                        leafPrefab->namedChildrenIds[name] = ent;
                }
            }

            // Own anchors are applied at top level only, a nested node lets its parent apply them in its scope
            if (applyOwnAnchors)
                applyAnchorsToEntity(container.entity, spec.anchors, nameToEntity);

            return container.entity;
        }

        EntityRef buildNodeImpl(EntitySystem* ecs, const NodeSpec& spec, const ElementMap& inherited, bool applyOwnAnchors, NameMap* liftedNames)
        {
            // Layout kinds produce a single layout-bearing entity, children are added via addEntity
            if (startsWith(spec.kind, LayoutPrefix))
            {
                NameMap local;

                return buildLayoutNode(ecs, spec, inherited, liftedNames ? *liftedNames : local);
            }

            // Everything else: realise the kind's leaf (may be empty) and wrap it
            FactoryResult leaf = realiseLeaf(ecs, spec);

            return buildPrefabWrapping(ecs, spec, leaf, inherited, applyOwnAnchors);
        }

        // A layout root gets the same Prefab container as every other node: the layout is its main
        // entity (the container follows the layout's size), the placement props and the root anchors
        // go on the container, and the layout's named children are registered on it
        EntityRef wrapLayoutRoot(EntitySystem* ecs, const NodeSpec& spec, EntityRef layoutEnt, const NameMap& names)
        {
            auto container = makeAnchoredPrefab(ecs);
            auto prefab = container.get<Prefab>();

            if (hasParam(spec.props, "x"))
                container.get<PositionComponent>()->setX(getParamFloat(spec.props, "x"));

            if (hasParam(spec.props, "y"))
                container.get<PositionComponent>()->setY(getParamFloat(spec.props, "y"));

            if (hasParam(spec.props, "z"))
                container.get<PositionComponent>()->setZ(getParamFloat(spec.props, "z"));

            prefab->setMainEntity(container.entity, layoutEnt);

            NameMap nameToEntity;
            nameToEntity[ReservedParent] = container.entity;
            nameToEntity[ReservedMain] = layoutEnt;

            if (not spec.name.empty())
            {
                prefab->addToPrefab(layoutEnt, spec.name);
                nameToEntity[spec.name] = layoutEnt;
            }

            adoptLiftedNames(prefab, nameToEntity, names);

            applyAnchorsToEntity(container.entity, spec.anchors, nameToEntity);

            return container.entity;
        }
    }

    EntityRef buildTree(EntitySystem* ecs, const NodeSpec& spec)
    {
        if (not startsWith(spec.kind, LayoutPrefix))
            return buildNodeImpl(ecs, spec, ElementMap{}, true, nullptr);

        NameMap names;
        EntityRef layoutEnt = buildLayoutNode(ecs, spec, ElementMap{}, names);

        if (layoutEnt.id == 0)
            return EntityRef{};

        return wrapLayoutRoot(ecs, spec, layoutEnt, names);
    }
}
