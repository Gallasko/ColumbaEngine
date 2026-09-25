#include "mark.h"

#include <unordered_set>

#include "logger.h"

#include "2D/position.h"
#include "UI/iconsystem.h"
#include "UI/prefab.h"

#include "paint.h"
#include "label.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Mark";

        // The 27 marks, grouped as the design system lists them. This vector is the order
        // the gallery draws and the only place the set is enumerated.
        const std::vector<std::string> NAMES = {
            // Parts
            "strength", "dexterity", "intelligence", "vitality", "swordsmanship", "magic",
            // Resources
            "gold", "equipment", "relic",
            // Activities
            "adventure", "work", "study", "training", "forge", "quill", "trade",
            // Places
            "town", "gate", "academy", "guild",
            // Meta
            "time", "age", "reputation", "seal", "skull",
            // Status
            "check", "cross",
        };

        const std::unordered_set<std::string> NAME_SET(NAMES.begin(), NAMES.end());

        // ECS instances that have already registered the set; keeps a repeat call a no-op.
        std::unordered_set<EntitySystem*>& registeredIn()
        {
            static std::unordered_set<EntitySystem*> instances;
            return instances;
        }

        // A missing mark must be visible, never blank: draw "seal" and log the name once.
        std::string validateMarkName(const std::string& name)
        {
            if (isMarkName(name))
                return name;

            static std::unordered_set<std::string> warned;

            if (warned.insert(name).second)
                LOG_ERROR(DOM, "Unknown mark '" << name << "'; drawing 'seal'");

            return "seal";
        }
    }

    MarkSize markSizeFor(const std::string& style)
    {
        if (style == "caption" or style == "tick" or style == "body-sm"
            or style == "gloss" or style == "label" or style == "figure-sm")
            return MarkSize::S14;

        if (style == "body" or style == "control")
            return MarkSize::S16;

        if (style == "figure" or style == "heading" or style == "tab" or style == "gloss-title")
            return MarkSize::S18;

        if (style == "title" or style == "figure-xl" or style == "chapter")
            return MarkSize::S24;

        if (style == "versal")
            return MarkSize::S48;

        static std::unordered_set<std::string> warned;
        if (warned.insert(style).second)
            LOG_WARNING(DOM, "No mark size for style '" << style << "'; using S16");

        return MarkSize::S16;
    }

    const std::vector<std::string>& markNames() { return NAMES; }

    bool isMarkName(const std::string& name) { return NAME_SET.count(name) > 0; }

    bool registerMarks(EntitySystem* ecs, const std::string& iconRoot)
    {
        auto* icons = ecs->getSystem<IconSystem>();
        if (not icons)
        {
            LOG_ERROR(DOM, "registerMarks: no IconSystem in this ECS");
            return false;
        }

        // Idempotent: a second call must not re-rasterise the 135 glyphs.
        if (not registeredIn().insert(ecs).second)
            return true;

        std::vector<std::string> paths;
        paths.reserve(NAMES.size());
        for (const auto& name : NAMES)
            paths.push_back(iconRoot + "/" + name + ".svg");

        icons->registerIconSet("chronicle", paths, {14, 16, 18, 24, 48});
        return true;
    }

    Mark makeMark(EntitySystem* ecs, const Tokens& tokens, const MarkSpec& specIn)
    {
        MarkSpec spec = specIn;
        spec.name = validateMarkName(spec.name);

        auto icon = makeIcon(ecs, "chronicle", spec.name, px(spec.size), tokens.colour(spec.colour));
        icon.get<PositionComponent>()->setZ(static_cast<float>(spec.z));

        ecs->attach<PaintComponent>(icon.entity, spec.colour);

        Mark mark;
        mark.entity = icon.entity;
        mark.spec = spec;
        return mark;
    }

    void Mark::setName(EntitySystem*, const std::string& name)
    {
        spec.name = validateMarkName(name);
        entity->get<IconComponent>()->setIconName(spec.name);
    }

    void Mark::setColour(EntitySystem*, const std::string& token)
    {
        entity->get<PaintComponent>()->setToken(token);
        spec.colour = token;
    }

    void Mark::setSize(EntitySystem*, MarkSize size)
    {
        spec.size = size;
        auto pos = entity->get<PositionComponent>();
        pos->setWidth(px(size));
        pos->setHeight(px(size));
    }

    MarkedLabel makeMarkedLabel(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const MarkedLabelSpec& specIn)
    {
        MarkedLabelSpec spec = specIn;
        const float gap = spec.gap >= 0.0f ? spec.gap : tokens.space(2);

        const bool hasMark = not spec.mark.empty();
        const bool reserve = hasMark or spec.reserveMark;
        const bool versal = spec.label.style == "versal";
        const MarkSize markSize = versal ? MarkSize::S48 : markSizeFor(spec.label.style);
        const float markPx = px(markSize);
        const float labelLeft = reserve ? markPx + gap : 0.0f;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(spec.z));

        // Label glyphs one layer above the root; shifted right by the mark column.
        LabelSpec labelSpec = spec.label;
        labelSpec.z = spec.z + 1;
        Label label = makeLabel(ecs, tokens, styles, labelSpec);

        auto boxAnchor = label.entity->get<UiAnchor>();
        boxAnchor->setTopAnchor(PosAnchor{root.id, AnchorType::Top});
        boxAnchor->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});

        if (labelLeft > 0.0f)
            boxAnchor->setLeftMargin(labelLeft);

        root.get<Prefab>()->addToPrefab(label.entity);

        std::optional<Mark> mark;
        if (hasMark)
        {
            Mark m = makeMark(ecs, tokens, {spec.mark, markSize, spec.label.colour, spec.z + 1});
            auto markAnchor = m.entity->get<UiAnchor>();
            markAnchor->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
            markAnchor->setVerticalCenter(PosAnchor{root.id, AnchorType::VerticalCenter});
            root.get<Prefab>()->addToPrefab(m.entity);
            mark = m;
        }

        const float labelW = label.entity->get<PositionComponent>()->width;
        const float labelH = label.entity->get<PositionComponent>()->height;

        auto rootPos = root.get<PositionComponent>();
        rootPos->setWidth(labelLeft + labelW);
        rootPos->setHeight(versal ? px(MarkSize::S48) : labelH);   // only a versal mark drives the height

        MarkedLabel ml;
        ml.root = root.entity;
        ml.mark = mark;
        ml.label = label;
        return ml;
    }

    void MarkedLabel::setColour(EntitySystem* ecs, const std::string& token)
    {
        label.setColour(ecs, token);
        if (mark)
            mark->setColour(ecs, token);
    }

    void MarkedLabel::setText(EntitySystem* ecs, const TextStyles&, const std::string& newText)
    {
        label.setText(ecs, newText);

        // A Grow label re-measures its box; the mark column is the box's left margin, so the
        // root stays exactly that column plus the (possibly new) box width.
        const float labelLeft = label.entity->get<UiAnchor>()->leftMargin;
        root->get<PositionComponent>()->setWidth(labelLeft + label.entity->get<PositionComponent>()->width);
    }

    void MarkedLabel::setMark(EntitySystem* ecs, const Tokens& tokens, const std::string& name)
    {
        if (name.empty())
        {
            if (mark)
            {
                ecs->removeEntity(mark->entity.id);
                mark.reset();
            }
            return;
        }

        if (mark)
        {
            mark->setName(ecs, name);
            return;
        }

        // Adding a mark where none existed: size from the label's style, shift the box, grow the root.
        const MarkSize markSize = label.spec.style == "versal" ? MarkSize::S48 : markSizeFor(label.spec.style);
        const float gap = tokens.space(2);
        const float labelLeft = px(markSize) + gap;

        auto boxAnchor = label.entity->get<UiAnchor>();
        boxAnchor->setLeftMargin(labelLeft);

        Mark m = makeMark(ecs, tokens, {name, markSize, label.spec.colour, label.spec.z + 1});
        auto markAnchor = m.entity->get<UiAnchor>();
        markAnchor->setLeftAnchor(PosAnchor{root.id, AnchorType::Left});
        markAnchor->setVerticalCenter(PosAnchor{root.id, AnchorType::VerticalCenter});
        root->get<Prefab>()->addToPrefab(m.entity);
        mark = m;

        root->get<PositionComponent>()->setWidth(labelLeft + label.entity->get<PositionComponent>()->width);
    }
}
