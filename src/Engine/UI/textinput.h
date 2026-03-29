#pragma once

#include "ECS/entitysystem.h"

#include "Input/inputcomponent.h"

#include "2D/simple2dobject.h"

#include "Systems/coresystems.h"

#include "sentencesystem.h"
#include "ttftext.h"
#include "focusable.h"

namespace pg
{
    enum class AcceptableTextInput
    {
        AllCharacters = 0,
        Numericals = 1,
        NoSymbols = 2
    };

    struct CurrentTextInputTextChanged
    {
        std::string text;
        _unique_id id;
    };

    struct __InternalCurrentTextInputTextChanged
    {
        std::string text;
        _unique_id id;
    };

    struct TextInputComponent : public Component
    {
        DEFAULT_COMPONENT_MEMBERS(TextInputComponent)

        TextInputComponent(StandardEvent event, const std::string& defaultText = "") : event(event), text(defaultText) { LOG_THIS_MEMBER("TextInputComponent"); }

        void setText(const std::string& text)
        {
            ecsRef->sendEvent(__InternalCurrentTextInputTextChanged{text, entityId});
        }

        StandardEvent event;

        std::string text;
        std::string returnText;

        // Cursor position within the text (index into the string, 0 = before first char)
        size_t cursorPos = 0;

        // Entity used to render the cursor visual (a small square)
        EntityRef cursorEntity;

        // Todo add support for all those option in the text input !
        bool clearTextAfterEnter = true;
        bool acceptMultilines = false;

        size_t minWidth = 50;
        size_t minHeight = 10;

        AcceptableTextInput acceptableInput = AcceptableTextInput::AllCharacters;
    };

    struct TextInputSystem: public System<Own<TextInputComponent>, Ref<FocusableComponent>,
        Listener<OnSDLTextInput>, Listener<OnSDLScanCode>,
        Listener<EntityChangedEvent>, Listener<OnFocus>, Listener<TickEvent>,
        QueuedListener<__InternalCurrentTextInputTextChanged>, InitSys>
    {
        TextInputSystem(Input* inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("Text Input System"); }

        virtual void init() override
        {
            registerGroup<TextInputComponent, FocusableComponent>();
        }

        virtual std::string getSystemName() const override { return "Text Input System"; }

        virtual void onEvent(const OnSDLTextInput& event) override;

        virtual void onEvent(const OnSDLScanCode& event) override;

        virtual void onEvent(const OnFocus& event) override;

        virtual void onEvent(const TickEvent& event) override;

        virtual void onEvent(const EntityChangedEvent& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent or not ent->has<TextInputComponent>() or not ent->has<PositionComponent>())
                return;

            auto pos = ent->get<PositionComponent>();
            auto input = ent->get<TextInputComponent>();

            if (pos->width < input->minWidth)
                pos->setWidth(input->minWidth);

            if (pos->height < input->minHeight)
                pos->setHeight(input->minHeight);
        }

        virtual void onProcessEvent(const __InternalCurrentTextInputTextChanged& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent)
                return;

            auto text = ent->get<TextInputComponent>();

            text->text = event.text;
            text->cursorPos = text->text.size();

            if (ent->has<TTFText>())
            {
                ent->get<TTFText>()->setText(text->text);
            }

            updateCursorVisual(ent, text);

            ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, event.id});
        }

        virtual void execute() override;

        /** Recomputes the cursor entity position based on the current text and cursor index. */
        void updateCursorVisual(EntityRef entity, CompRef<TextInputComponent> textComp);

        /** Resets the blink timer so the cursor stays visible right after user interaction. */
        void resetBlinkTimer();

        Input *inputHandler;

        // Cursor blink state
        float blinkTimer = 0.0f;
        float blinkInterval = 0.5f; // seconds per on/off half-cycle
        bool cursorVisible = true;
    };

    // template <typename Type>
    // CompList<PositionComponent, UiAnchor, SentenceText, FocusableComponent, TextInputComponent> makeTextInput(Type *ecs, float x, float y, StandardEvent event, const SentenceText& defaultText = {"Input"})
    // {
    //     LOG_THIS("Text Input System");

    //     auto entity = ecs->createEntity();

    //     auto ui = ecs->template attach<PositionComponent>(entity);

    //     ui->setX(x);
    //     ui->setY(y);

    //     auto sentence = ecs->template attach<SentenceText>(entity, defaultText);

    //     ui->setWidth(sentence->textWidth);
    //     ui->setHeight(sentence->textHeight);

    //     auto anchor = ecs->template attach<UiAnchor>(entity);

    //     auto focused = ecs->template attach<FocusableComponent>(entity);

    //     ecs->template attach<MouseLeftClickComponent>(entity, makeCallable<OnFocus>(OnFocus{entity.id}) );

    //     auto textInputComp = ecs->template attach<TextInputComponent>(entity, event, defaultText.getText());

    //     return {entity, ui, anchor, sentence, focused, textInputComp};
    // }

    template <typename Type>
    CompList<PositionComponent, UiAnchor, TTFText, FocusableComponent, TextInputComponent> makeTTFTextInput(Type *ecs, float x, float y, StandardEvent event, const std::string& font, const std::string& defaultText = "Input", float size = 1)
    {
        LOG_THIS("Text Input System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto sentence = ecs->template attach<TTFText>(entity, defaultText, font, size);

        ui->setWidth(sentence->textWidth);
        ui->setHeight(sentence->textHeight);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto focused = ecs->template attach<FocusableComponent>(entity);

        ecs->template attach<MouseLeftClickComponent>(entity, makeCallable<OnFocus>(OnFocus{entity.id}) );

        auto textInputComp = ecs->template attach<TextInputComponent>(entity, event, defaultText);

        textInputComp->cursorPos = defaultText.size();

        // Create a small square entity as the text cursor visual
        float cursorWidth = 2.0f;
        float cursorHeight = sentence->textHeight > 0 ? sentence->textHeight : 14.0f * size;

        auto cursorEnt = makeUiSimple2DShape(ecs, Shape2D::Square, cursorWidth, cursorHeight, {255.0f, 255.0f, 255.0f, 255.0f});

        auto cursorAnchor = cursorEnt.template get<UiAnchor>();
        auto cursorUi = cursorEnt.template get<PositionComponent>();

        // Anchor the cursor to the text input's left edge, offset by text width
        cursorAnchor->setTopAnchor({entity.id, AnchorType::Top});
        cursorAnchor->setLeftAnchor({entity.id, AnchorType::Left});
        cursorAnchor->setLeftMargin(sentence->textWidth);
        cursorAnchor->setZConstrain({entity.id, AnchorType::Z, PosOpType::Add, 1.0f});

        // Start hidden (shown when focused)
        cursorUi->setVisible(false);

        textInputComp->cursorEntity = cursorEnt.entity;

        return {entity, ui, anchor, sentence, focused, textInputComp};
    }
}