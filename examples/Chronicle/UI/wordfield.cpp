#include "wordfield.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#elif __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include "ECS/callable.h"
#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/prefab.h"
#include "UI/ttftext.h"
#include "UI/themesystem.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        struct WordNoOp {};

        constexpr float FACE_H = 44.0f;
        constexpr float PAD_X = 12.0f;     // space-3
        constexpr float LINE_H = 20.0f;    // The control style's line
        constexpr float CARET_W = 2.0f;

        // Where the caret stands after the word: the pen's end, so a space typed last moves it
        // at once (the measure's width trims trailing spaces, which leave no ink)
        float wordWidth(EntitySystem* ecs, const std::string& text)
        {
            auto* theme = ecs->getSystem<ThemeSystem>();
            const TextStyle& style = theme->style("control");

            return ecs->getSystem<TTFTextSystem>()->measureText(style.fontAlias, text, 1.0f, 0.0f, 0.0f, style.letterSpacingPx).penEnd;
        }
    }

    WordField makeWordField(EntitySystem* ecs, const WordFieldSpec& spec)
    {
        auto* theme = ecs->getSystem<ThemeSystem>();
        const int z = spec.z;

        WordField f;
        f.spec = spec;

        auto root = makeAnchoredPrefab(ecs, 0.0f, 0.0f, static_cast<float>(z));
        root.get<PositionComponent>()->setWidth(spec.width);
        root.get<PositionComponent>()->setHeight(FACE_H);
        const _unique_id rootId = root.id;
        f.root = root.entity;

        auto face = ecs->createEntity();
        auto fp = ecs->attach<PositionComponent>(face);
        fp->setWidth(spec.width); fp->setHeight(FACE_H); fp->setZ(static_cast<float>(z));
        auto fa = ecs->attach<UiAnchor>(face);
        fa->fillIn(root.get<UiAnchor>());
        fa->setZConstrain(PosConstrain{rootId, AnchorType::Z});
        ecs->attach<FocusableComponent>(face);
        auto st = ecs->attach<WordFieldState>(face);
        st->tag = spec.tag; st->most = spec.most;
        const _unique_id faceId = face.id;
        root.get<Prefab>()->addToPrefab(face, "face");
        f.face = face;

        if (auto* fo = ecs->getSystem<FocusOrderSystem>())
            fo->add(faceId);

        auto add = [&root](EntityRef e) { root.get<Prefab>()->addToPrefab(e); };

        // The well, its frame
        auto ground = makeRoundedRect2DShape(ecs, theme->radius("radius-md"), 1.0f, 1.0f);
        ground.get<UiAnchor>()->fillIn(fa);
        ground.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z});
        ecs->attach<ThemeComponent>(ground.entity, "field.ground");
        add(ground.entity);
        f.ground = ground.entity;

        auto frame = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f}, 2.0f);
        frame.get<UiAnchor>()->fillIn(fa);
        frame.get<UiAnchor>()->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 1.0f});
        ecs->attach<ThemeComponent>(frame.entity, "field.frame");
        add(frame.entity);
        f.frame = frame.entity;
        st->frame = frame.entity.id;

        // The word, and the placeholder in its place while it is empty
        auto placeText = [&](const std::string& text, const std::string& color) {
            LabelSpec ls; ls.style = "control"; ls.text = text; ls.color = color; ls.z = z + 3;
            Label label = makeLabel(ecs, ls);
            auto la = label.entity->get<UiAnchor>();
            la->setLeftAnchor(PosAnchor{faceId, AnchorType::Left}); la->setLeftMargin(PAD_X);
            la->setTopAnchor(PosAnchor{faceId, AnchorType::Top}); la->setTopMargin((FACE_H - LINE_H) / 2.0f);
            la->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 3.0f});
            add(label.entity);
            return label;
        };

        f.word = placeText("", "ink");
        st->word = f.word.entity.id;

        f.placeholder = placeText(spec.placeholder, "ink-faint");
        st->placeholder = f.placeholder.entity.id;

        // The caret after the word, while it takes what is typed
        auto caret = makeUiSimple2DShape(ecs, Shape2D::Square, CARET_W, LINE_H);
        auto ca = caret.get<UiAnchor>();
        ca->setLeftAnchor(PosAnchor{faceId, AnchorType::Left}); ca->setLeftMargin(PAD_X);
        ca->setTopAnchor(PosAnchor{faceId, AnchorType::Top}); ca->setTopMargin((FACE_H - LINE_H) / 2.0f);
        ca->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 4.0f});
        caret.get<PositionComponent>()->setVisible(false);
        ecs->attach<ThemeComponent>(caret.entity, "field.caret");
        add(caret.entity);
        f.caret = caret.entity;
        st->caret = caret.entity.id;

        // ring: 2 px focus-ink, 4 px outside the well
        auto ring = makeStrokeRect2DShape(ecs, 1.0f, 1.0f, {255.0f, 255.0f, 255.0f, 255.0f}, theme->border("border-rule"));
        auto ra = ring.get<UiAnchor>();
        ra->setLeftAnchor(PosAnchor{faceId, AnchorType::Left});     ra->setLeftMargin(-4.0f);
        ra->setRightAnchor(PosAnchor{faceId, AnchorType::Right});   ra->setRightMargin(-4.0f);
        ra->setTopAnchor(PosAnchor{faceId, AnchorType::Top});       ra->setTopMargin(-4.0f);
        ra->setBottomAnchor(PosAnchor{faceId, AnchorType::Bottom}); ra->setBottomMargin(-4.0f);
        ra->setZConstrain(PosConstrain{faceId, AnchorType::Z, PosOpType::Add, 4.0f});
        ring.get<PositionComponent>()->setVisible(false);
        ecs->attach<ThemeComponent>(ring.entity, "field.ring");
        add(ring.entity);
        f.ring = ring.entity;
        st->ring = ring.entity.id;

        return f;
    }

    std::string WordField::text(EntitySystem*) const
    {
        return face.empty() ? std::string() : face.get<WordFieldState>()->text;
    }

    void WordField::setText(EntitySystem* ecs, const std::string& text)
    {
        if (auto* sys = ecs->getSystem<WordFieldSystem>())
            sys->write(face, text, false);
    }

    void WordField::focus(EntitySystem* ecs)
    {
        if (auto* sys = ecs->getSystem<WordFieldSystem>())
            sys->setTyping(face, true);

        // The Tab order goes on from here
        if (auto* fo = ecs->getSystem<FocusOrderSystem>())
            fo->focus(face.id);
    }

    void WordField::setWidth(EntitySystem*, float width)
    {
        spec.width = width;

        root->get<PositionComponent>()->setWidth(width);
        face->get<PositionComponent>()->setWidth(width);
    }

    // ── WordFieldSystem ────────────────────────────────────────────────────────
    void WordFieldSystem::init()
    {
        auto group = registerGroup<WordFieldState>();
        group->addOnGroup([this](EntityRef entity) {
            if (not entity->has<MouseEnterComponent>())
                ecsRef->attach<MouseEnterComponent>(entity, makeCallable<WordNoOp>(WordNoOp{}));
            if (not entity->has<MouseLeaveComponent>())
                ecsRef->attach<MouseLeaveComponent>(entity, makeCallable<WordNoOp>(WordNoOp{}));
        });
    }

    bool WordFieldSystem::anyTyping()
    {
        for (auto* st : view<WordFieldState>())
            if (st->typing)
                return true;

        return false;
    }

    void WordFieldSystem::applyVisual(EntityRef face)
    {
        auto st = face->get<WordFieldState>();

        if (auto e = ecsRef->getEntity(st->placeholder))
            e->get<PositionComponent>()->setVisible(st->text.empty());

        if (auto e = ecsRef->getEntity(st->caret))
        {
            e->get<UiAnchor>()->setLeftMargin(PAD_X + (st->text.empty() ? 0.0f : wordWidth(ecsRef, st->text) + 1.0f));
            e->get<PositionComponent>()->setVisible(st->typing);
        }

        if (auto e = ecsRef->getEntity(st->ring))
            e->get<PositionComponent>()->setVisible(st->typing);

        if (auto e = ecsRef->getEntity(st->frame))
            e->get<ThemeComponent>()->setElement(st->typing or st->hovered ? "field.frame.lit" : "field.frame");
    }

    void WordFieldSystem::setTyping(EntityRef face, bool typing)
    {
        if (face.empty() or not face->has<WordFieldState>())
            return;

        auto st = face->get<WordFieldState>();

        // One field takes what is typed at a time
        if (typing)
            for (auto* other : view<WordFieldState>())
                if (other->typing and other->entityId != face.id)
                    if (auto e = ecsRef->getEntity(other->entityId))
                    {
                        other->typing = false;
                        applyVisual(e);
                    }

        st->typing = typing;
        applyVisual(face);
    }

    void WordFieldSystem::write(EntityRef face, const std::string& text, bool tell)
    {
        if (face.empty() or not face->has<WordFieldState>())
            return;

        auto st = face->get<WordFieldState>();

        st->text = text.size() > st->most ? text.substr(0, st->most) : text;

        if (auto e = ecsRef->getEntity(st->word))
            e->get<TTFText>()->setText(st->text);

        applyVisual(face);

        if (tell)
            ecsRef->sendEvent(WordChangedEvent{face.id, st->tag, st->text});
    }

    void WordFieldSystem::onEvent(const HoverChangedEvent& event)
    {
        for (auto id : event.entered)
            if (auto e = ecsRef->getEntity(id); e and e->has<WordFieldState>())
            {
                e->get<WordFieldState>()->hovered = true;
                applyVisual(e);
            }

        for (auto id : event.left)
            if (auto e = ecsRef->getEntity(id); e and e->has<WordFieldState>())
            {
                e->get<WordFieldState>()->hovered = false;
                applyVisual(e);
            }
    }

    void WordFieldSystem::onEvent(const OnMouseClick& event)
    {
        if (event.button != 1)
            return;

        for (auto* st : view<WordFieldState>())
        {
            if (st->hovered == st->typing)
                continue;

            if (auto e = ecsRef->getEntity(st->entityId))
                setTyping(e, st->hovered);
        }
    }

    void WordFieldSystem::onEvent(const OnSDLTextInput& event)
    {
        for (auto* st : view<WordFieldState>())
        {
            if (not st->typing)
                continue;

            // Letters only: what a key that is no letter sends (a tab, a return) stays out
            std::string typed;

            for (char c : event.text)
                if (static_cast<unsigned char>(c) >= 0x20 and c != 0x7f)
                    typed += c;

            if (typed.empty() or st->text.size() >= st->most)
                return;

            if (auto e = ecsRef->getEntity(st->entityId))
                write(e, st->text + typed, true);

            return;
        }
    }

    void WordFieldSystem::onEvent(const OnSDLScanCode& event)
    {
        if (event.key != SDL_SCANCODE_BACKSPACE)
            return;

        for (auto* st : view<WordFieldState>())
        {
            if (not st->typing or st->text.empty())
                continue;

            // The last letter, whole: a letter of more than one byte goes as one
            std::string text = st->text;
            size_t cut = text.size() - 1;

            while (cut > 0 and (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
                --cut;

            text.erase(cut);

            if (auto e = ecsRef->getEntity(st->entityId))
                write(e, text, true);

            return;
        }
    }

    void WordFieldSystem::onEvent(const KeyboardFocusChangedEvent& event)
    {
        // The Tab order reaching a field gives it the focus; moving on takes it away. A mouse click
        // (keyboard false) leaves it to the click
        if (not event.keyboard)
            return;

        for (auto* st : view<WordFieldState>())
        {
            const bool here = st->entityId == event.face;

            if (here != st->typing)
                if (auto e = ecsRef->getEntity(st->entityId))
                    setTyping(e, here);
        }
    }
}
