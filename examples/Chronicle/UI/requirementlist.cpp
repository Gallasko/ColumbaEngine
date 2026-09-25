#include "requirementlist.h"

#include <string>

#include "logger.h"

#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"

#include "Core/textmetrics.h"
#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Requirement";

        constexpr float ROW_GAP = 4.0f;    // space-1
        constexpr float VALUE_GAP = 8.0f;  // space-2, between the label and the pair

        std::string rowStyle(bool dense) { return dense ? "tick" : "body-sm"; }
        std::string valueStyle(bool dense) { return dense ? "tick" : "figure-sm"; }

        // "15 / 18": U+002F with a space each side; tabular figures around it, so the
        // pairs share a right edge down a list.
        std::string pairText(int current, int needed)
        {
            return std::to_string(current) + " / " + std::to_string(needed);
        }

        std::string toneOf(const Requirement& item) { return item.isMet() ? "status-gain" : "status-loss"; }
        std::string inkOf(const Requirement& item) { return item.isMet() ? "ink-muted" : "ink"; }

        // A condition with no verdict is not met - but it is a scene bug worth one line.
        void warnNoVerdict(const Requirement& item)
        {
            if (item.current < 0 and item.met < 0)
            {
                static bool warned = false;
                if (not warned)
                {
                    LOG_WARNING(DOM, "Non-numeric requirement '" << item.label << "' has no `met`; shown unmet");
                    warned = true;
                }
            }
        }
    }

    RequirementList makeRequirementList(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const RequirementListSpec& spec)
    {
        RequirementList list;
        list.spec = spec;
        list.spec.items.clear();   // setItems owns the rows; spec.items lives in rows[i].item
        list.tokens = &tokens;
        list.styles = &styles;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(spec.z));
        root.get<PositionComponent>()->setWidth(spec.width);
        root.get<PositionComponent>()->setHeight(0.0f);
        list.root = root.entity;

        list.setItems(ecs, tokens, styles, spec.items);
        return list;
    }

    void RequirementList::setItems(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const std::vector<Requirement>& items)
    {
        auto prefab = root->get<Prefab>();

        // Remove every row: the row entities are prefabs themselves, so their own
        // children (mark, label) follow through ClearPrefabEvent.
        for (auto& row : rows)
        {
            prefab->childrenIds.erase(row.name.root.id);
            ecs->removeEntity(row.name.root.id);
            if (row.value)
            {
                prefab->childrenIds.erase(row.value->entity.id);
                ecs->removeEntity(row.value->entity.id);
            }
        }
        rows.clear();

        const std::string style = rowStyle(spec.dense);
        const std::string vStyle = valueStyle(spec.dense);
        const float lineH = static_cast<float>(styles.get(style).lineHeightPx);
        const float W = spec.width;
        const _unique_id rootId = root.id;

        for (size_t i = 0; i < items.size(); ++i)
        {
            const Requirement& item = items[i];
            warnNoVerdict(item);

            const bool numeric = item.current >= 0;
            const bool met = item.isMet();
            const std::string tone = toneOf(item);
            const float rowTop = static_cast<float>(i) * (lineH + ROW_GAP);

            Row row;
            row.item = item;

            // The value is measured first, so the label's ellipsis width leaves room for it.
            float valueW = 0.0f;
            if (numeric)
            {
                LabelSpec vs;
                vs.style = vStyle;
                vs.text = pairText(item.current, item.needed);
                vs.colour = tone;
                vs.align = Align::Right;
                vs.overflow = Overflow::Grow;
                vs.z = spec.z + 1;

                Label value = makeLabel(ecs, tokens, styles, vs);
                valueW = value.entity->get<PositionComponent>()->width;

                auto va = value.entity->get<UiAnchor>();
                va->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
                va->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
                va->setTopMargin(rowTop + baselineShift(ecs, styles, style, vStyle));
                va->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
                root->get<Prefab>()->addToPrefab(value.entity);
                row.value = value;
            }

            MarkedLabelSpec ns;
            ns.mark = met ? "check" : "cross";
            ns.label.style = style;
            ns.label.text = item.label;
            ns.label.colour = inkOf(item);
            ns.label.overflow = Overflow::Ellipsis;
            ns.label.width = numeric ? W - valueW - VALUE_GAP : W;
            ns.gap = VALUE_GAP;
            ns.z = spec.z;
            MarkedLabel name = makeMarkedLabel(ecs, tokens, styles, ns);

            // The one row in the kit where mark and label differ in colour by design:
            // the mark carries the state (tone), the label carries the words (ink).
            if (name.mark)
                name.mark->setColour(ecs, tone);

            auto na = name.root->get<UiAnchor>();
            na->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            na->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            na->setTopMargin(rowTop);
            na->setZConstrain(PosConstrain{rootId, AnchorType::Z});
            root->get<Prefab>()->addToPrefab(name.root);
            row.name = name;

            rows.push_back(row);
        }

        const size_t n = rows.size();
        root->get<PositionComponent>()->setHeight(
            n == 0 ? 0.0f : static_cast<float>(n) * lineH + static_cast<float>(n - 1) * ROW_GAP);
    }

    void RequirementList::setItem(EntitySystem* ecs, const TextStyles&, size_t index, int current, int needed)
    {
        if (index >= rows.size())
        {
            LOG_ERROR(DOM, "setItem(" << index << ") out of range (" << rows.size() << " rows)");
            return;
        }

        Row& row = rows[index];
        if (not row.value)
        {
            LOG_WARNING(DOM, "setItem on non-numeric row " << index << " ('" << row.item.label << "'); ignored");
            return;
        }

        row.item.current = current;
        row.item.needed = needed;

        row.value->setText(ecs, pairText(current, needed));

        // The pair may have widened or narrowed: re-fit the label's ellipsis width.
        const float valueW = row.value->entity->get<PositionComponent>()->width;
        row.name.label.setWidth(ecs, spec.width - valueW - VALUE_GAP);

        repaintRow(ecs, index);   // met is re-derived unless explicit
    }

    void RequirementList::setMet(EntitySystem* ecs, size_t index, bool met)
    {
        if (index >= rows.size())
        {
            LOG_ERROR(DOM, "setMet(" << index << ") out of range (" << rows.size() << " rows)");
            return;
        }

        rows[index].item.met = met ? 1 : 0;
        repaintRow(ecs, index);
    }

    void RequirementList::clearMet(EntitySystem* ecs, size_t index)
    {
        if (index >= rows.size())
        {
            LOG_ERROR(DOM, "clearMet(" << index << ") out of range (" << rows.size() << " rows)");
            return;
        }

        rows[index].item.met = -1;
        repaintRow(ecs, index);
    }

    void RequirementList::repaintRow(EntitySystem* ecs, size_t index)
    {
        Row& row = rows[index];
        const std::string tone = toneOf(row.item);

        if (row.name.mark)
        {
            row.name.mark->setName(ecs, row.item.isMet() ? "check" : "cross");
            row.name.mark->setColour(ecs, tone);
        }
        row.name.label.setColour(ecs, inkOf(row.item));
        if (row.value)
            row.value->setColour(ecs, tone);
    }

    float RequirementList::height(EntitySystem* ecs) const
    {
        return ecs->getEntity(root.id)->get<PositionComponent>()->height;
    }
}
