#include "stdafx.h"

#include "textinput.h"

#include "sentencesystem.h"
#include "ttftext.h"

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

                // Insert text at cursor position instead of appending
                text->text.insert(text->cursorPos, event.text);
                text->cursorPos += event.text.size();

                if (entity->entity->has<TTFText>())
                {
                    entity->entity->get<TTFText>()->setText(text->text);
                }

                updateCursorVisual(entity->entity, text);

                ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, entity->entityId});

                LOG_INFO(DOM, "Current Text: " << text->text);
            }
        }
    }

    void TextInputSystem::onEvent(const OnSDLScanCode& event)
    {
        if (event.key != SDL_SCANCODE_RETURN and event.key != SDL_SCANCODE_BACKSPACE
            and event.key != SDL_SCANCODE_LEFT and event.key != SDL_SCANCODE_RIGHT
            and event.key != SDL_SCANCODE_HOME and event.key != SDL_SCANCODE_END
            and event.key != SDL_SCANCODE_DELETE)
            return;

        for (const auto& entity : viewGroup<TextInputComponent, FocusableComponent>())
        {
            auto focus = entity->get<FocusableComponent>();

            if (focus->focused)
            {
                auto text = entity->get<TextInputComponent>();

                switch (event.key)
                {
                    case SDL_SCANCODE_RETURN:
                    {
                        text->returnText = text->text;

                        if (text->clearTextAfterEnter)
                        {
                            text->text = "";
                            text->cursorPos = 0;
                        }

                        auto ev = text->event;

                        ev.values["return"] = text->returnText;

                        ecsRef->sendEvent(ev);

                        if (entity->entity->has<TTFText>())
                        {
                            entity->entity->get<TTFText>()->setText(text->text);
                        }

                        updateCursorVisual(entity->entity, text);

                        ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, entity->entityId});
                    }
                    break;

                    case SDL_SCANCODE_BACKSPACE:
                    {
                        if (text->cursorPos > 0)
                        {
                            // If control is held, remove from cursor back to previous word boundary
                            if (inputHandler->isKeyPressed(SDL_SCANCODE_LCTRL) or inputHandler->isKeyPressed(SDL_SCANCODE_RCTRL))
                            {
                                size_t eraseEnd = text->cursorPos;

                                // Skip back past current position
                                text->cursorPos--;

                                // Skip back to word boundary
                                while (text->cursorPos > 0 and text->text[text->cursorPos - 1] != ' ')
                                {
                                    text->cursorPos--;
                                }

                                text->text.erase(text->cursorPos, eraseEnd - text->cursorPos);
                            }
                            else
                            {
                                text->cursorPos--;
                                text->text.erase(text->cursorPos, 1);
                            }

                            if (entity->entity->has<TTFText>())
                            {
                                entity->entity->get<TTFText>()->setText(text->text);
                            }

                            updateCursorVisual(entity->entity, text);

                            ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, entity->entityId});
                        }
                    }
                    break;

                    case SDL_SCANCODE_DELETE:
                    {
                        if (text->cursorPos < text->text.size())
                        {
                            if (inputHandler->isKeyPressed(SDL_SCANCODE_LCTRL) or inputHandler->isKeyPressed(SDL_SCANCODE_RCTRL))
                            {
                                size_t eraseStart = text->cursorPos;

                                // Skip forward to next word boundary
                                size_t eraseEnd = text->cursorPos;
                                while (eraseEnd < text->text.size() and text->text[eraseEnd] != ' ')
                                {
                                    eraseEnd++;
                                }

                                // Also consume the space after the word
                                if (eraseEnd < text->text.size() and text->text[eraseEnd] == ' ')
                                    eraseEnd++;

                                text->text.erase(eraseStart, eraseEnd - eraseStart);
                            }
                            else
                            {
                                text->text.erase(text->cursorPos, 1);
                            }

                            if (entity->entity->has<TTFText>())
                            {
                                entity->entity->get<TTFText>()->setText(text->text);
                            }

                            updateCursorVisual(entity->entity, text);

                            ecsRef->sendEvent(CurrentTextInputTextChanged{text->text, entity->entityId});
                        }
                    }
                    break;

                    case SDL_SCANCODE_LEFT:
                    {
                        if (text->cursorPos > 0)
                        {
                            if (inputHandler->isKeyPressed(SDL_SCANCODE_LCTRL) or inputHandler->isKeyPressed(SDL_SCANCODE_RCTRL))
                            {
                                // Jump to previous word boundary
                                text->cursorPos--;
                                while (text->cursorPos > 0 and text->text[text->cursorPos - 1] != ' ')
                                {
                                    text->cursorPos--;
                                }
                            }
                            else
                            {
                                text->cursorPos--;
                            }

                            updateCursorVisual(entity->entity, text);
                        }
                    }
                    break;

                    case SDL_SCANCODE_RIGHT:
                    {
                        if (text->cursorPos < text->text.size())
                        {
                            if (inputHandler->isKeyPressed(SDL_SCANCODE_LCTRL) or inputHandler->isKeyPressed(SDL_SCANCODE_RCTRL))
                            {
                                // Jump to next word boundary
                                text->cursorPos++;
                                while (text->cursorPos < text->text.size() and text->text[text->cursorPos] != ' ')
                                {
                                    text->cursorPos++;
                                }
                            }
                            else
                            {
                                text->cursorPos++;
                            }

                            updateCursorVisual(entity->entity, text);
                        }
                    }
                    break;

                    case SDL_SCANCODE_HOME:
                    {
                        text->cursorPos = 0;
                        updateCursorVisual(entity->entity, text);
                    }
                    break;

                    case SDL_SCANCODE_END:
                    {
                        text->cursorPos = text->text.size();
                        updateCursorVisual(entity->entity, text);
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

    void TextInputSystem::onEvent(const OnFocus& event)
    {
        for (const auto& entity : viewGroup<TextInputComponent, FocusableComponent>())
        {
            auto text = entity->get<TextInputComponent>();

            if (not text->cursorEntity)
                continue;

            auto cursorUi = text->cursorEntity->get<PositionComponent>();

            if (not cursorUi)
                continue;

            bool isFocused = (entity->entityId == event.id);

            cursorUi->setVisible(isFocused);
        }

        resetBlinkTimer();
    }

    void TextInputSystem::onEvent(const TickEvent& event)
    {
        // Accumulate time (TickEvent::tick is in milliseconds)
        blinkTimer += event.tick / 1000.0f;

        if (blinkTimer >= blinkInterval)
        {
            blinkTimer -= blinkInterval;
            cursorVisible = !cursorVisible;

            for (const auto& entity : viewGroup<TextInputComponent, FocusableComponent>())
            {
                auto focus = entity->get<FocusableComponent>();
                auto text = entity->get<TextInputComponent>();

                if (not focus->focused or not text->cursorEntity)
                    continue;

                auto cursorUi = text->cursorEntity->get<PositionComponent>();

                if (cursorUi)
                    cursorUi->setVisible(cursorVisible);
            }
        }
    }

    void TextInputSystem::resetBlinkTimer()
    {
        blinkTimer = 0.0f;
        cursorVisible = true;

        // Immediately make all focused cursors visible
        for (const auto& entity : viewGroup<TextInputComponent, FocusableComponent>())
        {
            auto focus = entity->get<FocusableComponent>();
            auto text = entity->get<TextInputComponent>();

            if (not focus->focused or not text->cursorEntity)
                continue;

            auto cursorUi = text->cursorEntity->get<PositionComponent>();

            if (cursorUi)
                cursorUi->setVisible(true);
        }
    }

    void TextInputSystem::updateCursorVisual(EntityRef entity, CompRef<TextInputComponent> textComp)
    {
        if (not textComp->cursorEntity)
            return;

        auto cursorAnchor = textComp->cursorEntity->get<UiAnchor>();

        if (not cursorAnchor)
            return;

        // Reset the blink timer so cursor stays visible after any interaction
        resetBlinkTimer();

        // Compute the X offset of the cursor by summing glyph advances up to cursorPos
        float cursorX = 0.0f;

        if (entity->has<TTFText>())
        {
            auto ttf = entity->get<TTFText>();
            auto* ttfSystem = ecsRef->getSystem<TTFTextSystem>();

            if (ttfSystem)
            {
                auto mapIt = ttfSystem->charactersMap.find(ttf->fontPath);
                if (mapIt != ttfSystem->charactersMap.end())
                {
                    const auto& fontChars = mapIt->second;
                    float scale = ttf->scale;

                    for (size_t i = 0; i < textComp->cursorPos and i < textComp->text.size(); i++)
                    {
                        char c = textComp->text[i];
                        auto it = fontChars.find(c);
                        if (it != fontChars.end())
                        {
                            cursorX += (it->second.advance >> 6) * scale;
                        }
                    }
                }
            }
        }

        cursorAnchor->setLeftMargin(cursorX);
    }

    void TextInputSystem::execute()
    {

    }

}
