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
        constexpr float PadX = 12.0f;             // Inside the edge
        constexpr float PadY = 8.0f;
        constexpr float RowSpacing = 1.0f;        // Rows one pixel apart
        constexpr float RowHeight = 20.0f;        // The body-sm line
        constexpr float AgeWidth = 30.0f;         // Room for "43.9"
        constexpr float Gap = 8.0f;               // space-2
        constexpr float RubricLine = 22.0f;       // The gloss-title line
        constexpr float RubricAbove = 12.0f;      // space-3, none for the first item
        constexpr float FootnoteGap = 4.0f;       // space-1

        std::string ageText(float age)
        {
            const int tenths = static_cast<int>(std::round(age * 10.0f));

            return std::to_string(tenths / 10) + "." + std::to_string(std::abs(tenths % 10));
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
        EntityRef makeLine(EntitySystem* ecs, float width, float height, int z, _unique_id listId)
        {
            auto line = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
            line.get<PositionComponent>()->setWidth(width);
            line.get<PositionComponent>()->setHeight(height);
            ecs->attach<ClippedTo>(line.entity, listId);

            return line.entity;
        }

        void addPart(EntityRef line, EntityRef part)
        {
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

        // The list: fixed to the well's inner rect, so it clips and scrolls instead of growing
        auto list = makeVerticalLayout(ecs, 0.0f, 0.0f, log.listWidth(), H - 2.0f * PadY, true);
        list.get<VerticalLayout>()->spacing = static_cast<size_t>(RowSpacing);
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

        for (const auto& entry : specIn.entries)
            log.append(ecs, entry);

        if (not specIn.footnote.empty())
            log.setFootnote(ecs, specIn.footnote);

        return log;
    }

    float EventLog::listWidth() const
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
        const float LW = listWidth();

        // A layout holds its children at its own z, so the lines share the list's; their parts
        // stand above it: marks and ages at z+4, texts at z+5.
        const int z = spec.z + 2;
        const _unique_id listId = list.id;

        auto layout = list.get<VerticalLayout>();

        // The engine flag used for this insertion only: the view follows the new lines when it was
        // at the end, and stays where the player is reading otherwise.
        layout->stickToEnd = atEnd(ecs);

        const int year = static_cast<int>(std::floor(entry.age));

        if (year < lastYear)
            LOG_WARNING(DOM, "Entry at " << entry.age << " is older than the year " << lastYear << " it follows; entered under that year");

        if (year > lastYear)
        {
            const float above = items.empty() ? 0.0f : RubricAbove;

            Year y;
            y.year = year;

            y.line = makeLine(ecs, LW, above + RubricLine, z, listId);

            y.rubric = makeLogText(ecs, "gloss-title", "log.year", spec.yearPrefix + ordinal(year) + " YEAR", z + 3);
            placeIn(y.rubric.entity, y.line.id, 0.0f, above, 3.0f);
            addPart(y.line, y.rubric.entity);

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
        row.age = makeLogText(ecs, "caption", "log.age", ageText(entry.age), z + 2, Overflow::Ellipsis, AgeWidth);
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
