#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include "2D/position.h"
#include "Renderer/renderer.h"

#include "Components/TTFText.generated.h"
#include "Components/ViewportComponent.generated.h"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace pg
{
    struct TTFTextSystem : public AbstractRenderer, System<Own<TTFText>, Ref<PositionComponent>,
        Listener<PositionComponentChangedEvent>, Listener<TTFTextChangedEvent>, Listener<ViewportComponentChangedEvent>, InitSys>
    {
        struct Character
        {
            glm::ivec2   size;       // Size of glyph
            glm::ivec2   bearing;    // Offset from baseline to left/top of glyph
            unsigned int advance;    // Offset to advance to next glyph
            glm::vec2    uvTopLeft;
            glm::vec2    uvBottomRight;
        };

        // Position-independent glyph data, relative to the PositionComponent origin.
        // Rebuilt only when text content changes; reused for position-only updates.
        struct GlyphRenderData
        {
            float relX, relY;   // Position relative to PositionComponent (x, y)
            float w, h;         // Glyph size
            float a, r, g, b;   // Colors (alpha, red, green, blue)
            float uvX0, uvY0;   // UV top-left
            float uvX1, uvY1;   // UV bottom-right
            size_t materialId;
            size_t viewport;
        };

        TTFTextSystem(MasterRenderer *renderer);

        virtual std::string getSystemName() const override { return "TTFText System"; }

        virtual void init() override;

        virtual void onEvent(const PositionComponentChangedEvent& event) override;
        virtual void onEvent(const TTFTextChangedEvent& event) override;
        virtual void onEvent(const ViewportComponentChangedEvent& event) override;

        void registerFont(const std::string& fontPath, const std::string& fontName = "", int size = 48);

        virtual void execute() override;

        // Builds glyph layout templates from text content. Only called when text changes.
        std::vector<GlyphRenderData> buildGlyphTemplates(CompRef<PositionComponent> ui, CompRef<TTFText> obj, size_t viewport);

        // Produces RenderCalls by applying position data to pre-built glyph templates.
        std::vector<RenderCall> createRenderCall(CompRef<PositionComponent> ui, const std::vector<GlyphRenderData>& glyphs);

        // Fast-path: regenerates render calls from stored templates using only new position data.
        void applyPositionUpdate(_unique_id entityId, CompRef<PositionComponent> ui);

        // Use this material preset if a material is not specified when creating a ttf component !
        Material baseMaterialPreset;

        FT_Library ft;

        std::vector<std::string> loadedFont;

        std::unordered_map<std::string, std::unordered_map<char, Character>> charactersMap;

        std::unordered_map<_unique_id, std::vector<GlyphRenderData>> entityGlyphTemplates;
        std::unordered_map<_unique_id, std::vector<RenderCall>> entityRenderCalls;
        std::vector<_unique_id> entitiesInRenderGroup;
        std::unordered_set<_unique_id> textContentUpdateSet;  // Full rebuild needed (text changed)
        std::unordered_set<_unique_id> positionUpdateSet;     // Position-only update needed

    private:
        // Render call helpers
        float computeLineHeight(const std::string& text, const std::string& fontPath, float scale);
        float getGlyphAdvance(char c, const std::string& fontPath, float scale);
        float computeWordWidth(const std::string& word, const std::string& fontPath, float scale);

        std::vector<TTFText> parseFormattedText(const TTFText &original);

        size_t getMaterialId(const std::string& fontPath);

        // Used for memoizing the material id of a font
        std::map<std::string, size_t> currentLoadedMaterialId;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, ViewportComponent, TTFText> makeTTFText(Type *ecs, float x, float y, float z, const std::string& fontPath, const std::string& text, float scale = 1.0f, constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        LOG_THIS("TTFText System");
        // Todo add an error when trying to create a ttf with a non existing font

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<PositionComponent>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setZ(z);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        auto vp = ecs->template attach<ViewportComponent>(entity);

        auto sentence = ecs->template attach<TTFText>(entity, text, fontPath, scale, colors);

        ui->setWidth(sentence->textWidth);
        ui->setHeight(sentence->textHeight);

        return {entity, ui, anchor, vp, sentence};
    }
}