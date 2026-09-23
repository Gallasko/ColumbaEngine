#include "panel.h"

#include <unordered_map>

#include "logger.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/sizer.h"
#include "UI/ttftext.h"

#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Panel";

        constexpr float TITLE_BOX = 26.0f;                              // the heading's line box
        constexpr float HEAD_BLOCK = TITLE_BOX + 12.0f + 1.0f + 12.0f;  // 51: title + gap + rule + gap
        constexpr float GLYPH_ADVANCE = 18.0f + 8.0f;                   // mark width + space-2

        float paddingFor(PanelFrame f, const Tokens& t)
        {
            switch (f)
            {
            case PanelFrame::Plain:
                return 0.0f;

            case PanelFrame::Illuminated:
                return t.space(5);

            default:
                return t.space(4);
            }
        }

        // Ascender of a style's "H" (a property of the font atlas). Measured fresh: the atlas is
        // per-ECS, so a process-wide cache keyed on style name would leak values across ECS instances.
        float ascenderOf(EntitySystem* ecs, const TextStyles& styles, const std::string& style)
        {
            const TextStyle& s = styles.get(style);
            return ecs->getSystem<TTFTextSystem>()->measureText(s.fontAlias, "H", 1.0f, 0.0f, 0.0f, s.letterSpacingPx).ascender;
        }
    }

    Panel makePanel(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const PanelSpec& specIn)
    {
        PanelSpec spec = specIn;

        const float P = paddingFor(spec.frame, tokens);
        const float W = spec.width;
        const float inner = W - 2.0f * P;
        const int z = spec.z;
        const bool hasHead = not spec.heading.empty();
        const float headBlock = hasHead ? HEAD_BLOCK : 0.0f;

        // The content band must clear the head band (z+4).
        if (spec.contentZ <= z + 4)
        {
            LOG_WARNING(DOM, "contentZ " << spec.contentZ << " must be > z+4; using " << (z + 10));
            spec.contentZ = z + 10;
        }

        Panel panel;
        panel.spec = spec;
        panel.padding = P;
        panel.headBlock = headBlock;
        panel.styles = &styles;

        // root: width fixed, height a constraint on the body.
        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(W);
        const _unique_id rootId = root.id;
        panel.root = root.entity;

        // body: a vertical layout inset by the padding; height free, so it grows with its rows.
        auto body = makeVerticalLayout(ecs, 0.0f, 0.0f, inner, 0.0f);
        body.get<VerticalLayout>()->spacing = static_cast<size_t>(tokens.space(3));

        auto ba = body.get<UiAnchor>();
        ba->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
        ba->setLeftMargin(P);
        ba->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
        ba->setRightMargin(P);
        ba->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
        ba->setTopMargin(P + headBlock);

        body.get<PositionComponent>()->setZ(static_cast<float>(spec.contentZ));
        root.get<Prefab>()->addToPrefab(body.entity);
        panel.body = body.entity;

        // root height = body.height + (P + headBlock + P). The body auto-sizes to its rows, but the
        // engine's layout appends its spacing after the last row too, so discount one spacing.
        const float trailing = tokens.space(3);
        root.get<UiAnchor>()->setHeightConstrain(PosConstrain{body.id, AnchorType::Height, PosOpType::Add, 2.0f * P + headBlock - trailing});

        // ground + frame (not Plain).
        if (spec.frame != PanelFrame::Plain)
        {
            auto ground = makeUiSimple2DShape(ecs, Shape2D::Square, 1.0f, 1.0f, tokens.colour("folio"));
            ground.get<UiAnchor>()->fillIn(root.get<UiAnchor>());
            ground.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z});

            ecs->attach<PaintComponent>(ground.entity, "folio");

            root.get<Prefab>()->addToPrefab(ground.entity);
            panel.ground = ground.entity;

            std::string frameToken;
            float sw = 1.0f, gap = 0.0f;
            bool doubled = false;

            switch (spec.frame)
            {
            case PanelFrame::Hair:
                sw = tokens.border("border-hair");
                frameToken = "rule-hair";
                break;

            case PanelFrame::Ruled:
                sw = 1.0f;
                frameToken = "rule-ruled";
                break;

            case PanelFrame::Illuminated:
                sw = 1.0f;
                gap = 1.0f;
                doubled = true;
                frameToken = "gold-edge";
                break;

            default:
                break;
            }

            auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, tokens.colour(frameToken), sw, gap, doubled);
            frame.get<UiAnchor>()->fillIn(root.get<UiAnchor>());
            frame.get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});

            ecs->attach<PaintComponent>(frame.entity, frameToken);

            root.get<Prefab>()->addToPrefab(frame.entity);
            panel.frame = frame.entity;
        }

        // corners (Illuminated): 2 px inside the frame.
        if (spec.frame == PanelFrame::Illuminated)
        {
            const CornerPos poss[4] = {CornerPos::TL, CornerPos::TR, CornerPos::BL, CornerPos::BR};

            for (auto pos : poss)
            {
                OrnamentSpec cs;
                cs.kind = OrnamentKind::Corner;
                cs.corner = pos;
                cs.z = z + 2;

                Ornament c = makeOrnament(ecs, tokens, styles, cs);
                auto ca = c.root->get<UiAnchor>();
                const bool right = (pos == CornerPos::TR or pos == CornerPos::BR);
                const bool bottom = (pos == CornerPos::BL or pos == CornerPos::BR);

                if (right)
                {
                    ca->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
                    ca->setRightMargin(2.0f);
                }
                else
                {
                    ca->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
                    ca->setLeftMargin(2.0f);
                }

                if (bottom)
                {
                    ca->setBottomAnchor(PosAnchor{rootId, AnchorType::Bottom});
                    ca->setBottomMargin(2.0f);
                }
                else
                {
                    ca->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
                    ca->setTopMargin(2.0f);
                }

                root.get<Prefab>()->addToPrefab(c.root);
                panel.corners.push_back(c);
            }
        }

        // head (when there is a heading), all at z+3.
        if (hasHead)
        {
            const int headZ = z + 3;

            float asideWidth = 0.0f;
            if (not spec.aside.empty())
            {
                LabelSpec as;
                as.style = "label";
                as.text = spec.aside;
                as.colour = "ink-muted";
                as.z = headZ;

                Label a = makeLabel(ecs, tokens, styles, as);
                asideWidth = a.box->get<PositionComponent>()->width;
                auto aa = a.box->get<UiAnchor>();
                aa->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
                aa->setRightMargin(P);

                // Align the aside's baseline on the heading's: shift its box top by the ascender gap.
                // (The title is set in the "heading" style; spec.heading is its text, not a style.)
                const float baseline = ascenderOf(ecs, styles, "heading") - ascenderOf(ecs, styles, "label");
                aa->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
                aa->setTopMargin(P + baseline);

                root.get<Prefab>()->addToPrefab(a.box);
                panel.aside = a;
            }

            float glyphAdvance = 0.0f;
            if (not spec.glyph.empty())
            {
                Mark m = makeMark(ecs, tokens, {spec.glyph, MarkSize::S18, "ink-muted", headZ});

                auto ma = m.entity->get<UiAnchor>();
                ma->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
                ma->setLeftMargin(P);
                ma->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
                ma->setTopMargin(P + (TITLE_BOX - 18.0f) / 2.0f);

                root.get<Prefab>()->addToPrefab(m.entity);
                panel.glyph = m;
                glyphAdvance = GLYPH_ADVANCE;
            }

            const float titleWidth = inner - glyphAdvance - (asideWidth > 0.0f ? asideWidth + 8.0f : 0.0f);

            LabelSpec ts;
            ts.style = "heading";
            ts.text = spec.heading;
            ts.colour = "ink";
            ts.align = Align::Left;
            ts.overflow = Overflow::Ellipsis;
            ts.width = titleWidth;
            ts.z = headZ;

            Label t = makeLabel(ecs, tokens, styles, ts);
            auto ta = t.box->get<UiAnchor>();
            ta->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ta->setLeftMargin(P + glyphAdvance);
            ta->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            ta->setTopMargin(P);

            root.get<Prefab>()->addToPrefab(t.box);
            panel.title = t;

            OrnamentSpec rs;
            rs.kind = OrnamentKind::Divider;
            rs.weight = DividerWeight::Hair;
            rs.knot = false;
            rs.width = 0.0f;
            rs.z = headZ;

            Ornament r = makeOrnament(ecs, tokens, styles, rs);
            auto ra = r.root->get<UiAnchor>();
            ra->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ra->setLeftMargin(P);
            ra->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            ra->setRightMargin(P);
            ra->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            ra->setTopMargin(P + TITLE_BOX + 12.0f);

            root.get<Prefab>()->addToPrefab(r.root);
            panel.rule = r;
        }

        return panel;
    }

    void Panel::addChild(EntitySystem*, EntityRef child)
    {
        body->get<VerticalLayout>()->addEntity(child);
    }

    void Panel::removeChild(EntitySystem*, EntityRef child)
    {
        body->get<VerticalLayout>()->removeEntity(child);
    }

    void Panel::setHeading(EntitySystem* ecs, const TextStyles& s, const std::string& text)
    {
        if (text.empty())
        {
            LOG_WARNING(DOM, "setHeading(\"\") is not allowed after construction; ignored");
            return;
        }

        if (title)
            title->setText(ecs, s, text);
    }

    void Panel::setAside(EntitySystem* ecs, const TextStyles& s, const std::string& text)
    {
        if (text.empty())
        {
            // Remove the aside and widen the title back to the glyph-only inner width.
            if (aside)
            {
                ecs->removeEntity(aside->box.id);
                aside.reset();
            }

            if (title)
            {
                const float glyphAdvance = glyph ? GLYPH_ADVANCE : 0.0f;
                title->setWidth(ecs, s, innerWidth() - glyphAdvance);
            }

            return;
        }

        if (aside)
            aside->setText(ecs, s, text);
    }

    void Panel::setWidth(EntitySystem* ecs, float w)
    {
        spec.width = w;
        root->get<PositionComponent>()->setWidth(w);

        if (title and styles)
        {
            const float glyphAdvance = glyph ? GLYPH_ADVANCE : 0.0f;
            const float asideW = aside ? aside->box->get<PositionComponent>()->width : 0.0f;
            const float titleWidth = innerWidth() - glyphAdvance - (asideW > 0.0f ? asideW + 8.0f : 0.0f);

            title->setWidth(ecs, *styles, titleWidth);
        }
    }

    float Panel::width(EntitySystem* ecs) const { return ecs->getEntity(root.id)->get<PositionComponent>()->width; }
    float Panel::height(EntitySystem* ecs) const { return ecs->getEntity(root.id)->get<PositionComponent>()->height; }
    float Panel::innerWidth() const { return spec.width - 2.0f * padding; }
}
