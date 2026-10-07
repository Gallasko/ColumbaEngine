#include "lifeclock.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "logger.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/themesystem.h"
#include "Systems/tween.h"

#include "Core/motion.h"
#include "Core/textmetrics.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.LifeClock";

        constexpr float HeadHeight = 34.0f;     // The figure-xl line box
        constexpr float TrackGap = 8.0f;        // space-2
        constexpr float TrackHeight = 14.0f;
        constexpr float ScaleHeight = 20.0f;
        constexpr float TrackTop = HeadHeight + TrackGap;
        constexpr float TrackBottom = TrackTop + TrackHeight;
        constexpr float InnerHeight = TrackHeight - 2.0f;   // Inside the 1 px frame
        constexpr float UnitGap = 8.0f;         // Between the age and YEARS
        constexpr float NextGap = 8.0f;         // space-2, between the right-hand parts
        constexpr float EdgeWidth = 2.0f;       // Window edges
        constexpr float TickHeight = 5.0f;
        constexpr float TickLabelTop = TrackBottom + 6.0f;
        constexpr float MonthsPerYear = 12.0f;

        void place(EntityRef entity, _unique_id rootId, float x, float y, float dz)
        {
            auto anchor = entity->get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setLeftMargin(x);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(y);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, dz});
        }

        // A themed rectangle, placed in the root and owned by its prefab.
        EntityRef makeRect(EntitySystem* ecs, EntityRef root, float x, float y, float width, float height, float dz, const std::string& element)
        {
            auto rect = makeUiSimple2DShape(ecs, Shape2D::Square, width, height);

            place(rect.entity, root.id, x, y, dz);
            ecs->attach<ThemeComponent>(rect.entity, element);
            root->get<Prefab>()->addToPrefab(rect.entity);

            return rect.entity;
        }

        // A label painted by a clock element instead of its label.<style>.<color> key. The
        // style must be the element's font: the label measures itself with it.
        Label makeClockText(EntitySystem* ecs, EntityRef root, const std::string& style, const std::string& element, const std::string& text, int z)
        {
            LabelSpec ls;
            ls.style = style;
            ls.text = text;
            ls.z = z;

            Label label = makeLabel(ecs, ls);
            label.entity->get<ThemeComponent>()->setElement(element);
            root->get<Prefab>()->addToPrefab(label.entity);

            return label;
        }

        void setElement(EntityRef entity, const std::string& element)
        {
            auto theme = entity->get<ThemeComponent>();

            if (theme->element != element)
                theme->setElement(element);
        }

        void setVisible(EntityRef entity, bool visible)
        {
            auto pos = entity->get<PositionComponent>();

            if (pos->isVisible() != visible)
                pos->setVisibility(visible);
        }

        void removeChild(EntitySystem* ecs, EntityRef root, EntityRef child)
        {
            root->get<Prefab>()->childrenIds.erase(child.id);
            ecs->removeEntity(child.id);
        }

        std::string ageText(float age)
        {
            return std::to_string(static_cast<int>(std::floor(age)));
        }

        // A milestone age as written on the scale: whole ages bare, others to one decimal.
        std::string tickText(float age)
        {
            const float whole = std::round(age);

            if (std::abs(age - whole) < 0.001f)
                return std::to_string(static_cast<int>(whole));

            const int tenths = static_cast<int>(std::round(age * 10.0f));

            return std::to_string(tenths / 10) + "." + std::to_string(std::abs(tenths % 10));
        }

        std::string monthsText(int months)
        {
            return std::to_string(months) + " mo";
        }

        std::string edgeElement(bool closed)
        {
            return closed ? "clock.window.edge.closed" : "clock.window.edge";
        }
    }

    LifeClock makeLifeClock(EntitySystem* ecs, const LifeClockSpec& specIn)
    {
        LifeClockSpec spec = specIn;

        if (spec.endAge <= spec.startAge)
        {
            LOG_ERROR(DOM, "endAge (" << spec.endAge << ") must be above startAge (" << spec.startAge << "); using startAge + 1");
            spec.endAge = spec.startAge + 1.0f;
        }

        spec.age = std::clamp(spec.age, spec.startAge, spec.endAge);
        spec.runningMonths = std::max(0.0f, spec.runningMonths);

        const float W = spec.width;
        const int z = spec.z;

        LifeClock clock;
        clock.spec = spec;
        clock.shownAge = spec.age;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        root.get<PositionComponent>()->setHeight(TrackBottom + ScaleHeight);
        clock.root = root.entity;

        const _unique_id rootId = root.id;

        // Head, left: the age and YEARS on one baseline
        clock.age = makeClockText(ecs, clock.root, "figure-xl", "clock.age", ageText(spec.age), z + 2);
        place(clock.age.entity, rootId, 0.0f, 0.0f, 2.0f);

        clock.unit = makeClockText(ecs, clock.root, "label", "clock.unit", spec.unit, z + 2);
        {
            auto anchor = clock.unit.entity->get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{clock.age.entity.id, AnchorType::Right});
            anchor->setLeftMargin(UnitGap);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(baselineShift(ecs, "figure-xl", "label"));
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }

        // Head, right: mark, label and months, laid right to left, centred on the age's line box
        clock.nextIn = makeClockText(ecs, clock.root, "body-sm", "clock.next.in", monthsText(std::max(0, spec.nextIn)), z + 2);
        {
            auto anchor = clock.nextIn.entity->get<UiAnchor>();

            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin((HeadHeight - static_cast<float>(clock.nextIn.lineHeightPx)) / 2.0f);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }

        clock.next = makeClockText(ecs, clock.root, "body-sm", "clock.next", spec.nextLabel, z + 2);
        {
            auto anchor = clock.next.entity->get<UiAnchor>();

            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin((HeadHeight - static_cast<float>(clock.next.lineHeightPx)) / 2.0f);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }

        clock.nextMark = makeMark(ecs, {"time", MarkSize::S14, "status-time", z + 1});
        clock.nextMark.entity->get<ThemeComponent>()->setElement("clock.next.mark");
        {
            auto anchor = clock.nextMark.entity->get<UiAnchor>();

            anchor->setRightAnchor(PosAnchor{clock.next.entity.id, AnchorType::Left});
            anchor->setRightMargin(NextGap);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin((HeadHeight - px(MarkSize::S14)) / 2.0f);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
        }
        root.get<Prefab>()->addToPrefab(clock.nextMark.entity);

        clock.placeNext();

        // Track and frame; the frame sits above everything drawn on the track
        clock.track = makeRect(ecs, clock.root, 0.0f, TrackTop, W, TrackHeight, 1.0f, "clock.track");

        auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f}, 1.0f);
        frame.get<UiAnchor>()->fillIn(clock.track->get<UiAnchor>());
        frame.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 6.0f});
        ecs->attach<ThemeComponent>(frame.entity, "clock.frame");
        root.get<Prefab>()->addToPrefab(frame.entity);
        clock.frame = frame.entity;

        // Lived, then the running hatch and its hairline; layoutAt sizes them
        clock.lived = makeRect(ecs, clock.root, 1.0f, TrackTop + 1.0f, 0.0f, InnerHeight, 2.0f, "clock.lived");

        auto running = makeHatchRect2DShape(ecs, 0.0f, InnerHeight, {255.0f, 255.0f, 255.0f, 255.0f}, 6.0f, 2.0f);
        running.get<HatchRect2DObject>()->setAngle(45.0f);
        place(running.entity, rootId, 1.0f, TrackTop + 1.0f, 4.0f);
        ecs->attach<ThemeComponent>(running.entity, "clock.running");
        root.get<Prefab>()->addToPrefab(running.entity);
        clock.running = running.entity;

        clock.runningEdge = makeRect(ecs, clock.root, 1.0f, TrackTop + 1.0f, 1.0f, InnerHeight, 5.0f, "clock.running.edge");

        clock.buildWindows(ecs, spec.windows);
        clock.buildTicks(ecs, spec.milestones);

        clock.layoutAt(clock.shownAge);

        return clock;
    }

    float LifeClock::xFor(float a) const
    {
        const float span = spec.endAge - spec.startAge;
        const float inner = spec.width - 2.0f;
        const float clamped = std::clamp(a, spec.startAge, spec.endAge);

        return 1.0f + inner * (clamped - spec.startAge) / span;
    }

    void LifeClock::layoutAt(float shown)
    {
        const float head = xFor(shown);

        lived->get<PositionComponent>()->setWidth(head - 1.0f);

        const float end = xFor(std::min(shown + spec.runningMonths / MonthsPerYear, spec.endAge));
        const float runningWidth = end - head;
        const bool hasRunning = spec.runningMonths > 0.0f and runningWidth > 0.0f;

        running->get<UiAnchor>()->setLeftMargin(head);
        running->get<PositionComponent>()->setWidth(hasRunning ? runningWidth : 0.0f);
        runningEdge->get<UiAnchor>()->setLeftMargin(head);

        setVisible(running, hasRunning);
        setVisible(runningEdge, hasRunning);

        for (auto& tick : ticks)
        {
            setElement(tick.label.entity, tick.age <= shown ? "clock.tick.label.past" : "clock.tick.label");
        }
    }

    void LifeClock::placeNext()
    {
        const bool hasLabel = not spec.nextLabel.empty();
        const bool hasFigure = hasLabel and spec.nextIn >= 0;

        setVisible(nextMark.entity, hasLabel);
        setVisible(next.entity, hasLabel);
        setVisible(nextIn.entity, hasFigure);

        auto anchor = next.entity->get<UiAnchor>();

        if (hasFigure)
        {
            anchor->setRightAnchor(PosAnchor{nextIn.entity.id, AnchorType::Left});
            anchor->setRightMargin(NextGap);
        }
        else
        {
            anchor->setRightAnchor(PosAnchor{root.id, AnchorType::Right});
            anchor->setRightMargin(0.0f);
        }
    }

    void LifeClock::buildWindows(EntitySystem* ecs, const std::vector<ClockWindow>& list)
    {
        spec.windows.clear();

        for (const auto& window : list)
        {
            if (window.to <= window.from or window.from >= spec.endAge or window.to <= spec.startAge)
            {
                LOG_ERROR(DOM, "Window '" << window.label << "' [" << window.from << ", " << window.to << "] is outside the span [" << spec.startAge << ", " << spec.endAge << "]; dropped");
                continue;
            }

            const float left = xFor(window.from);
            const float right = xFor(std::min(window.to, spec.endAge));
            const std::string edge = edgeElement(window.closed);

            Band band;
            band.spec = window;
            band.wash = makeRect(ecs, root, left, TrackTop + 1.0f, right - left, InnerHeight, 3.0f, "clock.window.wash");
            band.left = makeRect(ecs, root, left, TrackTop + 1.0f, EdgeWidth, InnerHeight, 5.0f, edge);
            band.right = makeRect(ecs, root, right - EdgeWidth, TrackTop + 1.0f, EdgeWidth, InnerHeight, 5.0f, edge);

            windows.push_back(band);
            spec.windows.push_back(window);
        }
    }

    void LifeClock::buildTicks(EntitySystem* ecs, const std::vector<ClockMilestone>& list)
    {
        spec.milestones.clear();

        const float W = spec.width;

        for (const auto& milestone : list)
        {
            if (milestone.age < spec.startAge or milestone.age > spec.endAge)
            {
                LOG_ERROR(DOM, "Milestone at " << milestone.age << " is outside the span [" << spec.startAge << ", " << spec.endAge << "]; dropped");
                continue;
            }

            const float x = xFor(milestone.age);

            Tick tick;
            tick.age = milestone.age;
            tick.mark = makeRect(ecs, root, x - 0.5f, TrackBottom + 1.0f, 1.0f, TickHeight, 1.0f, "clock.tick.mark");

            const std::string element = milestone.age <= shownAge ? "clock.tick.label.past" : "clock.tick.label";
            tick.label = makeClockText(ecs, root, "caption", element, tickText(milestone.age), spec.z + 2);

            // Centred under the tick, nudged inward at the ends so no label overhangs the root.
            const float labelWidth = tick.label.entity->get<PositionComponent>()->width;
            const float labelX = std::clamp(x - labelWidth / 2.0f, 0.0f, std::max(0.0f, W - labelWidth));
            place(tick.label.entity, root.id, labelX, TickLabelTop, 2.0f);

            ticks.push_back(tick);
            spec.milestones.push_back(milestone);
        }
    }

    void LifeClock::setUnit(EntitySystem* ecs, const std::string& text)
    {
        spec.unit = text;
        unit.setText(ecs, text);
    }

    void LifeClock::setAge(EntitySystem* ecs, float a, bool animate)
    {
        // The figure is the fact and changes at once, past the track's end too: a life may outlast
        // its scale. The fill catches up, and stops at the end.
        age.setText(ecs, ageText(std::max(a, spec.startAge)));

        a = std::clamp(a, spec.startAge, spec.endAge);
        spec.age = a;

        if (auto entity = ecs->getEntity(lived.id); entity and entity->has<TweenComponent>())
            ecs->detach<TweenComponent>(entity);

        if (not animate or Motion::reduced() or std::abs(a - shownAge) < 0.0001f)
        {
            shownAge = a;
            layoutAt(shownAge);

            return;
        }

        // A whole span fills in the time a full bar does.
        const float msPerYear = Motion::kMsPerPercent * 100.0f / (spec.endAge - spec.startAge);
        const float duration = std::abs(a - shownAge) * msPerYear;

        LifeClock* self = this;

        ecs->attach<TweenComponent>(ecs->getEntity(lived.id), TweenComponent{
            TweenValue{shownAge}, TweenValue{a}, duration,
            [self, a](const TweenValue& v) {
                self->shownAge = std::get<float>(v);

                // Land on the target exactly: the promise is checked against it.
                if (std::abs(self->shownAge - a) < 0.0001f)
                    self->shownAge = a;

                self->layoutAt(self->shownAge);
            },
            nullptr, 1, false, false, TweenLinear});
    }

    void LifeClock::setRunning(EntitySystem*, float months)
    {
        spec.runningMonths = std::max(0.0f, months);

        // A statement, as the forecast is: never animated.
        layoutAt(shownAge);
    }

    void LifeClock::setNext(EntitySystem* ecs, const std::string& label, int months)
    {
        spec.nextLabel = label;
        spec.nextIn = months;

        if (not label.empty())
            next.setText(ecs, label);

        if (months >= 0)
            nextIn.setText(ecs, monthsText(months));

        placeNext();
    }

    void LifeClock::setWindows(EntitySystem* ecs, const std::vector<ClockWindow>& list)
    {
        for (const auto& band : windows)
        {
            removeChild(ecs, root, band.wash);
            removeChild(ecs, root, band.left);
            removeChild(ecs, root, band.right);
        }

        windows.clear();

        // The list may be spec.windows itself, which buildWindows clears first.
        const std::vector<ClockWindow> copy = list;
        buildWindows(ecs, copy);
    }

    void LifeClock::setWindowClosed(EntitySystem*, size_t index, bool closed)
    {
        if (index >= windows.size())
        {
            LOG_ERROR(DOM, "setWindowClosed(" << index << ") out of range (" << windows.size() << " windows)");
            return;
        }

        Band& band = windows[index];
        band.spec.closed = closed;
        spec.windows[index].closed = closed;

        setElement(band.left, edgeElement(closed));
        setElement(band.right, edgeElement(closed));
    }

    void LifeClock::setMilestones(EntitySystem* ecs, const std::vector<ClockMilestone>& list)
    {
        for (const auto& tick : ticks)
        {
            removeChild(ecs, root, tick.mark);
            removeChild(ecs, root, tick.label.entity);
        }

        ticks.clear();

        const std::vector<ClockMilestone> copy = list;
        buildTicks(ecs, copy);

        layoutAt(shownAge);
    }
}
