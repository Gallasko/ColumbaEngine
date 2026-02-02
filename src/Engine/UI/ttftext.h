#pragma once

#include <string>
#include <vector>

#include "2D/position.h"
#include "Renderer/renderer.h"

#include "Components/TTFText.generated.h"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace pg
{
    struct TTFTextCall
    {
        TTFTextCall(const std::vector<RenderCall>& calls) : calls(calls) {}

        std::vector<RenderCall> calls;
    };

    struct TTFTextSystem : public AbstractRenderer, System<Own<TTFText>, Own<TTFTextCall>, Ref<PositionComponent>,
        Listener<PositionComponentChangedEvent>, Listener<TTFTextChangedEvent>, InitSys>
    {
        struct Character
        {
            glm::ivec2   size;       // Size of glyph
            glm::ivec2   bearing;    // Offset from baseline to left/top of glyph
            unsigned int advance;    // Offset to advance to next glyph
            glm::vec2    uvTopLeft;
            glm::vec2    uvBottomRight;
        };

        TTFTextSystem(MasterRenderer *renderer);

        virtual std::string getSystemName() const override { return "TTFText System"; }

        virtual void init() override;

        virtual void onEvent(const PositionComponentChangedEvent& event) override;
        virtual void onEvent(const TTFTextChangedEvent& event) override;

        void registerFont(const std::string& fontPath, const std::string& fontName = "", int size = 48);

        void onEventUpdate(_unique_id entityId);

        virtual void execute() override;

        std::vector<RenderCall> createRenderCall(CompRef<PositionComponent> ui, CompRef<TTFText> obj);

        // Use this material preset if a material is not specified when creating a ttf component !
        Material baseMaterialPreset;

        FT_Library ft;

        std::vector<std::string> loadedFont;

        std::unordered_map<std::string, std::unordered_map<char, Character>> charactersMap;

        std::queue<_unique_id> textUpdateQueue;

    private:
        // Render call helpers
        float computeLineHeight(const std::string& text, const std::string& fontPath, float scale);
        float getGlyphAdvance(char c, const std::string& fontPath, float scale);
        float computeWordWidth(const std::string& word, const std::string& fontPath, float scale);
        RenderCall createGlyphRenderCall(CompRef<PositionComponent> ui, const std::string& fontPath, size_t materialId, char c, float currentX, float currentY, float z, float scale, float lineHeight, const constant::Vector4D &colors, size_t viewport);

        std::vector<TTFText> parseFormattedText(const TTFText &original);

        size_t getMaterialId(const std::string& fontPath);

        // Used for memoiszing the material id of a font
        std::map<std::string, size_t> currentLoadedMaterialId;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, TTFText> makeTTFText(Type *ecs, float x, float y, float z, const std::string& fontPath, const std::string& text, float scale = 1.0f, constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        LOG_THIS("TTFText System");
        // Todo add an error when trying to create a ttf with a non existing font

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setZ(z);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto sentence = ecs->template attach<TTFText>(entity, text, fontPath, scale, colors);

        ui->setWidth(sentence->textWidth);
        ui->setHeight(sentence->textHeight);

        return {entity, ui, anchor, sentence};
    }
}