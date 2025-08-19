#pragma once

#include "ECS/entitysystem.h"

#include "Input/inputcomponent.h"

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
        DEFAULT_COMPONENT_MEMBERS(Component)

        TextInputComponent(StandardEvent event, const std::string& defaultText = "") : event(event), text(defaultText) { LOG_THIS_MEMBER("TextInputComponent"); }

        void setText(const std::string& text)
        {
            ecsRef->sendEvent(__InternalCurrentTextInputTextChanged{text, entityId});
        }

        StandardEvent event;

        std::string text;
        std::string returnText;

        // Todo add support for all those option in the text input !
        bool clearTextAfterEnter = true;
        bool acceptMultilines = false;

        size_t minWidth = 50;
        size_t minHeight = 10;

        AcceptableTextInput acceptableInput = AcceptableTextInput::AllCharacters;
    };

    struct TextInputSystem: public System<Own<TextInputComponent>, Ref<FocusableComponent>, Listener<OnSDLTextInput>, Listener<OnSDLScanCode>, QueuedListener<__InternalCurrentTextInputTextChanged>, InitSys>
    {
        TextInputSystem(Input* inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("Text Input System"); }

        virtual void init() override
        {
            registerGroup<TextInputComponent, FocusableComponent>();
        }

        virtual std::string getSystemName() const override { return "Text Input System"; }

        virtual void onEvent(const OnSDLTextInput& event) override;

        virtual void onEvent(const OnSDLScanCode& event) override;

        virtual void onProcessEvent(const __InternalCurrentTextInputTextChanged& event) override
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent)
                return;

            auto text = ent->get<TextInputComponent>();

            text->text = event.text;

            if (ent->has<SentenceText>())
            {
                ent->get<SentenceText>()->setText(text->text);
            }

            if (ent->has<TTFText>())
            {
                ent->get<TTFText>()->setText(text->text);
            }

            ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, event.id});
        }

        virtual void execute() override;

        Input *inputHandler;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, SentenceText, FocusableComponent, TextInputComponent> makeTextInput(Type *ecs, float x, float y, StandardEvent event, const SentenceText& defaultText = {"Input"})
    {
        LOG_THIS("Text Input System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto sentence = ecs->template attach<SentenceText>(entity, defaultText);

        ui->setWidth(sentence->textWidth);
        ui->setHeight(sentence->textHeight);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto focused = ecs->template attach<FocusableComponent>(entity);

        ecs->template attach<MouseLeftClickComponent>(entity, makeCallable<OnFocus>(OnFocus{entity.id}) );

        auto textInputComp = ecs->template attach<TextInputComponent>(entity, event, defaultText.getText());

        return {entity, ui, anchor, sentence, focused, textInputComp};
    }

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

        return {entity, ui, anchor, sentence, focused, textInputComp};
    }
}