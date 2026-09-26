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
#include "UI/themesystem.h"

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
            if (c >= 0xF0)
                len = 4;
            else if (c >= 0xE0)
                len = 3;
            else if (c >= 0xC0)
                len = 2;

            return s.substr(0, std::min(len, s.size()));
        }

        // The theme defines "ornament.versal" (vermilion), ".gold" and ".lapis".
        std::string toneElement(VersalTone tone)
        {
            switch (tone)
            {
            case VersalTone::Gold:
                return "ornament.versal.gold";

            case VersalTone::Lapis:
                return "ornament.versal.lapis";

            case VersalTone::Vermilion:
            default:
                return "ornament.versal";
            }
        }

        std::string toneToken(VersalTone tone)
        {
            switch (tone)
            {
            case VersalTone::Gold:
                return "gold-edge";

            case VersalTone::Lapis:
                return "lapis";

            case VersalTone::Vermilion:
            default:
                return "vermilion";
            }
        }

        std::string cornerName(CornerPos pos)
        {
            switch (pos)
            {
            case CornerPos::TR:
                return "corner-tr";

            case CornerPos::BL:
                return "corner-bl";

            case CornerPos::BR:
                return "corner-br";

            case CornerPos::TL:
            default:
                return "corner-tl";
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

    Ornament makeOrnament(EntitySystem* ecs, const OrnamentSpec& spec)
    {
        auto* theme = ecs->getSystem<ThemeSystem>();

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(spec.z));
        const _unique_id rootId = root.id;

        Ornament orn;
        orn.root = root.entity;
        orn.kind = spec.kind;

        // Attach one child: anchor it, constrain its z to root+offset, theme it, prefab it, record it.
        auto add = [&](EntityRef child, const std::string& element, int zOffset, bool inked)
        {
            child->get<UiAnchor>()->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, static_cast<float>(zOffset)});
            ecs->attach<ThemeComponent>(child, element);
            root.get<Prefab>()->addToPrefab(child);
            orn.parts.push_back(child);

            if (inked)
                orn.inked.push_back(child);
        };

        switch (spec.kind)
        {
        case OrnamentKind::Divider:
        {
            const float h = theme->border(spec.weight == DividerWeight::Hair ? "border-hair" : "border-rule");
            const std::string lineElement = spec.weight == DividerWeight::Hair ? "ornament.divider.hair" : "ornament.divider.rule";
            orn.inkedElement = lineElement;

            root.get<PositionComponent>()->setWidth(spec.width);
            root.get<PositionComponent>()->setHeight(h);

            auto line = makeUiSimple2DShape(ecs, Shape2D::Square, 1.0f, h);
            auto la = line.get<UiAnchor>();
            la->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            la->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            la->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            la->setHeightConstrain(PosConstrain{rootId, AnchorType::Height});
            add(line.entity, lineElement, 1, /*inked*/ true);

            if (spec.knot)
            {
                auto patch = makeUiSimple2DShape(ecs, Shape2D::Square, 22.0f, 10.0f);
                auto pa = patch.get<UiAnchor>();
                pa->setHorizontalCenter(PosAnchor{rootId, AnchorType::HorizontalCenter});
                pa->setVerticalCenter(PosAnchor{rootId, AnchorType::VerticalCenter});
                add(patch.entity, "ornament.patch." + spec.ground, 1, /*inked*/ false);   // the ground patch keeps its own token

                auto knot = makeIcon(ecs, "chronicle-ornaments", "knot", 22.0f);
                auto ka = knot.get<UiAnchor>();
                ka->setHorizontalCenter(PosAnchor{rootId, AnchorType::HorizontalCenter});
                ka->setVerticalCenter(PosAnchor{rootId, AnchorType::VerticalCenter});
                add(knot.entity, lineElement, 2, /*inked*/ true);
            }
            break;
        }

        case OrnamentKind::Flourish:
        {
            orn.inkedElement = "ornament.flourish";
            const std::string element = spec.color.empty() ? orn.inkedElement : orn.inkedElement + "." + spec.color;
            root.get<PositionComponent>()->setWidth(120.0f);
            root.get<PositionComponent>()->setHeight(18.0f);

            auto icon = makeIcon(ecs, "chronicle-ornaments", "flourish", 120.0f);
            auto ia = icon.get<UiAnchor>();
            ia->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ia->setVerticalCenter(PosAnchor{rootId, AnchorType::VerticalCenter});
            add(icon.entity, element, 1, /*inked*/ true);
            break;
        }

        case OrnamentKind::Corner:
        {
            orn.inkedElement = "ornament.corner";
            const std::string element = spec.color.empty() ? orn.inkedElement : orn.inkedElement + "." + spec.color;
            root.get<PositionComponent>()->setWidth(28.0f);
            root.get<PositionComponent>()->setHeight(28.0f);

            auto icon = makeIcon(ecs, "chronicle-ornaments", cornerName(spec.corner), 28.0f);
            auto ia = icon.get<UiAnchor>();
            ia->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ia->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            add(icon.entity, element, 1, /*inked*/ true);
            break;
        }

        case OrnamentKind::Versal:
        {
            orn.inkedElement = "ornament.versal";
            const std::string token = spec.color.empty() ? toneToken(spec.tone) : spec.color;
            const std::string element = spec.color.empty() ? toneElement(spec.tone) : orn.inkedElement + "." + spec.color;
            root.get<PositionComponent>()->setWidth(72.0f);
            root.get<PositionComponent>()->setHeight(72.0f);

            // Frame: a 64x64 stroke inset 4 px on every side. G4 draws the stroke inside the quad.
            auto frame = makeStrokeRect2DShape(ecs, 64.0f, 64.0f, {255.0f, 255.0f, 255.0f, 255.0f}, 3.0f);

            auto fa = frame.get<UiAnchor>();
            fa->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            fa->setLeftMargin(4.0f);
            fa->setRightAnchor(PosAnchor{rootId, AnchorType::Right});
            fa->setRightMargin(4.0f);
            fa->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            fa->setTopMargin(4.0f);
            fa->setBottomAnchor(PosAnchor{rootId, AnchorType::Bottom});
            fa->setBottomMargin(4.0f);

            add(frame.entity, element, 1, /*inked*/ true);

            auto curls = makeIcon(ecs, "chronicle-ornaments", "versal-curls", 72.0f);
            auto ca = curls.get<UiAnchor>();
            ca->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            ca->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            add(curls.entity, element, 1, /*inked*/ true);

            // Letter: chapter style, centred horizontally in the 72 box, nudged so its cap
            // (not its line box) is centred at the root's middle.
            LabelSpec ls;
            ls.style = "chapter";
            ls.text = firstCodePoint(spec.letter);
            ls.color = token;
            ls.align = Align::Centre;
            ls.overflow = Overflow::Ellipsis;
            ls.width = 72.0f;
            ls.z = spec.z + 1;

            Label lab = makeLabel(ecs, ls);

            const TextStyle& cs = theme->style("chapter");
            const float ascender = ecs->getSystem<TTFTextSystem>()->measureText(cs.fontAlias, ls.text, 1.0f, 0.0f, 0.0f, cs.letterSpacingPx).ascender;
            const float capHeight = kCormorantCapRatio * 44.0f;
            const float labelTop = 36.0f - ascender + capHeight * 0.5f;

            auto lba = lab.entity->get<UiAnchor>();
            lba->setLeftAnchor(PosAnchor{rootId, AnchorType::Left});
            lba->setTopAnchor(PosAnchor{rootId, AnchorType::Top});
            lba->setTopMargin(labelTop);
            lba->setZConstrain(PosConstrain{rootId, AnchorType::Z, PosOpType::Add, 2.0f});   // glyphs at root + 2
            root.get<Prefab>()->addToPrefab(lab.entity);
            orn.letter = lab;
            break;
        }
        }

        return orn;
    }

    void Ornament::setColor(EntitySystem* ecs, const std::string& token)
    {
        for (auto& e : inked)
            e->get<ThemeComponent>()->setElement(inkedElement + "." + token);

        if (letter)
            letter->setColor(ecs, token);
    }

    void Ornament::setLetter(EntitySystem* ecs, const std::string& newLetter)
    {
        if (letter)
            letter->setText(ecs, firstCodePoint(newLetter));
    }
}
