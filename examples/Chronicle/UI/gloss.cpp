#include "gloss.h"

#include <algorithm>
#include <unordered_set>

#include "logger.h"

#include "ECS/entitysystem_fwd.h"   // ResizeEvent, used by tooltip.h
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"
#include "UI/tooltip.h"

#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Gloss";

        constexpr float MARGIN_W = 240.0f;
        constexpr float TOOLTIP_W = 280.0f;
        constexpr float EDGE = 2.0f;      // border-rule
        constexpr float PAD = 12.0f;      // space-3
        constexpr float GAP1 = 4.0f;      // space-1
        constexpr float GAP2 = 8.0f;      // space-2
        constexpr float TEXT_INDENT = EDGE + PAD;   // 14

        float boxH(const Label& l) { EntityRef b = l.box; return b->get<PositionComponent>()->height; }
    }

    Gloss makeGloss(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const GlossSpec& spec)
    {
        auto* paint = ecs->getSystem<PaintSystem>();

        Gloss g;
        g.spec = spec;

        if (spec.kind == GlossKind::Margin)
        {
            if (not spec.title.empty() or not spec.rows.empty())
            {
                static bool warned = false;
                if (not warned)
                {
                    LOG_WARNING(DOM, "A Margin gloss has no title or rows; ignoring them");
                    warned = true;
                }
            }

            const float W = spec.width > 0.0f ? spec.width : MARGIN_W;
            const int z = spec.z;

            auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
            root.get<PositionComponent>()->setWidth(W);
            const _unique_id rootId = root.id;
            g.root = root.entity;

            LabelSpec ts; ts.style = "gloss"; ts.text = spec.text; ts.colour = "ink-muted";
            ts.overflow = Overflow::Wrap; ts.width = W - TEXT_INDENT; ts.z = z;
            Label text = makeLabel(ecs, tokens, styles, ts);
            auto ta = text.box->get<UiAnchor>();
            ta->setLeftAnchor(PosAnchor{rootId, AnchorType::Left}); ta->setLeftMargin(TEXT_INDENT);
            ta->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            ta->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
            root.get<Prefab>()->addToPrefab(text.box);
            g.text = text;

            root.get<PositionComponent>()->setHeight(boxH(text));

            // The 2 px rule-hair edge, spanning the whole gloss height.
            auto edge = makeUiSimple2DShape(ecs, Shape2D::Square, EDGE, 1.0f, tokens.colour("rule-hair"));
            auto ea = edge.get<UiAnchor>();
            ea->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ea->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            ea->setBottomAnchor(PosAnchor{rootId, AnchorType::Bottom});
            ea->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
            paint->paint(edge.entity, "rule-hair");
            root.get<Prefab>()->addToPrefab(edge.entity);
            g.edge = edge.entity;

            return g;
        }

        // ── Tooltip ──────────────────────────────────────────────────────────
        const float W = spec.width > 0.0f ? spec.width : TOOLTIP_W;
        const float inner = W - 2.0f * PAD;
        const int z = spec.z;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        const _unique_id rootId = root.id;
        g.root = root.entity;

        auto ground = makeUiSimple2DShape(ecs, Shape2D::Square, 1.0f, 1.0f, tokens.colour("folio"));
        ground.get<UiAnchor>()->fillIn(root.get<UiAnchor>());
        ground.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        paint->paint(ground.entity, "folio");
        root.get<Prefab>()->addToPrefab(ground.entity);
        g.ground = ground.entity;

        auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, tokens.colour("rule-ruled"), 1.0f);
        frame.get<UiAnchor>()->fillIn(root.get<UiAnchor>());
        frame.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});
        paint->paint(frame.entity, "rule-ruled");
        root.get<Prefab>()->addToPrefab(frame.entity);
        g.frame = frame.entity;

        // Stack the parts top to bottom at `inner` wide, tracking the running y.
        float y = PAD;
        auto stack = [&](const std::string& style, const std::string& text, const std::string& colour,
                         Overflow overflow, float width, bool alignRight) -> Label
        {
            LabelSpec ls; ls.style = style; ls.text = text; ls.colour = colour;
            ls.overflow = overflow; ls.width = width; ls.z = z;
            if (alignRight) ls.align = Align::Right;
            Label l = makeLabel(ecs, tokens, styles, ls);
            auto a = l.box->get<UiAnchor>();
            if (alignRight) { a->setRightAnchor(PosAnchor{rootId, AnchorType::Right}); a->setRightMargin(PAD); }
            else            { a->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});   a->setLeftMargin(PAD); }
            a->setTopAnchor(PosAnchor{rootId, AnchorType::Top}); a->setTopMargin(y);
            a->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});
            root.get<Prefab>()->addToPrefab(l.box);
            return l;
        };

        if (not spec.title.empty())
        {
            g.title = stack("gloss-title", spec.title, "ink", Overflow::Ellipsis, inner, false);
            y += boxH(*g.title) + GAP1;
        }
        if (not spec.text.empty())
        {
            g.text = stack("body-sm", spec.text, "ink-muted", Overflow::Wrap, inner, false);
            y += boxH(*g.text);
        }
        for (const auto& row : spec.rows)
        {
            y += GAP1;
            Label lbl = stack("body-sm", row.label, "ink-muted", Overflow::Grow, 0.0f, false);
            Label val = stack("figure-sm", row.value, "ink", Overflow::Grow, 0.0f, true);
            y += std::max(boxH(lbl), boxH(val));
            g.rows.emplace_back(lbl, val);
        }
        if (not spec.footnote.empty())
        {
            y += GAP2;
            g.footnote = stack("caption", spec.footnote, "ink-faint", Overflow::Wrap, inner, false);
            y += boxH(*g.footnote);
        }
        y += PAD;
        root.get<PositionComponent>()->setHeight(y);

        return g;
    }

    float Gloss::height(EntitySystem* ecs) const
    {
        return ecs->getEntity(root.id)->get<PositionComponent>()->height;
    }

    void Gloss::setText(EntitySystem* ecs, const TextStyles& styles, const std::string& newText)
    {
        if (not text)
            return;
        text->setText(ecs, styles, newText);
        spec.text = newText;
        if (spec.kind == GlossKind::Margin)
            root->get<PositionComponent>()->setHeight(text->box->get<PositionComponent>()->height);
    }

    // ── GlossRegistry ────────────────────────────────────────────────────────
    GlossRegistry::GlossRegistry(const Tokens* tokens, const TextStyles* styles) : tokens(tokens), styles(styles) {}

    void GlossRegistry::init()
    {
        auto* tip = ecsRef->getSystem<TooltipSystem>();
        if (not tip)
        {
            LOG_ERROR(DOM, "GlossRegistry: no TooltipSystem in this ECS");
            return;
        }

        tip->registerStyle("gloss", [this](EntitySystem& ecs, const TooltipComponent& t) -> EntityRef
        {
            const std::string key = t.text;
            if (const GlossSpec* spec = find(key))
            {
                lastBuilt = makeGloss(&ecs, *tokens, *styles, *spec);
                return lastBuilt.root;
            }

            // A missing gloss is visible, like a missing mark.
            static std::unordered_set<std::string> logged;
            if (logged.insert(key).second)
                LOG_ERROR(DOM, "No gloss registered for key '" << key << "'");

            GlossSpec fallback; fallback.kind = GlossKind::Tooltip; fallback.text = "(no gloss: " + key + ")";
            lastBuilt = makeGloss(&ecs, *tokens, *styles, fallback);
            if (lastBuilt.text)
                ecs.getSystem<PaintSystem>()->paint(lastBuilt.text->text, "vermilion");
            return lastBuilt.root;
        });
    }

    void GlossRegistry::set(const std::string& key, GlossSpec spec)
    {
        spec.kind = GlossKind::Tooltip;
        specs[key] = std::move(spec);
    }

    bool GlossRegistry::has(const std::string& key) const { return specs.count(key) > 0; }
    void GlossRegistry::erase(const std::string& key) { specs.erase(key); }

    const GlossSpec* GlossRegistry::find(const std::string& key) const
    {
        auto it = specs.find(key);
        return it == specs.end() ? nullptr : &it->second;
    }

    void attachGloss(EntitySystem* ecs, EntityRef target, const std::string& key, uint32_t delayMs)
    {
        auto tc = ecs->attach<TooltipComponent>(target, key, "gloss");
        tc->delayMs = delayMs;
    }
}
