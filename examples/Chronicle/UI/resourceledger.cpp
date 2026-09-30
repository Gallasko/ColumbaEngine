#include "resourceledger.h"

#include <algorithm>
#include <string>

#include "logger.h"

#include "2D/position.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/themesystem.h"

#include "Core/textmetrics.h"
#include "gloss.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Ledger";

        constexpr float LineHeight = 26.0f;       // 3 + the body-sm line (20) + 3
        constexpr float LinePad = 3.0f;
        constexpr float NameX = 24.0f;            // The S16 mark, then space-2
        constexpr float Gap = 8.0f;               // space-2: name to leader, leader to figure, figure to rate
        constexpr float LeaderRaise = 3.0f;       // Above the baseline
        constexpr float MinLeader = 24.0f;        // Shorter than this is a content error
        constexpr float HeadingAbove = 12.0f;     // space-3, none for the first group
        constexpr float HeadingLine = 16.0f;      // The label line box
        constexpr float HeadingBelow = 4.0f;      // space-1

        const std::string Minus = "\xE2\x88\x92";   // U+2212

        std::string markKey(const LedgerRowSpec& spec)
        {
            if (spec.muted)
                return "ledger.mark.muted";

            switch (spec.tone)
            {
            case LedgerTone::Coin:
                return "ledger.mark.coin";

            case LedgerTone::Guild:
                return "ledger.mark.guild";

            case LedgerTone::Relic:
                return "ledger.mark.relic";

            case LedgerTone::None:
            default:
                return "ledger.mark";
            }
        }

        // State beats tone: a muted relic is muted, not gold.
        std::string figureKey(const LedgerRowSpec& spec)
        {
            if (spec.muted)
                return "ledger.figure.muted";

            return spec.tone == LedgerTone::Relic ? "ledger.figure.relic" : "ledger.figure";
        }

        std::string nameKey(const LedgerRowSpec& spec)
        {
            return spec.muted ? "ledger.name.muted" : "ledger.name";
        }

        std::string rateKey(const std::string& rate)
        {
            const bool loss = rate.rfind(Minus, 0) == 0 or rate.rfind("-", 0) == 0;

            return loss ? "ledger.rate.loss" : "ledger.rate";
        }

        void setElement(EntityRef entity, const std::string& element)
        {
            auto theme = entity->get<ThemeComponent>();

            if (theme->element != element)
                theme->setElement(element);
        }

        // A label painted by a ledger element instead of its label.<style>.<color> key. The
        // style must be the element's font: the label measures itself with it.
        Label makeLedgerText(EntitySystem* ecs, const std::string& style, const std::string& element, const std::string& text, int z)
        {
            LabelSpec ls;
            ls.style = style;
            ls.text = text;
            ls.z = z;

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

        void placeRight(EntityRef entity, _unique_id rootId, float y, float dz)
        {
            auto anchor = entity->get<UiAnchor>();

            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setTopMargin(y);
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, dz});
        }

        void removeChild(EntitySystem* ecs, EntityRef root, _unique_id child)
        {
            root->get<Prefab>()->childrenIds.erase(child);
            ecs->removeEntity(child);
        }

        float widthOf(const Label& label)
        {
            return label.entity.get<PositionComponent>()->width;
        }

        // The figure hangs off the rate when there is one, else off the line's right edge.
        void anchorFigure(ResourceLedger::Row& row)
        {
            auto anchor = row.figure.entity->get<UiAnchor>();

            if (row.rate)
            {
                anchor->setRightAnchor(PosAnchor{row.rate->entity.id, AnchorType::Left});
                anchor->setRightMargin(Gap);
            }
            else
            {
                anchor->setRightAnchor(PosAnchor{row.line.id, AnchorType::Right});
                anchor->setRightMargin(0.0f);
            }
        }

        Label makeRate(EntitySystem* ecs, EntityRef line, const std::string& rate, int z)
        {
            Label label = makeLedgerText(ecs, "tick", rateKey(rate), rate, z + 2);
            placeRight(label.entity, line.id, LinePad + baselineShift(ecs, "body-sm", "tick"), 2.0f);
            line->get<Prefab>()->addToPrefab(label.entity);

            return label;
        }

        // The leader the engine will size, measured now: a name that leaves it under 24 px is a
        // content error the scene should see, once.
        void checkLeader(ResourceLedger::Row& row, float width)
        {
            float leader = width - NameX - widthOf(row.name) - Gap - Gap - widthOf(row.figure);

            if (row.rate)
                leader -= Gap + widthOf(*row.rate);

            if (leader < MinLeader)
            {
                if (not row.shortLeader)
                    LOG_WARNING(DOM, "Row '" << row.spec.id << "': '" << row.spec.name << "' leaves a " << leader << " px leader (under " << MinLeader << ") at width " << width);

                row.shortLeader = true;
            }
            else
            {
                row.shortLeader = false;
            }
        }

        ResourceLedger::Row makeLine(EntitySystem* ecs, const LedgerRowSpec& spec, float width, int z)
        {
            ResourceLedger::Row row;
            row.spec = spec;

            auto line = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
            line.get<PositionComponent>()->setWidth(width);
            line.get<PositionComponent>()->setHeight(LineHeight);
            row.line = line.entity;

            const _unique_id lineId = line.id;
            auto prefab = line.get<Prefab>();

            // The mark, centred on the line
            row.mark = makeMark(ecs, {spec.glyph, MarkSize::S16, "ink", z + 1});
            placeIn(row.mark.entity, lineId, 0.0f, (LineHeight - px(MarkSize::S16)) / 2.0f, 1.0f);
            row.mark.entity->get<ThemeComponent>()->setElement(markKey(spec));
            prefab->addToPrefab(row.mark.entity);

            // The name hugs its text: the leader starts where it ends
            row.name = makeLedgerText(ecs, "body-sm", nameKey(spec), spec.name, z + 2);
            placeIn(row.name.entity, lineId, NameX, LinePad, 2.0f);
            prefab->addToPrefab(row.name.entity);

            // Right to left: the rate, then the figure on the name's baseline
            if (not spec.rate.empty())
                row.rate = makeRate(ecs, row.line, spec.rate, z);

            row.figure = makeLedgerText(ecs, "figure-sm", figureKey(spec), spec.value, z + 2);
            {
                auto anchor = row.figure.entity->get<UiAnchor>();

                anchor->setTopAnchor(PosAnchor{lineId, AnchorType::Top});
                anchor->setTopMargin(LinePad + baselineShift(ecs, "body-sm", "figure-sm"));
                anchor->setZConstrain(PosConstrain{lineId, AnchorType::Z, PosOpType::Add, 2.0f});
            }
            prefab->addToPrefab(row.figure.entity);
            anchorFigure(row);

            // The leader: a dotted hairline 3 px above the baseline, sized by its two anchors
            auto leader = makeDottedLine2DShape(ecs, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f});
            {
                auto anchor = leader.get<UiAnchor>();

                anchor->setLeftAnchor(PosAnchor{row.name.entity.id, AnchorType::Right});
                anchor->setLeftMargin(Gap);
                anchor->setRightAnchor(PosAnchor{row.figure.entity.id, AnchorType::Left});
                anchor->setRightMargin(Gap);
                anchor->setTopAnchor(PosAnchor{lineId, AnchorType::Top});
                anchor->setTopMargin(LinePad + ascenderOf(ecs, "body-sm") - LeaderRaise);
                anchor->setZConstrain(PosConstrain{lineId, AnchorType::Z, PosOpType::Add, 1.0f});
            }
            leader.get<PositionComponent>()->setHeight(1.0f);
            ecs->attach<ThemeComponent>(leader.entity, "ledger.leader");
            prefab->addToPrefab(leader.entity);
            row.leader = leader.entity;

            checkLeader(row, width);

            if (not spec.glossKey.empty())
                attachGloss(ecs, row.line, spec.glossKey);

            return row;
        }

        // A group with no label has no heading to read: its block is only the space above it.
        EntityRef makeHeading(EntitySystem* ecs, const std::string& text, float width, int z, bool first, Label& label)
        {
            const float above = first ? 0.0f : HeadingAbove;

            auto block = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
            block.get<PositionComponent>()->setWidth(width);
            block.get<PositionComponent>()->setHeight(text.empty() ? above : above + HeadingLine + HeadingBelow);

            label = makeLedgerText(ecs, "label", "ledger.group", text, z + 2);
            placeIn(label.entity, block.id, 0.0f, above, 2.0f);
            block.get<Prefab>()->addToPrefab(label.entity);

            return block.entity;
        }
    }

    ResourceLedger makeResourceLedger(EntitySystem* ecs, const ResourceLedgerSpec& specIn)
    {
        const float W = specIn.width;
        const int z = specIn.z;

        ResourceLedger ledger;
        ledger.spec = specIn;
        ledger.spec.groups.clear();   // addGroup / addRow own them; they live in `groups`

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        ledger.root = root.entity;

        const _unique_id rootId = root.id;

        auto body = makeVerticalLayout(ecs, 0.0f, 0.0f, W, 0.0f, false);
        body.get<VerticalLayout>()->spacing = 0;
        {
            auto anchor = body.get<UiAnchor>();

            anchor->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            anchor->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            anchor->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            anchor->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        }
        root.get<Prefab>()->addToPrefab(body.entity, "body");
        ledger.body = body.entity;

        root.get<UiAnchor>()->setHeightConstrain(PosConstrain{body.entity.id, AnchorType::Height});

        for (const auto& group : specIn.groups)
        {
            ledger.addGroup(ecs, group.id, group.label);

            for (const auto& row : group.rows)
                ledger.addRow(ecs, group.id, row);
        }

        return ledger;
    }

    ResourceLedger::Row* ResourceLedger::row(const std::string& id)
    {
        for (auto& g : groups)
        {
            for (auto& r : g.rows)
            {
                if (r.spec.id == id)
                    return &r;
            }
        }

        return nullptr;
    }

    ResourceLedger::Group* ResourceLedger::group(const std::string& id)
    {
        for (auto& g : groups)
        {
            if (g.spec.id == id)
                return &g;
        }

        return nullptr;
    }

    int ResourceLedger::layoutIndexAfter(size_t groupIndex) const
    {
        int index = 0;

        for (size_t i = 0; i <= groupIndex and i < groups.size(); ++i)
            index += 1 + static_cast<int>(groups[i].rows.size());

        return index;
    }

    void ResourceLedger::addGroup(EntitySystem* ecs, const std::string& groupId, const std::string& label)
    {
        if (group(groupId))
            return;

        Group g;
        g.spec.id = groupId;
        g.spec.label = label;
        g.heading = makeHeading(ecs, label, spec.width, spec.z, groups.empty(), g.label);

        body->get<VerticalLayout>()->addEntity(g.heading);

        groups.push_back(g);
    }

    void ResourceLedger::addRow(EntitySystem* ecs, const std::string& groupId, const LedgerRowSpec& rowSpec, const std::string& groupLabel)
    {
        if (rowSpec.id.empty())
        {
            LOG_ERROR(DOM, "A ledger row needs an id ('" << rowSpec.name << "' has none); not added");
            return;
        }

        if (row(rowSpec.id))
        {
            LOG_ERROR(DOM, "The ledger already has a row '" << rowSpec.id << "'; not added");
            return;
        }

        if (not group(groupId))
        {
            std::string label = groupLabel;

            if (label.empty())
            {
                LOG_WARNING(DOM, "Row '" << rowSpec.id << "' goes into an unknown group '" << groupId << "' with no label; headed with its id");
                label = groupId;
            }

            addGroup(ecs, groupId, label);
        }

        size_t index = 0;

        while (groups[index].spec.id != groupId)
            ++index;

        Row r = makeLine(ecs, rowSpec, spec.width, spec.z);

        // The line goes after the group's last line: at the end for the last group
        auto layout = body->get<VerticalLayout>();

        if (index + 1 == groups.size())
            layout->addEntity(r.line);
        else
            layout->insertEntity(r.line, layoutIndexAfter(index));

        groups[index].rows.push_back(r);
    }

    void ResourceLedger::removeRow(EntitySystem*, const std::string& id)
    {
        for (auto& g : groups)
        {
            auto it = std::find_if(g.rows.begin(), g.rows.end(), [&id](const Row& r) { return r.spec.id == id; });

            if (it == g.rows.end())
                continue;

            // The layout destroys the line; its parts follow it as the line's prefab children.
            body->get<VerticalLayout>()->removeEntity(it->line.id);
            g.rows.erase(it);

            return;
        }

        LOG_ERROR(DOM, "removeRow: the ledger has no row '" << id << "'");
    }

    void ResourceLedger::clear(EntitySystem*)
    {
        // The layout destroys the headings and lines; their parts follow them.
        body->get<VerticalLayout>()->clear();
        groups.clear();
    }

    void ResourceLedger::setValue(EntitySystem* ecs, const std::string& id, const std::string& value)
    {
        Row* r = row(id);

        if (not r)
        {
            LOG_ERROR(DOM, "setValue: the ledger has no row '" << id << "'");
            return;
        }

        r->spec.value = value;

        // Right-anchored: the figure widens leftward and the leader gives way.
        r->figure.setText(ecs, value);
        checkLeader(*r, spec.width);
    }

    void ResourceLedger::setRate(EntitySystem* ecs, const std::string& id, const std::string& rate)
    {
        Row* r = row(id);

        if (not r)
        {
            LOG_ERROR(DOM, "setRate: the ledger has no row '" << id << "'");
            return;
        }

        r->spec.rate = rate;

        if (rate.empty())
        {
            if (r->rate)
            {
                removeChild(ecs, r->line, r->rate->entity.id);
                r->rate.reset();
            }
        }
        else if (r->rate)
        {
            r->rate->setText(ecs, rate);
            setElement(r->rate->entity, rateKey(rate));
        }
        else
        {
            r->rate = makeRate(ecs, r->line, rate, spec.z);
        }

        anchorFigure(*r);
        checkLeader(*r, spec.width);
    }

    void ResourceLedger::setMuted(EntitySystem*, const std::string& id, bool muted)
    {
        Row* r = row(id);

        if (not r)
        {
            LOG_ERROR(DOM, "setMuted: the ledger has no row '" << id << "'");
            return;
        }

        r->spec.muted = muted;

        setElement(r->mark.entity, markKey(r->spec));
        setElement(r->name.entity, nameKey(r->spec));
        setElement(r->figure.entity, figureKey(r->spec));
    }

    float ResourceLedger::height(EntitySystem*) const
    {
        return root.get<PositionComponent>()->height;
    }
}
