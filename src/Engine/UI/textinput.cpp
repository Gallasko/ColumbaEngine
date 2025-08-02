#include "stdafx.h"

#include "textinput.h"

#include "sentencesystem.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Text Input System";
    }

    void TextInputSystem::onEvent(const OnSDLTextInput& event)
    {
        for (const auto& entity : viewGroup<TextInputComponent, FocusableComponent>())
        {
            auto focus = entity->get<FocusableComponent>();

            if (focus->focused)
            {
                auto text = entity->get<TextInputComponent>();

                text->text += event.text;

                if (entity->entity->has<SentenceText>())
                {
                    entity->entity->get<SentenceText>()->setText(text->text);
                }

                if (entity->entity->has<TTFText>())
                {
                    entity->entity->get<TTFText>()->setText(text->text);
                }

                ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, entity->entityId});

                LOG_INFO(DOM, "Current Text: " << text->text);
            }
        }
    }

    void TextInputSystem::onEvent(const OnSDLScanCode& event)
    {
        if (event.key != SDL_SCANCODE_RETURN and event.key != SDL_SCANCODE_BACKSPACE)
            return;

        for (const auto& entity : viewGroup<TextInputComponent, FocusableComponent>())
        {
            auto focus = entity->get<FocusableComponent>();

            if (focus->focused)
            {
                switch (event.key)
                {
                    case SDL_SCANCODE_RETURN:
                    {
                        auto text = entity->get<TextInputComponent>();

                        text->returnText = text->text;

                        if (text->clearTextAfterEnter)
                            text->text = "";

                        auto event = text->event;

                        event.values["return"] = text->returnText;

                        ecsRef->sendEvent(event);

                        if (entity->entity->has<SentenceText>())
                        {
                            entity->entity->get<SentenceText>()->setText(text->text);
                        }

                        if (entity->entity->has<TTFText>())
                        {
                            entity->entity->get<TTFText>()->setText(text->text);
                        }

                        ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, entity->entityId});
                    }
                    break;

                    case SDL_SCANCODE_BACKSPACE:
                    {
                        auto text = entity->get<TextInputComponent>();

                        // Remove the last character
                        if (not text->text.empty())
                            text->text.pop_back();

                        // If control is held try to remove everything till the beginning or till another space is found
                        if (inputHandler->isKeyPressed(SDL_SCANCODE_LCTRL) or inputHandler->isKeyPressed(SDL_SCANCODE_RCTRL))
                        {
                            while (not text->text.empty() and not (text->text.back() == ' '))
                            {
                                text->text.pop_back();
                            }
                        }

                        if (entity->entity->has<SentenceText>())
                        {
                            entity->entity->get<SentenceText>()->setText(text->text);
                        }

                        if (entity->entity->has<TTFText>())
                        {
                            entity->entity->get<TTFText>()->setText(text->text);
                        }

                        ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, entity->entityId});
                    }
                    break;

                    default:
                    {
                        LOG_ERROR(DOM, "Received unknown scan code: " << event.key);
                    }
                }

            }
        }
    }

    void TextInputSystem::execute()
    {

    }

}