#include "eventlog.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "logger.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/themesystem.h"

#include "Core/textmetrics.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Log";

        constexpr float EdgeWidth = 2.0f;
        constexpr float PadX = 12.0f;             // Inside the edge, and on the right of the lines
        constexpr float PadY = 8.0f;
        constexpr float ScrollLane = 8.0f;        // Taken from the right padding: the thumb runs there
        constexpr float ScrollWidth = 4.0f;
        constexpr float RowSpacing = 1.0f;        // Rows one pixel apart
        constexpr float RowHeight = 20.0f;        // The body-sm line
        constexpr float AgeWidth = 30.0f;         // Room for "43.12"
        constexpr float Gap = 8.0f;               // space-2
        constexpr float RubricLine = 22.0f;       // The gloss-title line
        constexpr float RubricAbove = 12.0f;      // space-3, none for the first item
        constexpr float FootnoteGap = 4.0f;       // space-1

        // Whole months lived. The small bias keeps an age summed from twelfths (16.9 + 1/12 + ...)
        // from falling a hair short of the month it names.
        int monthsOf(float age)
        {
            return static_cast<int>(std::floor(age * 12.0f + 0.001f));
        }

        float textWidth(EntitySystem* ecs, const std::string& style, const std::string& text)
        {
            const TextStyle& s = ecs->getSystem<ThemeSystem>()->style(style);

            return ecs->getSystem<TTFTextSystem>()->measureText(s.fontAlias, text, 1.0f, 0.0f, 0.0f, s.letterSpacingPx).width;
        }

        // The advance of a space in a style: "I I" less "II".
        float spaceWidth(EntitySystem* ecs, const std::string& style)
        {
            return textWidth(ecs, style, "I I") - textWidth(ecs, style, "II");
        }

        std::string trimmed(const std::string& text)
        {
            const size_t first = text.find_first_not_of(' ');

            if (first == std::string::npos)
                return "";

            return text.substr(first, text.find_last_not_of(' ') - first + 1);
        }

        // The text's style is the element's font: the label measures itself with it.
        std::string textStyle(LogKind kind)
        {
            switch (kind)
            {
            case LogKind::Note:
                return "gloss";

            case LogKind::Milestone:
                return "figure-sm";

            default:
                return "body-sm";
            }
        }

        std::string markKey(LogKind kind)
        {
            switch (kind)
            {
            case LogKind::Gain:
                return "log.mark.gain";

            case LogKind::Loss:
                return "log.mark.loss";

            case LogKind::Coin:
                return "log.mark.coin";

            case LogKind::Milestone:
                return "log.mark.milestone";

            case LogKind::Note:
            default:
                return "log.mark";
            }
        }

        std::string textKey(LogKind kind)
        {
            switch (kind)
            {
            case LogKind::Note:
                return "log.text.note";

            case LogKind::Gain:
                return "log.text.gain";

            case LogKind::Loss:
                return "log.text.loss";

            case LogKind::Milestone:
                return "log.text.milestone";

            case LogKind::Coin:
            default:
                return "log.text";
            }
        }

        std::string figureKey(LogKind kind)
        {
            switch (kind)
            {
            case LogKind::Gain:
                return "log.figure.gain";

            case LogKind::Loss:
                return "log.figure.loss";

            case LogKind::Coin:
                return "log.figure.coin";

            default:
                return "log.figure";
            }
        }

        // A label painted by a log element instead of its label.<style>.<color> key.
        Label makeLogText(EntitySystem* ecs, const std::string& style, const std::string& element, const std::string& text, int z, Overflow overflow = Overflow::Grow, float width = 0.0f)
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

        // A line is clipped to the list from the start, before the layout gets to it: the prefab
        // hands its clip down to the parts, and strips a clip the line itself does not have.
        //
        // It is born unobserved, and so are its parts: nothing is drawn until the layout has placed
        // the line and found it in view, and the prefab hands that down with the clip. Drawn at once,
        // a part would show for a frame where it was made, before its anchors had moved it.
        EntityRef makeLine(EntitySystem* ecs, float width, float height, int z, _unique_id listId)
        {
            auto line = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
            line.get<PositionComponent>()->setWidth(width);
            line.get<PositionComponent>()->setHeight(height);
            line.get<PositionComponent>()->setObservable(false);
            ecs->attach<ClippedTo>(line.entity, listId);

            return line.entity;
        }

        void addPart(EntityRef line, EntityRef part)
        {
            part->get<PositionComponent>()->setObservable(false);
            line->get<Prefab>()->addToPrefab(part);
        }

        void setRootHeight(EventLog& log)
        {
            float height = log.spec.height;

            if (log.footnote)
                height += FootnoteGap + log.footnote->entity.get<PositionComponent>()->height;

            log.root.get<PositionComponent>()->setHeight(height);
        }
    }

    std::string ordinal(int n)
    {
        const int lastTwo = std::abs(n) % 100;
        const int last = std::abs(n) % 10;

        std::string suffix = "TH";

        if (lastTwo < 11 or lastTwo > 13)
        {
            if (last == 1)
                suffix = "ST";
            else if (last == 2)
                suffix = "ND";
            else if (last == 3)
                suffix = "RD";
        }

        return std::to_string(n) + suffix;
    }

    std::string logAge(float age)
    {
        const int months = monthsOf(age);

        return std::to_string(months / 12) + "." + std::to_string(months % 12 + 1);
    }

    int logYear(float age)
    {
        return monthsOf(age) / 12;
    }

    std::string defaultGlyph(LogKind kind)
    {
        switch (kind)
        {
        case LogKind::Milestone:
            return "seal";

        case LogKind::Loss:
            return "cross";

        case LogKind::Gain:
            return "check";

        case LogKind::Coin:
            return "gold";

        case LogKind::Note:
        default:
            return "quill";
        }
    }

    EventLog makeEventLog(EntitySystem* ecs, const EventLogSpec& specIn)
    {
        const float W = specIn.width;
        const float H = specIn.height;
        const int z = specIn.z;

        EventLog log;
        log.spec = specIn;
        log.spec.entries.clear();   // append owns them; they live in `items`
        log.spec.footnote.clear();

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        root.get<PositionComponent>()->setHeight(H);
        log.root = root.entity;

        const _unique_id rootId = root.id;
        auto prefab = root.get<Prefab>();

        // The well: a vellum-worn ground and its hair edge
        auto gutter = makeUiSimple2DShape(ecs, Shape2D::Square, W, H);
        placeIn(gutter.entity, rootId, 0.0f, 0.0f, 0.0f);
        ecs->attach<ThemeComponent>(gutter.entity, "log.gutter");
        prefab->addToPrefab(gutter.entity);
        log.gutter = gutter.entity;

        auto edge = makeUiSimple2DShape(ecs, Shape2D::Square, EdgeWidth, H);
        placeIn(edge.entity, rootId, 0.0f, 0.0f, 1.0f);
        ecs->attach<ThemeComponent>(edge.entity, "log.edge");
        prefab->addToPrefab(edge.entity);
        log.edge = edge.entity;

        // The list: fixed to the well's inner rect, so it clips and scrolls instead of growing. It
        // runs 8 px into the right padding, where the thumb goes; the lines stop short of it.
        auto list = makeVerticalLayout(ecs, 0.0f, 0.0f, log.lineWidth() + ScrollLane, H - 2.0f * PadY, true);
        list.get<VerticalLayout>()->spacing = static_cast<size_t>(RowSpacing);
        list.get<VerticalLayout>()->dragToScroll = true;   // A press and a drag scroll the life, as the wheel does
        {
            auto anchor = list.get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{gutter.entity.id, AnchorType::Left});
            anchor->setLeftMargin(EdgeWidth + PadX);
            anchor->setTopAnchor(PosAnchor{gutter.entity.id, AnchorType::Top});
            anchor->setTopMargin(PadY);
            anchor->setBottomAnchor(PosAnchor{gutter.entity.id, AnchorType::Bottom});
            anchor->setBottomMargin(PadY);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
        }
        prefab->addToPrefab(list.entity, "list");
        log.list = list.entity;

        // The thumb: a faint bar on the list's right edge, sized to what is in view, hidden while
        // everything fits. The layout places it, and dragging it scrolls the list.
        auto thumb = makeUiSimple2DShape(ecs, Shape2D::Square, ScrollWidth, 1.0f);
        thumb.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 3.0f});
        thumb.get<PositionComponent>()->setVisibility(false);
        ecs->attach<ThemeComponent>(thumb.entity, "log.scroll");
        prefab->addToPrefab(thumb.entity, "scroll");
        log.scroll = thumb.entity;

        list.get<VerticalLayout>()->setVerticalScrollBar(thumb.entity);

        for (const auto& entry : specIn.entries)
            log.append(ecs, entry);

        if (not specIn.footnote.empty())
            log.setFootnote(ecs, specIn.footnote);

        return log;
    }

    float EventLog::lineWidth() const
    {
        return spec.width - EdgeWidth - 2.0f * PadX;
    }

    bool EventLog::atEnd(EntitySystem*) const
    {
        auto layout = list.get<VerticalLayout>();
        const float viewport = list.get<PositionComponent>()->height;

        return layout->yOffset >= layout->contentHeight - viewport - 1.0f;
    }

    size_t EventLog::size() const
    {
        return static_cast<size_t>(std::count_if(items.begin(), items.end(), [](const auto& item) { return std::holds_alternative<Row>(item); }));
    }

    void EventLog::append(EntitySystem* ecs, const LogEntry& entry)
    {
        const float LW = lineWidth();

        // A layout holds its children at its own z, so the lines share the list's; their parts
        // stand above it: marks and ages at z+4, texts at z+5.
        const int z = spec.z + 2;
        const _unique_id listId = list.id;

        auto layout = list.get<VerticalLayout>();

        // The engine flag used for this insertion only: the view follows the new lines when it was
        // at the end, and stays where the player is reading otherwise.
        layout->stickToEnd = atEnd(ecs);

        const int year = logYear(entry.age);

        if (year < lastYear)
            LOG_WARNING(DOM, "Entry at " << entry.age << " is older than the year " << lastYear << " it follows; entered under that year");

        if (year > lastYear)
        {
            const float above = items.empty() ? 0.0f : RubricAbove;

            Year y;
            y.year = year;
            y.text = spec.yearPrefix + ordinal(year) + " YEAR";

            y.line = makeLine(ecs, LW, above + RubricLine, z, listId);

            // Three labels on one baseline, a display-face space apart
            const float space = spaceWidth(ecs, "gloss-title");
            const std::string prefix = trimmed(spec.yearPrefix);
            float x = 0.0f;

            if (not prefix.empty())
            {
                y.rubric = makeLogText(ecs, "gloss-title", "log.year", prefix, z + 3);
                placeIn(y.rubric->entity, y.line.id, x, above, 3.0f);
                addPart(y.line, y.rubric->entity);
                x += y.rubric->entity.get<PositionComponent>()->width + space;
            }

            y.ordinal = makeLogText(ecs, "figure", "log.year.ordinal", ordinal(year), z + 3);
            placeIn(y.ordinal.entity, y.line.id, x, above + baselineShift(ecs, "gloss-title", "figure"), 3.0f);
            addPart(y.line, y.ordinal.entity);
            x += y.ordinal.entity.get<PositionComponent>()->width + space;

            y.suffix = makeLogText(ecs, "gloss-title", "log.year", "YEAR", z + 3);
            placeIn(y.suffix.entity, y.line.id, x, above, 3.0f);
            addPart(y.line, y.suffix.entity);

            layout->addEntity(y.line);
            items.push_back(y);
            lastYear = year;
        }

        Row row;
        row.entry = entry;

        const std::string style = textStyle(entry.kind);

        row.line = makeLine(ecs, LW, RowHeight, z, listId);

        const _unique_id lineId = row.line.id;

        // Age, on the text's baseline, in a 30 px column
        row.age = makeLogText(ecs, "caption", "log.age", logAge(entry.age), z + 2, Overflow::Ellipsis, AgeWidth);
        placeIn(row.age.entity, lineId, 0.0f, baselineShift(ecs, style, "caption"), 2.0f);
        addPart(row.line, row.age.entity);

        // The mark, centred on the line
        row.mark = makeMark(ecs, {entry.glyph.empty() ? defaultGlyph(entry.kind) : entry.glyph, MarkSize::S14, "ink", z + 2});
        placeIn(row.mark.entity, lineId, AgeWidth + Gap, (RowHeight - px(MarkSize::S14)) / 2.0f, 2.0f);
        row.mark.entity->get<ThemeComponent>()->setElement(markKey(entry.kind));
        addPart(row.line, row.mark.entity);

        // The figure, right-anchored; the text is elided in what it leaves
        float figureRoom = 0.0f;

        if (not entry.figure.empty())
        {
            row.figure = makeLogText(ecs, "figure-sm", figureKey(entry.kind), entry.figure, z + 3);
            {
                auto anchor = row.figure->entity->get<UiAnchor>();

                anchor->setRightAnchor(PosAnchor{lineId, AnchorType::Right});
                anchor->setTopAnchor(PosAnchor{lineId, AnchorType::Top});
                anchor->setTopMargin(baselineShift(ecs, style, "figure-sm"));
                anchor->setZConstrain(PosConstrain{lineId, AnchorType::Z, PosOpType::Add, 3.0f});
            }
            addPart(row.line, row.figure->entity);

            figureRoom = row.figure->entity.get<PositionComponent>()->width + Gap;
        }

        const float textX = AgeWidth + Gap + px(MarkSize::S14) + Gap;
        const float textWidth = std::max(1.0f, LW - textX - figureRoom);

        row.text = makeLogText(ecs, style, textKey(entry.kind), entry.text, z + 3, Overflow::Ellipsis, textWidth);
        placeIn(row.text.entity, lineId, textX, 0.0f, 3.0f);
        addPart(row.line, row.text.entity);

        layout->addEntity(row.line);
        items.push_back(row);
    }

    void EventLog::clear(EntitySystem*)
    {
        auto layout = list.get<VerticalLayout>();

        // The layout destroys the lines; their parts follow them as the lines' prefab children.
        layout->clear();
        layout->yOffset = 0.0f;
        layout->stickToEnd = false;

        items.clear();
        lastYear = -1;
    }

    void EventLog::setFootnote(EntitySystem* ecs, const std::string& text)
    {
        spec.footnote = text;

        if (text.empty())
        {
            if (footnote)
            {
                root->get<Prefab>()->childrenIds.erase(footnote->entity.id);
                ecs->removeEntity(footnote->entity.id);
                footnote.reset();
            }
        }
        else if (footnote)
        {
            footnote->setText(ecs, text);
        }
        else
        {
            footnote = makeLogText(ecs, "caption", "log.footnote", text, spec.z + 1, Overflow::Wrap, spec.width);
            placeIn(footnote->entity, root.id, 0.0f, spec.height + FootnoteGap, 1.0f);
            root->get<Prefab>()->addToPrefab(footnote->entity);
        }

        setRootHeight(*this);
    }

    void EventLog::scrollToEnd(EntitySystem* ecs)
    {
        auto layout = list.get<VerticalLayout>();
        const float viewport = list.get<PositionComponent>()->height;

        layout->yOffset = std::max(0.0f, layout->contentHeight - viewport);

        ecs->sendEvent(LayoutScrolledEvent{list.id});
    }
}
