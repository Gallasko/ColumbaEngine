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

    struct TextInputComponent
    {
        TextInputComponent(StandardEvent event, const std::string& defaultText = "") : event(event), text(defaultText) { LOG_THIS_MEMBER("TextInputComponent"); }
        TextInputComponent(const TextInputComponent& rhs) : 
            event(rhs.event),
            text(rhs.text),
            returnText(rhs.returnText),
            clearTextAfterEnter(rhs.clearTextAfterEnter),
            acceptMultilines(rhs.acceptMultilines),
            minWidth(rhs.minWidth), minHeight(rhs.minHeight),
            acceptableInput(rhs.acceptableInput)
        {
            LOG_THIS_MEMBER("TextInputComponent");
        }
        
        virtual ~TextInputComponent() { LOG_THIS_MEMBER("TextInputComponent"); }

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

    struct TextInputSystem: public System<Own<TextInputComponent>, Ref<FocusableComponent>, Listener<OnSDLTextInput>, Listener<OnSDLScanCode>, InitSys, StoragePolicy>
    {
        TextInputSystem(Input* inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("Text Input System"); }

        virtual void init() override
        {
            registerGroup<TextInputComponent, FocusableComponent>();
        }

        virtual std::string getSystemName() const override { return "Text Input System"; }

        virtual void onEvent(const OnSDLTextInput& event) override;

        virtual void onEvent(const OnSDLScanCode& event) override;

        Input *inputHandler;
    };

    template <typename Type>
    CompList<UiComponent, SentenceText, FocusableComponent, TextInputComponent> makeTextInput(Type *ecs, float x, float y, StandardEvent event, const SentenceText& defaultText = {"Input"})
    {
        LOG_THIS("Text Input System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<UiComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto sentence = ecs->template attach<SentenceText>(entity, defaultText);

        ui->setWidth(&sentence->textWidth);
        ui->setHeight(&sentence->textHeight);

        auto focused = ecs->template attach<FocusableComponent>(entity);

        ecs->template attach<MouseLeftClickComponent>(entity, makeCallable<OnFocus>(OnFocus{entity.id}) );

        auto textInputComp = ecs->template attach<TextInputComponent>(entity, event, defaultText.getText());

        return {entity, ui, sentence, focused, textInputComp};
    }

    template <typename Type>
    CompList<UiComponent, TTFText, FocusableComponent, TextInputComponent> makeTTFTextInput(Type *ecs, float x, float y, StandardEvent event, const std::string& font, const std::string& defaultText = "Input", float size = 1)
    {
        LOG_THIS("Text Input System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<UiComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto sentence = ecs->template attach<TTFText>(entity, defaultText, font, size);

        ui->setWidth(&sentence->textWidth);
        ui->setHeight(&sentence->textHeight);

        auto focused = ecs->template attach<FocusableComponent>(entity);

        ecs->template attach<MouseLeftClickComponent>(entity, makeCallable<OnFocus>(OnFocus{entity.id}) );

        auto textInputComp = ecs->template attach<TextInputComponent>(entity, event, defaultText);

        return {entity, ui, sentence, focused, textInputComp};
    }
}