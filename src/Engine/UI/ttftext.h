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

        std::string text;

        float textWidth, textHeight;

        std::string fontPath;

        float scale = 1.0f;

        constant::Vector4D colors;

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
            unsigned int textureID;  // ID handle of the glyph texture
            glm::ivec2   size;       // Size of glyph
            glm::ivec2   bearing;    // Offset from baseline to left/top of glyph
            unsigned int advance;    // Offset to advance to next glyph
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
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, TTFText> makeTTFText(Type *ecs, UiSize x, UiSize y, UiSize z, const std::string& fontPath, const std::string& text, float scale = 1.0f, constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f})
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