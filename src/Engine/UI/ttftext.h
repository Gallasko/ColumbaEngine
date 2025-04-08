#pragma once

#include <string>
#include <vector>

#include "2D/position.h"
#include "Renderer/renderer.h"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace pg
{
    struct TTFText : public Ctor
    {
        TTFText() {}
        TTFText(const std::string& text, const std::string& fontPath, float scale, constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f}) : text(text), fontPath(fontPath), scale(scale), colors(colors) {}

        inline static std::string getType() { return "TTFText"; }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity.ecsRef;

            entityId = entity.id;
        }

        void setText(const std::string& text)
        {
            this->text = text;

            if (ecsRef)
            {
                changed = true;
                ecsRef->sendEvent(EntityChangedEvent{entityId});
            }
        }

        void setColor(const constant::Vector4D& colors)
        {
            this->colors = colors;

            if (ecsRef)
            {
                changed = true;
                ecsRef->sendEvent(EntityChangedEvent{entityId});
            }
        }

        void setWrap(bool wrap)
        {
            if (this->wrap != wrap)
            {
                this->wrap = wrap;

                if (ecsRef)
                {
                    changed = true;
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
                }
            }
        }

        std::string text;

        float textWidth, textHeight;

        std::string fontPath;

        float scale = 1.0f;

        constant::Vector4D colors;

        bool wrap = false;

        EntitySystem * ecsRef = nullptr;

        _unique_id entityId = 0;

        bool changed = false;
    };

    struct TTFTextCall
    {
        TTFTextCall(const std::vector<RenderCall>& calls) : calls(calls) {}

        std::vector<RenderCall> calls;
    };

    template <>
    void serialize(Archive& archive, const TTFText& value);

    template <>
    TTFText deserialize(const UnserializedObject& serializedString);

    struct TTFTextResizeEvent
    {
        _unique_id id;
    };

    struct TTFTextSystem : public AbstractRenderer, System<Own<TTFText>, Own<TTFTextCall>, Ref<PositionComponent>, Listener<EntityChangedEvent>, InitSys>
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

        virtual void onEvent(const EntityChangedEvent& event) override;

        void registerFont(const std::string& fontPath, int size = 48);

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
        RenderCall createGlyphRenderCall(CompRef<PositionComponent> ui, const std::string& fontPath, size_t materialId, char c, float currentX, float currentY, float z, float scale, float lineHeight, const constant::Vector4D &colors);
        void addSpaceRenderCall(std::vector<RenderCall>& calls, CompRef<PositionComponent> ui, const std::string& fontPath, size_t materialId, float& currentX, float& currentLineWidth, float currentY, float z, float scale, float lineHeight, const constant::Vector4D& colors);

        std::vector<RenderCall> createWrappedRenderCall(CompRef<PositionComponent> ui, CompRef<TTFText> obj);
        std::vector<RenderCall> createNormalRenderCall(CompRef<PositionComponent> ui, CompRef<TTFText> obj);

        size_t getMaterialId(const std::string& fontPath);

        // Used for memoiszing the material id of a font
        std::map<std::string, size_t> currentLoadedMaterialId;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, TTFText> makeTTFText(Type *ecs, float x, float y, float z, const std::string& fontPath, const std::string& text, float scale = 1.0f, constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        LOG_THIS("TTFText System");

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