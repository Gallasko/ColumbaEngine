#include "typespecimen.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include "ECS/entitysystem.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/sdlevents.h"
#include "2D/simple2dobject.h"
#include "UI/sizer.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        const std::string DEFAULT_SAMPLE = "The quick brown fox jumps over the lazy dog";

        // A middle dot (U+00B7) in the shared charset, written as UTF-8 so the source stays ASCII.
        const std::string MIDDLE_DOT = "\xC2\xB7";

        std::string metricsLine(const TextStyle& s)
        {
            return std::to_string(s.sizePx) + "/" + std::to_string(s.lineHeightPx) + " " + MIDDLE_DOT + " " + std::to_string(s.weight);
        }

        std::string trackingLine(const TextStyle& s)
        {
            // One decimal, e.g. "+1.0 tracking".
            const int tenths = static_cast<int>(s.letterSpacingPx * 10.0f + 0.5f);
            return "+" + std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + " tracking";
        }
    }

    void TypeSpecimen::init()
    {
        // Sample text per style, read from the tokens' type section (else a default).
        std::unordered_map<std::string, std::string> samples;
        const nlohmann::json& type = tokens->typeSection();
        if (type.contains("groups"))
        {
            for (const auto& group : type["groups"])
                for (const auto& st : group["styles"])
                    if (st.contains("name") and st.contains("sample") and st["sample"].is_string())
                        samples[st["name"].get<std::string>()] = st["sample"].get<std::string>();
        }

        // Vellum background behind everything.
        auto bg = makeUiSimple2DShape(ecsRef, Shape2D::Square, screenWidth, screenHeight, tokens->colour("vellum"));
        bg.get<PositionComponent>()->setX(0.0f);
        bg.get<PositionComponent>()->setY(0.0f);
        bg.get<PositionComponent>()->setZ(0.0f);
        backgroundId = bg.entity.id;

        // Records a text entity for repainting and returns its CompList.
        auto paint = [this](const std::string& style, const std::string& text, const std::string& token)
        {
            auto comp = styles->makeText(ecsRef, style, text, tokens->colour(token));
            paintedTexts.emplace_back(comp.entity.id, token);
            return comp;
        };

        // One row per style, stacked in a vertical layout inside the page margin.
        const float margin = tokens->space(7);
        auto column = makeVerticalLayout(ecsRef, margin, margin, 1000.0f, screenHeight - 2.0f * margin, false);
        column.get<PositionComponent>()->setZ(10.0f);
        auto layout = column.get<VerticalLayout>();
        layout->spacing = tokens->space(3);

        for (const auto& style : styles->all())
        {
            auto row = makeHorizontalLayout(ecsRef, 0.0f, 0.0f, 1000.0f, static_cast<float>(style.lineHeightPx), false);
            auto rowLayout = row.get<HorizontalLayout>();
            rowLayout->spacing = tokens->space(4);

            // Style name in a fixed-width column.
            auto name = paint("caption", style.name, "ink-muted");
            name.get<PositionComponent>()->setWidth(140.0f);
            rowLayout->addEntity(name);

            // The sample, in the style itself; versal and chapter are shown as they are used.
            const std::string sampleText = samples.count(style.name) ? samples[style.name] : DEFAULT_SAMPLE;
            const std::string sampleColour = (style.name == "versal" or style.name == "chapter") ? "vermilion" : "ink";
            rowLayout->addEntity(paint(style.name, sampleText, sampleColour));

            // Metrics, then tracking when non-zero.
            rowLayout->addEntity(paint("caption", metricsLine(style), "ink-faint"));
            if (style.letterSpacingPx != 0.0f)
                rowLayout->addEntity(paint("caption", trackingLine(style), "ink-faint"));

            layout->addEntity(row);
        }

        // Footer hint.
        auto hint = paint("caption", "T  toggle theme     S  swatches", "ink-muted");
        hint.get<PositionComponent>()->setX(margin);
        hint.get<PositionComponent>()->setY(screenHeight - margin);
        hint.get<PositionComponent>()->setZ(10.0f);

        // T: switch theme, announce it, and repaint every owned ref.
        listenToEvent<OnSDLScanCode>([this](const OnSDLScanCode& event)
        {
            if (event.key == SDL_SCANCODE_T)
            {
                const Theme next = tokens->theme() == Theme::Day ? Theme::Candle : Theme::Day;
                tokens->setTheme(next);
                ecsRef->sendEvent(ThemeChangedEvent{next});
            }
            else if (event.key == SDL_SCANCODE_S)
            {
                toggleSwatches();
            }
        });

        // Repaint on a theme change - the pattern every component follows.
        listenToEvent<ThemeChangedEvent>([this](const ThemeChangedEvent&)
        {
            repaint();
        });

        // Keep the background covering the window.
        listenToEvent<ResizeEvent>([this](const ResizeEvent& event)
        {
            screenWidth = event.width;
            screenHeight = event.height;

            auto entity = ecsRef->getEntity(backgroundId);
            if (entity and entity->has<PositionComponent>())
            {
                entity->get<PositionComponent>()->setWidth(screenWidth);
                entity->get<PositionComponent>()->setHeight(screenHeight);
            }
        });
    }

    void TypeSpecimen::repaint()
    {
        auto background = ecsRef->getEntity(backgroundId);
        if (background and background->has<Simple2DObject>())
            background->get<Simple2DObject>()->setColors(tokens->colour("vellum"));

        for (const auto& [id, token] : paintedTexts)
        {
            auto entity = ecsRef->getEntity(id);
            if (entity and entity->has<TTFText>())
                entity->get<TTFText>()->setColors(tokens->colour(token));
        }

        // Rebuild the swatch column so its colours follow the theme too.
        if (swatchesShown)
        {
            toggleSwatches();
            toggleSwatches();
        }
    }

    void TypeSpecimen::toggleSwatches()
    {
        if (swatchesShown)
        {
            for (auto id : swatchIds)
                ecsRef->removeEntity(id);
            swatchIds.clear();
            swatchesShown = false;
            return;
        }

        const float x = 760.0f;
        float y = tokens->space(7);
        const float swatchW = 48.0f;
        const float swatchH = 24.0f;

        for (const auto& colourName : tokens->colourNames())
        {
            auto leaf = makeUiSimple2DShape(ecsRef, Shape2D::Square, swatchW, swatchH, tokens->colour(colourName));
            leaf.get<PositionComponent>()->setX(x);
            leaf.get<PositionComponent>()->setY(y);
            leaf.get<PositionComponent>()->setZ(11.0f);
            swatchIds.push_back(leaf.entity.id);

            auto label = styles->makeText(ecsRef, "caption", colourName, tokens->colour("ink-muted"));
            label.get<PositionComponent>()->setX(x + swatchW + tokens->space(2));
            label.get<PositionComponent>()->setY(y + 4.0f);
            label.get<PositionComponent>()->setZ(11.0f);
            swatchIds.push_back(label.entity.id);

            y += swatchH + tokens->space(1);
        }

        swatchesShown = true;
    }
}
