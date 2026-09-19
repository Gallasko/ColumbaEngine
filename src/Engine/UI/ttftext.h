#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <unordered_set>

#include "2D/position.h"
#include "Renderer/renderer.h"
#include "UI/fontatlas.h"

#include "Components/TTFText.generated.h"
#include "Components/ViewportComponent.generated.h"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace pg
{
    struct TextMetrics
    {
        float width = 0.0f;      // widest line
        float height = 0.0f;     // lineCount * (lineHeight + spacing)
        float lineHeight = 0.0f;
        float ascender = 0.0f;
        int lineCount = 1;
    };

    struct TTFTextSystem : public AbstractRenderer, System<Own<TTFText>, Ref<PositionComponent>,
        Listener<PositionSettledEvent>, Listener<TTFTextChangedEvent>, Listener<ViewportComponentChangedEvent>, InitSys>
    {
        // Position-independent glyph data, relative to the PositionComponent origin.
        // Rebuilt only when text content changes; reused for position-only updates.
        struct GlyphRenderData
        {
            float relX, relY;   // Position relative to PositionComponent (x, y)
            float w, h;         // Glyph size
            float a, r, g, b;   // Colors (alpha, red, green, blue), 0-1
            float uvX0, uvY0;   // UV top-left
            float uvX1, uvY1;   // UV bottom-right
            size_t materialId;
            size_t viewport;
        };

        TTFTextSystem(MasterRenderer *renderer);

        virtual std::string getSystemName() const override { return "TTFText System"; }

        virtual void init() override;

        virtual void onEvent(const PositionSettledEvent& event) override;
        virtual void onEvent(const TTFTextChangedEvent& event) override;
        virtual void onEvent(const ViewportComponentChangedEvent& event) override;

        void registerFont(const std::string& fontPath, const std::string& fontName = "", int size = 48);

        virtual void execute() override;

        /// Measures without creating an entity. maxWidth <= 0 disables wrapping. Markup (\n, \c{}) is honoured.
        TextMetrics measureText(const std::string& font, const std::string& text, float scale = 1.0f, float maxWidth = 0.0f, float spacing = 0.0f) const;

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

        std::unordered_map<std::string, FontAtlas> fonts;   // key = alias (or path when no alias)

        std::unordered_map<_unique_id, std::vector<GlyphRenderData>> entityGlyphTemplates;
        std::unordered_map<_unique_id, std::vector<RenderCall>> entityRenderCalls;
        std::vector<_unique_id> entitiesInRenderGroup;
        std::unordered_set<_unique_id> textContentUpdateSet;  // Full rebuild needed (text changed)
        std::unordered_set<_unique_id> positionUpdateSet;     // Position-only update needed

    private:
        // Emits one glyph placement: code point, pen-relative x/y, glyph record and colour.
        using GlyphEmitter = std::function<void(uint32_t, float, float, const GlyphInfo&, const constant::Vector4D&)>;

        // Single layout pass shared by measureText (counting) and buildGlyphTemplates (emitting),
        // so a measurement can never disagree with the drawing.
        TextMetrics layoutText(const FontAtlas& atlas, const std::vector<TTFText>& segments, float scale, float maxWidth, float spacing, const GlyphEmitter& emit) const;

        std::vector<TTFText> parseFormattedText(const TTFText &original) const;

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
