#include "activityrow.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "logger.h"

#include "ECS/callable.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/focusable.h"
#include "UI/themesystem.h"

#include "Core/textmetrics.h"
#include "gloss.h"

using namespace pg;

// The row and the list also live on their root entity as plain components. They are rebuilt
// from their spec, never saved, so the archive form is empty.
namespace pg
{
    template <>
    void serialize(Archive& archive, const chronicle::ActivityRow&)
    {
        archive.startSerialization("ActivityRow");
        archive.endSerialization();
    }

    template <>
    chronicle::ActivityRow deserialize(const UnserializedObject&)
    {
        return chronicle::ActivityRow{};
    }

    template <>
    void serialize(Archive& archive, const chronicle::ActivityList&)
    {
        archive.startSerialization("ActivityList");
        archive.endSerialization();
    }

    template <>
    chronicle::ActivityList deserialize(const UnserializedObject&)
    {
        return chronicle::ActivityList{};
    }
}

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Activity";

        // No-op event so the root's hover components carry a valid callable; the system reacts
        // to HoverChangedEvent, not to these.
        struct ActivityNoOp {};

        constexpr float Pad = 12.0f;             // space-3 padding
        constexpr float MarkColumn = 26.0f;
        constexpr float Gap = 12.0f;             // Between the columns
        constexpr float TitleHeight = 24.0f;     // The tab line box
        constexpr float MiddleGap = 4.0f;        // space-1 under the title line
        constexpr float RankGap = 8.0f;
        constexpr float EdgeWidth = 3.0f;        // border-frame
        constexpr float RingInset = 2.0f;
        constexpr float CostMarkGap = 4.0f;
        constexpr float EachGap = 2.0f;
        constexpr float MaxRequirementWidth = 300.0f;
        constexpr float DoubleReleaseMs = 400.0f;
        constexpr float HeadingHeight = 24.0f;
        constexpr float HeadingAbove = 12.0f;    // space-3, none for the first group
        constexpr float HeadingBelow = 4.0f;     // space-1
        constexpr float ScrollWidth = 4.0f;      // The scroll thumb
        constexpr float ScrollStep = 34.0f;      // Half an idle row per wheel notch

        const std::string NoBreakSpace = "\xC2\xA0";
        const std::string Minus = "\xE2\x88\x92";   // U+2212

        std::string monthsText(int months)
        {
            return std::to_string(months) + " mo";
        }

        // EntityRef::has looks an entity created during the frame up in the pool and dereferences
        // the miss. The arrow falls back to the pointer the ref holds, so go through it.
        template <typename Comp>
        bool hasComp(EntityRef entity)
        {
            return not entity.empty() and entity->template has<Comp>();
        }

        // A part added to a row that is already clipped (a list that scrolls) is clipped like
        // it: the prefab only hands its clip down when it moves or resizes.
        void clipLike(EntitySystem* ecs, EntityRef root, EntityRef part)
        {
            if (not hasComp<ClippedTo>(root) or hasComp<ClippedTo>(part))
                return;

            ecs->attach<ClippedTo>(part, root->get<ClippedTo>()->clipperId);
        }

        EntityRef entityOf(EntitySystem* ecs, _unique_id id)
        {
            if (id == 0)
                return EntityRef{};

            return ecs->getEntity(id);
        }

        void setElement(EntityRef entity, const std::string& element)
        {
            if (entity.empty() or not entity->has<ThemeComponent>())
                return;

            auto theme = entity->get<ThemeComponent>();

            if (theme->element != element)
                theme->setElement(element);
        }

        void setVisible(EntityRef entity, bool visible)
        {
            if (entity.empty())
                return;

            auto pos = entity->get<PositionComponent>();

            if (pos->isVisible() != visible)
                pos->setVisible(visible);
        }

        // A label painted by an activity element instead of its label.<style>.<color> key. The
        // style must be the element's font: the label measures itself with it.
        Label makeActivityText(EntitySystem* ecs, const std::string& style, const std::string& element, const std::string& text, int z, Overflow overflow = Overflow::Grow, float width = 0.0f)
        {
            LabelSpec ls;
            ls.style = style;
            ls.text = text;
            ls.z = z;
            ls.overflow = overflow;
            ls.width = width;

            Label label = makeLabel(ecs, ls);
            label.entity->get<ThemeComponent>()->setElement(element);

            return label;
        }

        float textWidth(EntitySystem* ecs, const std::string& style, const std::string& text)
        {
            const TextStyle& s = ecs->getSystem<ThemeSystem>()->style(style);

            return ecs->getSystem<TTFTextSystem>()->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
        }

        void placeIn(EntityRef entity, _unique_id rootId, float x, float y, float dz)
        {
            auto anchor = entity->get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setLeftMargin(x);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(y);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, dz});
        }

        void removeChild(EntitySystem* ecs, EntityRef root, _unique_id child)
        {
            if (child == 0)
                return;

            root->get<Prefab>()->childrenIds.erase(child);
            ecs->removeEntity(child);
        }

        // The copy of the row that lives on its root, or the caller's when there is none.
        ActivityRow* liveRow(ActivityRow* self)
        {
            if (hasComp<ActivityRow>(self->root))
                return self->root.get<ActivityRow>().component;

            return self;
        }

        // Runs a setter on the live row and brings the caller's copy in step.
        template <typename Fn>
        void onLive(ActivityRow* self, Fn fn)
        {
            ActivityRow* live = liveRow(self);

            fn(*live);

            if (live != self)
                *self = *live;
        }

        std::string groundElement(const ActivityRowState& st)
        {
            if (st.state == ActivityState::Running)
                return "activity.row.ground.running";

            if (st.state == ActivityState::Idle and st.hovered)
                return "activity.row.ground.hover";

            return st.stripe ? "activity.row.ground.stripe" : "activity.row.ground";
        }

        std::string edgeElement(const ActivityRowState& st)
        {
            if (st.state == ActivityState::Running)
                return "activity.row.edge.running";

            return st.selected ? "activity.row.edge.selected" : "activity.row.edge";
        }

        std::string markElement(const ActivityRowState& st)
        {
            switch (st.state)
            {
            case ActivityState::Running:
                return "activity.mark.running";

            case ActivityState::Locked:
                return "activity.mark.locked";

            case ActivityState::Idle:
            default:
                return "activity.mark";
            }
        }

        // State and flags -> the element of every part. Also the rule of the last row and the ring.
        void paint(EntitySystem* ecs, EntityRef root)
        {
            if (root.empty() or not root->has<ActivityRowState>())
                return;

            const ActivityRowState& st = *root->get<ActivityRowState>().component;
            const bool locked = st.state == ActivityState::Locked;

            setElement(entityOf(ecs, st.ground), groundElement(st));
            setElement(entityOf(ecs, st.edge), edgeElement(st));
            setElement(entityOf(ecs, st.mark), markElement(st));
            setElement(entityOf(ecs, st.name), locked ? "activity.name.locked" : "activity.name");
            setElement(entityOf(ecs, st.cost), locked ? "activity.cost.locked" : "activity.cost");

            setVisible(entityOf(ecs, st.rule), not st.last);
            setVisible(entityOf(ecs, st.ring), st.keyboardFocus and not locked);
        }

        // The same paint through the row's own refs. A row built during a frame is not in the
        // entity pool yet, so a lookup by id finds nothing and the paint would be lost.
        void paintRow(const ActivityRow& row)
        {
            if (not hasComp<ActivityRowState>(row.root))
                return;

            const ActivityRowState& st = *row.root.get<ActivityRowState>().component;
            const bool locked = st.state == ActivityState::Locked;

            setElement(row.ground, groundElement(st));
            setElement(row.edge, edgeElement(st));
            setElement(row.mark.entity, markElement(st));
            setElement(row.name.entity, locked ? "activity.name.locked" : "activity.name");
            setElement(row.cost.entity, locked ? "activity.cost.locked" : "activity.cost");

            setVisible(row.rule, not st.last);
            setVisible(row.ring, st.keyboardFocus and not locked);
        }

        // The entity a layout slot holds for a child: the builder wraps a node in a Prefab whose
        // main entity is what the kind produced.
        EntityRef mainOf(EntityRef entity)
        {
            if (entity->has<Prefab>())
            {
                auto prefab = entity->get<Prefab>();

                if (prefab->namedChildrenIds.count("MainEntity") > 0)
                    return prefab->getEntity("MainEntity");
            }

            return entity;
        }

        BaseLayout* layoutOf(EntityRef entity)
        {
            if (entity->has<VerticalLayout>())
                return entity->get<VerticalLayout>().component;

            if (entity->has<HorizontalLayout>())
                return entity->get<HorizontalLayout>().component;

            return nullptr;
        }

        // Collects the refs the layouts hold, not ids: the rows of a list built during a frame
        // are not in the entity pool yet.
        void walkList(EntityRef entity, std::vector<EntityRef>& rows, std::vector<EntityRef>& headings)
        {
            if (entity.empty())
                return;

            if (entity->has<ActivityRowState>())
            {
                rows.push_back(entity);
                return;
            }

            if (entity->has<ActivityHeadingState>())
            {
                headings.push_back(entity);
                return;
            }

            if (auto layout = layoutOf(entity))
            {
                for (auto& child : layout->entities)
                    walkList(child, rows, headings);

                return;
            }

            EntityRef main = mainOf(entity);

            if (main.id != entity.id)
                walkList(main, rows, headings);
        }
    }

    void registerActivityComponents(EntitySystem* ecs)
    {
        auto registry = ecs->getComponentRegistry();

        if (not registry->hasTypeId<ActivityRow>())
            ecs->registerFlagComponent<ActivityRow>();

        if (not registry->hasTypeId<ActivityList>())
            ecs->registerFlagComponent<ActivityList>();
    }

    std::string gainsText(const std::vector<Gain>& gains)
    {
        std::string text;

        for (const auto& gain : gains)
        {
            if (not text.empty())
                text += "   ";

            text += gain.stat + NoBreakSpace + (gain.amount < 0 ? Minus : "+") + std::to_string(std::abs(gain.amount));
        }

        return text;
    }

    ActivityRow makeActivityRow(EntitySystem* ecs, const ActivityRowSpec& specIn)
    {
        ActivityRowSpec spec = specIn;
        spec.months = std::max(0, spec.months);
        spec.percent = std::clamp(spec.percent, 0.0f, 100.0f);

        if (not ecs->getSystem<ActivitySystem>())
        {
            LOG_ERROR(DOM, "makeActivityRow needs an ActivitySystem in this ECS");
            return ActivityRow{};
        }

        if (spec.id.empty())
            LOG_ERROR(DOM, "ActivityRow '" << spec.name << "' has no id; its events will carry an empty one");

        const float W = spec.width;
        const int z = spec.z;

        ActivityRow row;
        row.spec = spec;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        row.root = root.entity;

        const _unique_id rootId = root.id;
        auto prefab = root.get<Prefab>();

        auto state = ecs->attach<ActivityRowState>(root.entity);
        state->id = spec.id;
        state->state = spec.state;
        state->stripe = spec.stripe;

        ecs->attach<FocusableComponent>(root.entity);

        if (auto focusOrder = ecs->getSystem<FocusOrderSystem>())
        {
            focusOrder->add(rootId);
            focusOrder->setEnabled(rootId, spec.state != ActivityState::Locked);
        }

        // Ground, rule, edge and ring follow the root's size through their anchors
        auto ground = makeUiSimple2DShape(ecs, Shape2D::Square, W, 1.0f);
        ground.get<UiAnchor>()->fillIn(root.get<UiAnchor>());
        ground.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        ecs->attach<ThemeComponent>(ground.entity, "activity.row.ground");
        prefab->addToPrefab(ground.entity);
        row.ground = ground.entity;
        state->ground = ground.entity.id;

        auto rule = makeUiSimple2DShape(ecs, Shape2D::Square, W, 1.0f);
        {
            auto anchor = rule.get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setBottomAnchor(PosAnchor{rootId, AnchorType::Bottom});
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        rule.get<PositionComponent>()->setHeight(1.0f);
        ecs->attach<ThemeComponent>(rule.entity, "activity.row.rule");
        prefab->addToPrefab(rule.entity);
        row.rule = rule.entity;
        state->rule = rule.entity.id;

        auto edge = makeUiSimple2DShape(ecs, Shape2D::Square, EdgeWidth, 1.0f);
        {
            auto anchor = edge.get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setBottomAnchor(PosAnchor{rootId, AnchorType::Bottom});
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }
        edge.get<PositionComponent>()->setWidth(EdgeWidth);
        ecs->attach<ThemeComponent>(edge.entity, "activity.row.edge");
        prefab->addToPrefab(edge.entity);
        row.edge = edge.entity;
        state->edge = edge.entity.id;

        // The focus ring sits inside the row, so rows stacked edge to edge never overlap it
        auto ring = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f}, ecs->getSystem<ThemeSystem>()->border("border-rule"));
        {
            auto anchor = ring.get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setLeftMargin(RingInset);
            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setRightMargin(RingInset);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(RingInset);
            anchor->setBottomAnchor(PosAnchor{rootId, AnchorType::Bottom});
            anchor->setBottomMargin(RingInset);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }
        ring.get<PositionComponent>()->setVisible(false);
        ecs->attach<ThemeComponent>(ring.entity, "activity.row.ring");
        prefab->addToPrefab(ring.entity);
        row.ring = ring.entity;
        state->ring = ring.entity.id;

        // Mark: S24 (the design system draws 22; the kit registers 18 and 24), centred in its column
        row.mark = makeMark(ecs, {spec.glyph, MarkSize::S24, "ink-muted", z + 3});
        {
            auto anchor = row.mark.entity->get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setLeftMargin(Pad + (MarkColumn - px(MarkSize::S24)) / 2.0f);
            anchor->setVerticalCenter(PosAnchor{rootId, AnchorType::VerticalCenter});
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 3.0f});
        }
        row.mark.entity->get<ThemeComponent>()->setElement("activity.mark");
        prefab->addToPrefab(row.mark.entity);
        state->mark = row.mark.entity.id;

        // Right column: the cost on the title line's baseline, `each` under it
        row.cost = makeActivityText(ecs, "control", "activity.cost", monthsText(spec.months), z + 4);
        {
            auto anchor = row.cost.entity->get<UiAnchor>();

            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setRightMargin(Pad);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(Pad + baselineShift(ecs, "tab", "control"));
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 4.0f});
        }
        prefab->addToPrefab(row.cost.entity);
        state->cost = row.cost.entity.id;

        row.costMark = makeMark(ecs, {"time", MarkSize::S14, "status-time", z + 3});
        {
            auto anchor = row.costMark.entity->get<UiAnchor>();

            anchor->setRightAnchor(PosAnchor{row.cost.entity.id, AnchorType::Left});
            anchor->setRightMargin(CostMarkGap);
            anchor->setVerticalCenter(PosAnchor{row.cost.entity.id, AnchorType::VerticalCenter});
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 3.0f});
        }
        row.costMark.entity->get<ThemeComponent>()->setElement("activity.cost.mark");
        prefab->addToPrefab(row.costMark.entity);
        state->costMark = row.costMark.entity.id;

        if (not spec.each.empty())
        {
            row.each = makeActivityText(ecs, "caption", "activity.each", spec.each, z + 4);

            auto anchor = row.each->entity->get<UiAnchor>();

            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setRightMargin(Pad);
            anchor->setTopAnchor(PosAnchor{row.cost.entity.id, AnchorType::Bottom});
            anchor->setTopMargin(EachGap);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 4.0f});
            prefab->addToPrefab(row.each->entity);
        }

        // Title line: the name, then the rank on its baseline
        row.name = makeActivityText(ecs, "tab", "activity.name", spec.name, z + 4);
        placeIn(row.name.entity, rootId, Pad + MarkColumn + Gap, Pad, 4.0f);
        prefab->addToPrefab(row.name.entity);
        state->name = row.name.entity.id;

        if (not spec.rank.empty())
        {
            row.rank = makeActivityText(ecs, "caption", "activity.rank", spec.rank, z + 4);

            auto anchor = row.rank->entity->get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{row.name.entity.id, AnchorType::Right});
            anchor->setLeftMargin(RankGap);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(Pad + baselineShift(ecs, "tab", "caption"));
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 4.0f});
            prefab->addToPrefab(row.rank->entity);
        }

        row.fitName(ecs);

        row.buildMiddle(ecs);
        row.resize(ecs);

        if (not spec.glossKey.empty())
            attachGloss(ecs, row.root, spec.glossKey);

        paintRow(row);

        // The row lives on its root, where a list finds it
        registerActivityComponents(ecs);
        ecs->attachGeneric<ActivityRow>(row.root, row);

        return row;
    }

    float ActivityRow::middleWidth() const
    {
        const float costWidth = px(MarkSize::S14) + CostMarkGap + cost.entity.get<PositionComponent>()->width;
        const float eachWidth = each ? each->entity.get<PositionComponent>()->width : 0.0f;
        const float rightWidth = std::max(costWidth, eachWidth);

        return spec.width - Pad - MarkColumn - Gap - rightWidth - Gap - Pad;
    }

    void ActivityRow::buildMiddle(EntitySystem* ecs)
    {
        const float x = Pad + MarkColumn + Gap;
        const float y = Pad + TitleHeight + MiddleGap;
        const float middle = middleWidth();
        const int z = spec.z;
        auto prefab = root->get<Prefab>();

        switch (spec.state)
        {
        case ActivityState::Running:
        {
            ProgressRuleSpec ps;
            ps.width = middle;
            ps.percent = spec.percent;
            ps.nib = true;
            ps.caption = spec.caption;
            ps.z = z + 3;

            progress = makeProgressRule(ecs, ps);
            placeIn(progress->root, root.id, x, y, 3.0f);
            prefab->addToPrefab(progress->root);
            clipLike(ecs, root, progress->root);
            break;
        }

        case ActivityState::Locked:
        {
            RequirementListSpec rs;
            rs.width = std::min(MaxRequirementWidth, middle);
            rs.dense = true;
            rs.items = spec.requirements;
            rs.z = z + 3;

            reqs = makeRequirementList(ecs, rs);
            placeIn(reqs->root, root.id, x, y, 3.0f);
            prefab->addToPrefab(reqs->root);
            clipLike(ecs, root, reqs->root);
            break;
        }

        case ActivityState::Idle:
        default:
            gains = makeActivityText(ecs, "tick", "activity.gains", gainsText(spec.gains), z + 4);
            placeIn(gains->entity, root.id, x, y, 4.0f);
            prefab->addToPrefab(gains->entity);
            clipLike(ecs, root, gains->entity);
            break;
        }
    }

    void ActivityRow::clearMiddle(EntitySystem* ecs)
    {
        if (gains)
            removeChild(ecs, root, gains->entity.id);

        if (progress)
            removeChild(ecs, root, progress->root.id);

        if (reqs)
            removeChild(ecs, root, reqs->root.id);

        gains.reset();
        progress.reset();
        reqs.reset();
    }

    void ActivityRow::resize(EntitySystem* ecs)
    {
        float middle = 0.0f;

        if (progress)
            middle = progress->root.get<PositionComponent>()->height;
        else if (reqs)
            middle = reqs->height(ecs);
        else if (gains)
            middle = static_cast<float>(gains->lineHeightPx);   // An empty gains line keeps its height

        const float height = Pad + TitleHeight + MiddleGap + middle + Pad;
        auto pos = root.get<PositionComponent>();

        if (std::abs(pos->height - height) > 0.01f)
        {
            pos->setHeight(height);

            // The list re-stacks on the height change; tell anyone who follows the prefab.
            ecs->sendEvent(PrefabChangedEvent{root.id});
        }
    }

    void ActivityRow::fitName(EntitySystem* ecs)
    {
        // The name hugs its text, so the rank follows it; it is elided only when the room
        // between the mark and the cost is too small for it.
        const float rankWidth = rank ? rank->entity.get<PositionComponent>()->width + RankGap : 0.0f;
        const float room = std::max(0.0f, middleWidth() - rankWidth);
        const bool elide = textWidth(ecs, "tab", name.spec.text) > room;
        const Overflow overflow = elide ? Overflow::Ellipsis : Overflow::Grow;

        if (name.spec.overflow != overflow)
        {
            name.spec.overflow = overflow;
            name.entity->get<TTFText>()->setOverflow(overflow);
        }

        // For a name that grows this only re-measures it.
        name.setWidth(ecs, room);
    }

    void ActivityRow::setState(EntitySystem* ecs, ActivityState newState)
    {
        onLive(this, [ecs, newState](ActivityRow& r) {
            if (r.spec.state == newState)
                return;

            r.spec.state = newState;

            auto st = r.root.get<ActivityRowState>();
            st->state = newState;

            // A running or a locked row cannot stay selected.
            if (st->selected and newState != ActivityState::Idle)
            {
                if (auto system = ecs->getSystem<ActivitySystem>(); system and st->list != 0)
                    system->select(ecs->getEntity(st->list), "");
                else
                    st->selected = false;
            }

            if (auto focusOrder = ecs->getSystem<FocusOrderSystem>())
                focusOrder->setEnabled(r.root.id, newState != ActivityState::Locked);

            r.clearMiddle(ecs);
            r.buildMiddle(ecs);
            r.resize(ecs);

            paintRow(r);
        });
    }

    void ActivityRow::setPercent(EntitySystem* ecs, float percent, bool animate)
    {
        onLive(this, [ecs, percent, animate](ActivityRow& r) {
            r.spec.percent = std::clamp(percent, 0.0f, 100.0f);

            if (r.progress)
                r.progress->setPercent(ecs, r.spec.percent, animate);
        });
    }

    void ActivityRow::setCaption(EntitySystem* ecs, const std::string& text)
    {
        onLive(this, [ecs, &text](ActivityRow& r) {
            r.spec.caption = text;

            if (r.progress)
            {
                r.progress->setCaption(ecs, text);
                r.resize(ecs);
            }
        });
    }

    void ActivityRow::setGains(EntitySystem* ecs, const std::vector<Gain>& newGains)
    {
        onLive(this, [ecs, &newGains](ActivityRow& r) {
            r.spec.gains = newGains;

            if (r.gains)
                r.gains->setText(ecs, gainsText(newGains));
        });
    }

    void ActivityRow::setRequirements(EntitySystem* ecs, const std::vector<Requirement>& items)
    {
        onLive(this, [ecs, &items](ActivityRow& r) {
            r.spec.requirements = items;

            if (r.reqs)
            {
                r.reqs->setItems(ecs, items);
                r.resize(ecs);
            }
        });
    }

    void ActivityRow::setRequirement(EntitySystem* ecs, size_t index, int current, int needed)
    {
        onLive(this, [ecs, index, current, needed](ActivityRow& r) {
            if (index >= r.spec.requirements.size())
            {
                LOG_ERROR(DOM, "setRequirement(" << index << ") out of range (" << r.spec.requirements.size() << " requirements)");
                return;
            }

            r.spec.requirements[index].current = current;
            r.spec.requirements[index].needed = needed;

            if (r.reqs)
                r.reqs->setItem(ecs, index, current, needed);
        });
    }

    void ActivityRow::setName(EntitySystem* ecs, const std::string& text)
    {
        onLive(this, [ecs, &text](ActivityRow& r) {
            r.spec.name = text;
            r.name.setText(ecs, text);
            r.fitName(ecs);
        });
    }

    void ActivityRow::setGlyph(EntitySystem* ecs, const std::string& glyph)
    {
        onLive(this, [ecs, &glyph](ActivityRow& r) {
            r.spec.glyph = glyph;
            r.mark.setName(ecs, glyph);
        });
    }

    void ActivityRow::setMonths(EntitySystem* ecs, int months)
    {
        onLive(this, [ecs, months](ActivityRow& r) {
            r.spec.months = std::max(0, months);
            r.cost.setText(ecs, monthsText(r.spec.months));
            r.fitName(ecs);
        });
    }

    void ActivityRow::setEach(EntitySystem* ecs, const std::string& text)
    {
        onLive(this, [ecs, &text](ActivityRow& r) {
            if (not r.each)
            {
                LOG_WARNING(DOM, "Row '" << r.spec.id << "' was built without an `each` line; setEach ignored");
                return;
            }

            r.spec.each = text;
            r.each->setText(ecs, text);
            r.fitName(ecs);
        });
    }

    float ActivityRow::height(EntitySystem*) const
    {
        return root.get<PositionComponent>()->height;
    }

    EntityRef makeActivityHeading(EntitySystem* ecs, const std::string& text, float width, int z)
    {
        auto block = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        block.get<PositionComponent>()->setWidth(width);
        block.get<PositionComponent>()->setHeight(HeadingAbove + HeadingHeight + HeadingBelow);

        Label label = makeActivityText(ecs, "heading", "activity.group", text, z + 4);
        placeIn(label.entity, block.id, 0.0f, HeadingAbove, 4.0f);
        block.get<Prefab>()->addToPrefab(label.entity, "label");

        auto state = ecs->attach<ActivityHeadingState>(block.entity);
        state->label = label.entity.id;

        return block.entity;
    }

    ActivityList makeActivityList(EntitySystem* ecs, const ActivityListSpec& spec)
    {
        if (not ecs->getSystem<ActivitySystem>())
        {
            LOG_ERROR(DOM, "makeActivityList needs an ActivitySystem in this ECS");
            return ActivityList{};
        }

        registerActivityComponents(ecs);

        const float W = spec.width;
        const float H = std::max(0.0f, spec.height);
        const bool scrolls = H > 0.0f;

        ActivityList list;
        list.spec = spec;
        list.spec.height = H;

        // Root: a prefab of its own, so a name given to the list resolves to it
        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(spec.z));
        root.get<PositionComponent>()->setWidth(W);
        list.root = root.entity;

        const _unique_id rootId = root.id;

        auto state = ecs->attach<ActivityListState>(root.entity);
        state->id = spec.id;

        // Body: the rows stack here. With a height it keeps it, clips its rows and scrolls.
        auto body = makeVerticalLayout(ecs, 0.0f, 0.0f, W, H, scrolls);
        body.get<VerticalLayout>()->spacing = 0;
        body.get<VerticalLayout>()->scrollSpeed = ScrollStep;
        {
            auto anchor = body.get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        }
        root.get<Prefab>()->addToPrefab(body.entity, "body");
        list.body = body.entity;

        if (scrolls)
        {
            root.get<PositionComponent>()->setHeight(H);

            auto thumb = makeUiSimple2DShape(ecs, Shape2D::Square, ScrollWidth, 1.0f);
            thumb.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 6.0f});
            thumb.get<PositionComponent>()->setVisibility(false);
            ecs->attach<ThemeComponent>(thumb.entity, "activity.scroll");
            root.get<Prefab>()->addToPrefab(thumb.entity, "scroll");
            list.scroll = thumb.entity;

            // The layout places the thumb on its right edge and sizes it to what is in view
            body.get<VerticalLayout>()->setVerticalScrollBar(thumb.entity);

            // A press and a drag scroll the rows; the release of a drag selects nothing
            body.get<VerticalLayout>()->dragToScroll = true;
        }
        else
        {
            root.get<UiAnchor>()->setHeightConstrain(PosConstrain{body.entity.id, AnchorType::Height});
        }

        list.setRows(ecs, spec.groups);

        return list;
    }

    void ActivityList::select(EntitySystem* ecs, const std::string& id)
    {
        auto system = ecs->getSystem<ActivitySystem>();

        if (not system)
        {
            LOG_ERROR(DOM, "ActivityList::select needs an ActivitySystem in this ECS");
            return;
        }

        system->select(root, id);
    }

    const std::string& ActivityList::selected() const
    {
        return root.get<ActivityListState>()->selected;
    }

    ActivityRow* ActivityList::find(EntitySystem*, const std::string& id)
    {
        std::vector<EntityRef> rows;
        std::vector<EntityRef> headings;
        walkList(body, rows, headings);

        for (auto& entity : rows)
        {
            if (entity->get<ActivityRowState>()->id == id and entity->has<ActivityRow>())
                return entity->get<ActivityRow>().component;
        }

        return nullptr;
    }

    ActivityRow* ActivityList::row(EntitySystem* ecs, const std::string& id)
    {
        if (auto r = find(ecs, id))
            return r;

        LOG_ERROR(DOM, "List '" << spec.id << "' has no row '" << id << "'");

        return nullptr;
    }

    void ActivityList::setRowState(EntitySystem* ecs, const std::string& id, ActivityState newState)
    {
        if (auto r = row(ecs, id))
            r->setState(ecs, newState);
    }

    void ActivityList::setRows(EntitySystem* ecs, const std::vector<ActivityGroup>& groups)
    {
        auto layout = body.get<VerticalLayout>();

        // Drop what the list holds, rows and headings alike
        std::vector<_unique_id> old;

        for (auto& child : layout->entities)
            old.push_back(child.id);

        for (auto id : old)
        {
            layout->removeEntity(id);
            ecs->removeEntity(id);
        }

        auto state = root.get<ActivityListState>();
        state->selected.clear();
        state->rows.clear();
        state->headings.clear();

        spec.groups = groups;

        for (const auto& group : groups)
        {
            if (not group.label.empty())
                layout->addEntity(makeActivityHeading(ecs, group.label, spec.width, spec.z));

            for (auto rowSpec : group.rows)
            {
                rowSpec.width = spec.width;
                rowSpec.z = spec.z;

                layout->addEntity(makeActivityRow(ecs, rowSpec).root);
            }
        }
    }

    void ActivitySystem::init()
    {
        registerActivityComponents(ecsRef);

        auto group = registerGroup<ActivityRowState>();

        group->addOnGroup([this](EntityRef entity) {
            if (not entity->has<MouseEnterComponent>())
                ecsRef->attach<MouseEnterComponent>(entity, makeCallable<ActivityNoOp>(ActivityNoOp{}));

            if (not entity->has<MouseLeaveComponent>())
                ecsRef->attach<MouseLeaveComponent>(entity, makeCallable<ActivityNoOp>(ActivityNoOp{}));
        });

        // A removed row leaves the Tab order with it.
        group->removeOfGroup([this](EntitySystem*, _unique_id id) {
            if (auto focusOrder = ecsRef->getSystem<FocusOrderSystem>())
                focusOrder->remove(id);

            if (lastReleased == id)
                lastReleased = 0;
        });
    }

    void ActivitySystem::applyVisual(EntityRef root)
    {
        // Through the row's refs when it has them, so a row still pending in the pool paints too.
        if (not root.empty() and root->has<ActivityRow>())
            paintRow(*root->get<ActivityRow>().component);
        else
            paint(ecsRef, root);
    }

    void ActivitySystem::adopt(EntityRef listRoot)
    {
        auto list = listRoot->get<ActivityListState>();

        EntityRef body = listRoot->get<Prefab>()->getEntity("body");

        std::vector<EntityRef> rows;
        std::vector<EntityRef> headings;
        walkList(body, rows, headings);

        std::vector<_unique_id> rowIds;
        std::vector<_unique_id> headingIds;

        for (auto& row : rows)
            rowIds.push_back(row.id);

        for (auto& heading : headings)
            headingIds.push_back(heading.id);

        if (rowIds == list->rows and headingIds == list->headings)
            return;

        list->rows = rowIds;
        list->headings = headingIds;

        bool selectionFound = list->selected.empty();

        // Stripes alternate across the whole list, not per group: the second, fourth... rows.
        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& entity = rows[i];
            auto st = entity->get<ActivityRowState>();

            st->list = listRoot.id;
            st->stripe = i % 2 == 1;
            st->last = i + 1 == rows.size();

            if (entity->has<ActivityRow>())
                entity->get<ActivityRow>()->spec.stripe = st->stripe;

            if (st->selected and st->id == list->selected)
                selectionFound = true;

            applyVisual(entity);
        }

        if (not selectionFound)
            list->selected.clear();

        for (size_t i = 0; i < headings.size(); ++i)
        {
            auto& entity = headings[i];
            const float above = i == 0 ? 0.0f : HeadingAbove;

            entity->get<PositionComponent>()->setHeight(above + HeadingHeight + HeadingBelow);

            EntityRef label = entity->get<Prefab>()->getEntity("label");

            if (not label.empty())
                label->get<UiAnchor>()->setTopMargin(above);
        }
    }

    void ActivitySystem::execute()
    {
        for (auto list : view<ActivityListState>())
        {
            if (auto entity = ecsRef->getEntity(list->entityId))
                adopt(entity);
        }
    }

    bool ActivitySystem::select(EntityRef listRoot, const std::string& id)
    {
        if (listRoot.empty() or not listRoot->has<ActivityListState>())
        {
            LOG_ERROR(DOM, "select('" << id << "'): not an activity list");
            return false;
        }

        auto list = listRoot->get<ActivityListState>();

        ActivityRowState* target = nullptr;

        if (not id.empty())
        {
            for (auto st : view<ActivityRowState>())
            {
                if (st->list == listRoot.id and st->id == id)
                    target = st;
            }

            if (not target)
            {
                LOG_ERROR(DOM, "List '" << list->id << "' has no row '" << id << "'");
                return false;
            }

            if (target->state != ActivityState::Idle)
            {
                LOG_WARNING(DOM, "Row '" << id << "' is " << (target->state == ActivityState::Locked ? "locked" : "running") << "; it cannot be selected");
                return false;
            }
        }

        if (list->selected == id)
            return true;

        for (auto st : view<ActivityRowState>())
        {
            if (st->list != listRoot.id)
                continue;

            const bool selected = st == target;

            if (st->selected != selected)
            {
                st->selected = selected;
                applyVisual(ecsRef->getEntity(st->entityId));
            }
        }

        list->selected = id;
        ecsRef->sendEvent(ActivitySelectedEvent{list->id, id});

        return true;
    }

    void ActivitySystem::activate(EntityRef row)
    {
        auto st = row->get<ActivityRowState>();

        if (st->state != ActivityState::Idle)
            return;

        std::string listId;

        if (auto listEntity = ecsRef->getEntity(st->list); listEntity and listEntity->has<ActivityListState>())
            listId = listEntity->get<ActivityListState>()->id;

        ecsRef->sendEvent(ActivityActivatedEvent{listId, st->id});
    }

    void ActivitySystem::onEvent(const HoverChangedEvent& event)
    {
        // Locked rows take the hover, so nothing under them lights, but they never change.
        for (auto id : event.entered)
        {
            if (auto entity = ecsRef->getEntity(id); entity and entity->has<ActivityRowState>())
            {
                entity->get<ActivityRowState>()->hovered = true;
                applyVisual(entity);
            }
        }

        for (auto id : event.left)
        {
            if (auto entity = ecsRef->getEntity(id); entity and entity->has<ActivityRowState>())
            {
                auto st = entity->get<ActivityRowState>();
                st->hovered = false;
                st->pressed = false;   // A drag off cancels
                applyVisual(entity);
            }
        }
    }

    void ActivitySystem::onEvent(const OnMouseClick& event)
    {
        if (event.button != 1)
            return;

        for (auto st : view<ActivityRowState>())
        {
            if (st->hovered and st->state == ActivityState::Idle)
                st->pressed = true;
        }
    }

    void ActivitySystem::onEvent(const OnMouseRelease& event)
    {
        if (event.button != 1)
            return;

        _unique_id released = 0;

        for (auto st : view<ActivityRowState>())
        {
            if (not st->pressed)
                continue;

            st->pressed = false;

            if (st->hovered)
                released = st->entityId;
        }

        // The press scrolled the list: it was not a click
        if (released == 0 or event.cancelled)
            return;

        auto row = ecsRef->getEntity(released);
        auto st = row->get<ActivityRowState>();

        // A second release on the selected row confirms it; a first one selects it.
        if (st->selected and lastReleased == released and now - lastReleaseAt <= DoubleReleaseMs)
            activate(row);
        else if (not st->selected and st->list != 0)
            select(ecsRef->getEntity(st->list), st->id);

        lastReleased = released;
        lastReleaseAt = now;
    }

    void ActivitySystem::onEvent(const OnSDLScanCode& event)
    {
        if (event.key != SDL_SCANCODE_RETURN and event.key != SDL_SCANCODE_SPACE)
            return;

        auto focusOrder = ecsRef->getSystem<FocusOrderSystem>();

        // Only a row the keyboard is on (its ring showing): after a click the focus order still
        // remembers the row, and a key the scene uses (Space) must not select it behind its back.
        if (not focusOrder or not focusOrder->keyboardFocus())
            return;

        auto row = ecsRef->getEntity(focusOrder->current());

        if (not row or not row->has<ActivityRowState>())
            return;

        auto st = row->get<ActivityRowState>();

        if (st->state != ActivityState::Idle)
            return;

        // Enter on the selected row confirms it; on another row it selects that one first.
        if (st->selected)
            activate(row);
        else if (st->list != 0)
            select(ecsRef->getEntity(st->list), st->id);
    }

    void ActivitySystem::onEvent(const KeyboardFocusChangedEvent& event)
    {
        for (auto st : view<ActivityRowState>())
        {
            const bool focused = st->entityId == event.face and event.keyboard;

            if (st->keyboardFocus != focused)
            {
                st->keyboardFocus = focused;
                applyVisual(ecsRef->getEntity(st->entityId));
            }
        }
    }

    void ActivitySystem::onEvent(const TickEvent& event)
    {
        now += event.tick;
    }
}
