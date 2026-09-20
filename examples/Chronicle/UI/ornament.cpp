#include "ornament.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "logger.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/iconsystem.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"

#include "paint.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Ornament";

        // Cormorant Garamond's cap-height as a fraction of its em, used to centre the versal's
        // cap in the frame. Measured against the design system's versal; adjust once, never per call.
        constexpr float kCormorantCapRatio = 0.63f;

        std::string firstCodePoint(const std::string& s)
        {
            if (s.empty())
                return s;
            const unsigned char c = static_cast<unsigned char>(s[0]);
            size_t len = 1;
            if (c >= 0xF0)      len = 4;
            else if (c >= 0xE0) len = 3;
            else if (c >= 0xC0) len = 2;
            return s.substr(0, std::min(len, s.size()));
        }

        std::string toneToken(VersalTone tone)
        {
            switch (tone)
            {
            case VersalTone::Gold:  return "gold-edge";
            case VersalTone::Lapis: return "lapis";
            case VersalTone::Vermilion:
            default:                return "vermilion";
            }
        }

        std::string cornerName(CornerPos pos)
        {
            switch (pos)
            {
            case CornerPos::TR: return "corner-tr";
            case CornerPos::BL: return "corner-bl";
            case CornerPos::BR: return "corner-br";
            case CornerPos::TL:
            default:            return "corner-tl";
            }
        }

        // The seven ornament drawings, authored on square viewBoxes.
        const std::vector<std::string> ORNAMENT_NAMES = {
            "knot", "flourish",
            "corner-tl", "corner-tr", "corner-bl", "corner-br",
            "versal-curls",
        };

        std::unordered_set<EntitySystem*>& registeredIn()
        {
            static std::unordered_set<EntitySystem*> instances;
            return instances;
        }
    }

    bool registerOrnaments(EntitySystem* ecs, const std::string& iconRoot)
    {
        auto* icons = ecs->getSystem<IconSystem>();
        if (not icons)
        {
            LOG_ERROR(DOM, "registerOrnaments: no IconSystem in this ECS");
            return false;
        }

        if (not registeredIn().insert(ecs).second)
            return true;

        std::vector<std::string> paths;
        paths.reserve(ORNAMENT_NAMES.size());
        for (const auto& name : ORNAMENT_NAMES)
            paths.push_back(iconRoot + "/" + name + ".svg");

        icons->registerIconSet("chronicle-ornaments", paths, {22, 28, 72, 120});
        return true;
    }

    Ornament makeOrnament(EntitySystem* ecs, const Tokens& tokens, const TextStyles& styles, const OrnamentSpec& spec)
    {
        auto* paint = ecs->getSystem<PaintSystem>();

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(spec.z));
        const _unique_id rootId = root.id;

        Ornament orn;
        orn.root = root.entity;
        orn.kind = spec.kind;

        // Attach one child: anchor it, constrain its z to root+offset, paint it, prefab it, record it.
        auto add = [&](EntityRef child, const std::string& token, int zOffset, bool inked)
        {
            child->get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, static_cast<float>(zOffset)});
            paint->paint(child, token);
            root.get<Prefab>()->addToPrefab(child);
            orn.parts.push_back(child);
            if (inked)
                orn.inked.push_back(child);
        };

        switch (spec.kind)
        {
        case OrnamentKind::Divider:
        {
            const float h = tokens.border(spec.weight == DividerWeight::Hair ? "border-hair" : "border-rule");
            const std::string lineToken = spec.weight == DividerWeight::Hair ? "rule-hair" : "rule-ruled";

            root.get<PositionComponent>()->setWidth(spec.width);
            root.get<PositionComponent>()->setHeight(h);

            auto line = makeUiSimple2DShape(ecs, Shape2D::Square, 1.0f, h, tokens.colour(lineToken));
            auto la = line.get<UiAnchor>();
            la->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            la->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            la->setHeightConstrain(PosConstrain{rootId, AnchorType::Height});
            add(line.entity, lineToken, 1, /*inked*/ true);

            if (spec.knot)
            {
                auto patch = makeUiSimple2DShape(ecs, Shape2D::Square, 22.0f, 10.0f, tokens.colour(spec.ground));
                auto pa = patch.get<UiAnchor>();
                pa->setHorizontalCenter(PosAnchor{rootId, AnchorType::HorizontalCenter});
                pa->setVerticalCenter(PosAnchor{rootId, AnchorType::VerticalCenter});
                add(patch.entity, spec.ground, 1, /*inked*/ false);   // the ground patch keeps its own token

                auto knot = makeIcon(ecs, "chronicle-ornaments", "knot", 22.0f, tokens.colour(lineToken));
                auto ka = knot.get<UiAnchor>();
                ka->setHorizontalCenter(PosAnchor{rootId, AnchorType::HorizontalCenter});
                ka->setVerticalCenter(PosAnchor{rootId, AnchorType::VerticalCenter});
                add(knot.entity, lineToken, 2, /*inked*/ true);
            }
            break;
        }

        case OrnamentKind::Flourish:
        {
            const std::string token = spec.colour.empty() ? "rule-ruled" : spec.colour;
            root.get<PositionComponent>()->setWidth(120.0f);
            root.get<PositionComponent>()->setHeight(18.0f);

            auto icon = makeIcon(ecs, "chronicle-ornaments", "flourish", 120.0f, tokens.colour(token));
            auto ia = icon.get<UiAnchor>();
            ia->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ia->setVerticalCenter(PosAnchor{rootId, AnchorType::VerticalCenter});
            add(icon.entity, token, 1, /*inked*/ true);
            break;
        }

        case OrnamentKind::Corner:
        {
            const std::string token = spec.colour.empty() ? "gold-edge" : spec.colour;
            root.get<PositionComponent>()->setWidth(28.0f);
            root.get<PositionComponent>()->setHeight(28.0f);

            auto icon = makeIcon(ecs, "chronicle-ornaments", cornerName(spec.corner), 28.0f, tokens.colour(token));
            auto ia = icon.get<UiAnchor>();
            ia->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ia->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            add(icon.entity, token, 1, /*inked*/ true);
            break;
        }

        case OrnamentKind::Versal:
        {
            const std::string token = spec.colour.empty() ? toneToken(spec.tone) : spec.colour;
            root.get<PositionComponent>()->setWidth(72.0f);
            root.get<PositionComponent>()->setHeight(72.0f);

            // Frame: a 64x64 stroke inset 4 px on every side. G4 draws the stroke inside the quad.
            auto frame = makeStrokeRect2DShape(ecs, 64.0f, 64.0f, tokens.colour(token), 3.0f);
            auto fa = frame.get<UiAnchor>();
            fa->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});   fa->setLeftMargin(4.0f);
            fa->setRightAnchor(PosAnchor{rootId, AnchorType::Right}); fa->setRightMargin(4.0f);
            fa->setTopAnchor(PosAnchor{rootId, AnchorType::Top});     fa->setTopMargin(4.0f);
            fa->setBottomAnchor(PosAnchor{rootId, AnchorType::Bottom}); fa->setBottomMargin(4.0f);
            add(frame.entity, token, 1, /*inked*/ true);

            auto curls = makeIcon(ecs, "chronicle-ornaments", "versal-curls", 72.0f, tokens.colour(token));
            auto ca = curls.get<UiAnchor>();
            ca->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ca->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            add(curls.entity, token, 1, /*inked*/ true);

            // Letter: chapter style, centred horizontally in the 72 box, nudged so its cap
            // (not its line box) is centred at the root's middle.
            LabelSpec ls;
            ls.style = "chapter"; ls.text = firstCodePoint(spec.letter); ls.colour = token;
            ls.align = Align::Centre; ls.overflow = Overflow::Ellipsis; ls.width = 72.0f;
            ls.z = spec.z;
            Label lab = makeLabel(ecs, tokens, styles, ls);

            const TextStyle& cs = styles.get("chapter");
            const float ascender = ecs->getSystem<TTFTextSystem>()
                ->measureText(cs.fontAlias, ls.text, 1.0f, 0.0f, 0.0f, cs.letterSpacingPx).ascender;
            const float capHeight = kCormorantCapRatio * 44.0f;
            const float labelTop = 36.0f - ascender + capHeight * 0.5f;

            auto lba = lab.box->get<UiAnchor>();
            lba->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            lba->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            lba->setTopMargin(labelTop);
            lba->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 1.0f});   // text is box z + 1 = root + 2
            root.get<Prefab>()->addToPrefab(lab.box);
            orn.letter = lab;
            break;
        }
        }

        return orn;
    }

    void Ornament::setColour(EntitySystem* ecs, const std::string& token)
    {
        auto* paint = ecs->getSystem<PaintSystem>();
        for (auto& e : inked)
            paint->paint(e, token);
        if (letter)
            letter->setColour(ecs, token);
    }

    void Ornament::setLetter(EntitySystem* ecs, const TextStyles& styles, const std::string& newLetter)
    {
        if (letter)
            letter->setText(ecs, styles, firstCodePoint(newLetter));
    }
}
