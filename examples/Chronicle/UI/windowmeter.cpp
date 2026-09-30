#include "windowmeter.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "logger.h"

#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/themesystem.h"

#include "Core/textmetrics.h"
#include "gloss.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.WindowMeter";

        constexpr float HeadHeight = 24.0f;     // The tab line box
        constexpr float Gap = 4.0f;             // space-1 between the rows
        constexpr float RuleHeight = 6.0f;      // A small ProgressRule
        constexpr float RuleTop = HeadHeight + Gap;
        constexpr float NoteTop = RuleTop + RuleHeight + Gap;   // 38
        constexpr float MarkGap = 8.0f;         // space-2, mark to name
        constexpr float RangeGap = 8.0f;        // space-2, name to range

        const std::string EnDash = "\xE2\x80\x93";   // U+2013

        std::string ageText(float age)
        {
            const float whole = std::round(age);

            if (std::abs(age - whole) < 0.001f)
                return std::to_string(static_cast<int>(whole));

            const int tenths = static_cast<int>(std::round(age * 10.0f));

            return std::to_string(tenths / 10) + "." + std::to_string(std::abs(tenths % 10));
        }

        std::string suffix(WindowState state)
        {
            switch (state)
            {
            case WindowState::Upcoming:
                return ".upcoming";

            case WindowState::Closed:
                return ".closed";

            case WindowState::Open:
            default:
                return "";
            }
        }

        std::string markName(WindowState state)
        {
            return state == WindowState::Closed ? "cross" : "gate";
        }

        void setElement(EntityRef entity, const std::string& element)
        {
            auto theme = entity->get<ThemeComponent>();

            if (theme->element != element)
                theme->setElement(element);
        }

        // A label painted by a window element instead of its label.<style>.<color> key. The
        // style must be the element's font: the label measures itself with it.
        Label makeWindowText(EntitySystem* ecs, const std::string& style, const std::string& element, const std::string& text, int z, Overflow overflow = Overflow::Grow, float width = 0.0f)
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

        void placeIn(EntityRef entity, _unique_id rootId, float x, float y, float dz)
        {
            auto anchor = entity->get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setLeftMargin(x);
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(y);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, dz});
        }
    }

    std::string windowRangeText(float from, float to)
    {
        return ageText(from) + EnDash + ageText(to);
    }

    WindowMeter makeWindowMeter(EntitySystem* ecs, const WindowMeterSpec& specIn)
    {
        WindowMeterSpec spec = specIn;

        if (spec.to <= spec.from)
        {
            LOG_ERROR(DOM, "Window '" << spec.name << "': to (" << spec.to << ") must be above from (" << spec.from << "); using from + 1");
            spec.to = spec.from + 1.0f;
        }

        const float W = spec.width;
        const int z = spec.z;

        WindowMeter meter;
        meter.spec = spec;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        meter.root = root.entity;

        const _unique_id rootId = root.id;
        auto prefab = root.get<Prefab>();
        const std::string state = suffix(spec.state);

        // Head, left: the mark centred on the tab line, the name 8 px after it
        meter.mark = makeMark(ecs, {markName(spec.state), MarkSize::S16, "ink", z + 1});
        placeIn(meter.mark.entity, rootId, 0.0f, (HeadHeight - px(MarkSize::S16)) / 2.0f, 1.0f);
        meter.mark.entity->get<ThemeComponent>()->setElement("window.mark" + state);
        prefab->addToPrefab(meter.mark.entity);

        // Head, right: the range on the name's baseline, its right edge on the root's
        meter.range = makeWindowText(ecs, "control", "window.range", windowRangeText(spec.from, spec.to), z + 2);
        {
            auto anchor = meter.range.entity->get<UiAnchor>();

            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(baselineShift(ecs, "tab", "control"));
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }
        prefab->addToPrefab(meter.range.entity);

        meter.name = makeWindowText(ecs, "tab", "window.name" + state, spec.name, z + 2, Overflow::Ellipsis, 1.0f);
        placeIn(meter.name.entity, rootId, px(MarkSize::S16) + MarkGap, 0.0f, 2.0f);
        prefab->addToPrefab(meter.name.entity);
        meter.fitName(ecs);

        // The small rule, no nib: how much of the window is spent
        ProgressRuleSpec rs;
        rs.width = W;
        rs.small = true;
        rs.nib = false;
        rs.percent = meter.percent();
        rs.z = z + 1;

        meter.rule = makeProgressRule(ecs, rs);
        placeIn(meter.rule.root, rootId, 0.0f, RuleTop, 1.0f);
        prefab->addToPrefab(meter.rule.root);

        // The note, the line that does the work
        meter.note = makeWindowText(ecs, "tick", "window.note" + state, spec.note, z + 2, Overflow::Wrap, W);
        placeIn(meter.note.entity, rootId, 0.0f, NoteTop, 2.0f);
        prefab->addToPrefab(meter.note.entity);

        meter.resize(ecs);

        if (not spec.glossKey.empty())
            attachGloss(ecs, meter.root, spec.glossKey);

        return meter;
    }

    float WindowMeter::percent() const
    {
        return 100.0f * std::clamp((spec.age - spec.from) / (spec.to - spec.from), 0.0f, 1.0f);
    }

    void WindowMeter::fitName(EntitySystem* ecs)
    {
        const float rangeWidth = range.entity->get<PositionComponent>()->width;
        const float room = spec.width - rangeWidth - RangeGap - (px(MarkSize::S16) + MarkGap);

        name.setWidth(ecs, std::max(1.0f, room));
    }

    void WindowMeter::paint()
    {
        const std::string state = suffix(spec.state);

        setElement(mark.entity, "window.mark" + state);
        setElement(name.entity, "window.name" + state);
        setElement(note.entity, "window.note" + state);
    }

    void WindowMeter::resize(EntitySystem* ecs)
    {
        // An empty note keeps its line, so the meter's rhythm does not change with its words.
        const float noteHeight = std::max(note.entity->get<PositionComponent>()->height, static_cast<float>(note.lineHeightPx));
        const float height = NoteTop + noteHeight;
        auto pos = root.get<PositionComponent>();

        if (std::abs(pos->height - height) > 0.01f)
        {
            pos->setHeight(height);

            // A layout the meter sits in re-stacks on the height change.
            ecs->sendEvent(PrefabChangedEvent{root.id});
        }
    }

    void WindowMeter::setAge(EntitySystem* ecs, float age, bool animate)
    {
        spec.age = age;
        rule.setPercent(ecs, percent(), animate);
    }

    void WindowMeter::setState(EntitySystem* ecs, WindowState state)
    {
        spec.state = state;

        const std::string wanted = markName(state);

        if (mark.spec.name != wanted)
            mark.setName(ecs, wanted);

        paint();
    }

    void WindowMeter::setNote(EntitySystem* ecs, const std::string& text)
    {
        spec.note = text;
        note.setText(ecs, text);
        resize(ecs);
    }

    void WindowMeter::setRange(EntitySystem* ecs, float from, float to)
    {
        if (to <= from)
        {
            LOG_ERROR(DOM, "Window '" << spec.name << "': setRange(" << from << ", " << to << ") needs to above from; ignored");
            return;
        }

        spec.from = from;
        spec.to = to;

        range.setText(ecs, windowRangeText(from, to));
        fitName(ecs);

        // A new range is a restatement, not a motion.
        rule.setPercent(ecs, percent(), false);
    }

    float WindowMeter::height(EntitySystem*) const
    {
        return root.get<PositionComponent>()->height;
    }
}
