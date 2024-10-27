#pragma once

#include <string>
#include <vector>

#include "uisystem.h"
#include "Renderer/renderer.h"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace pg
{
    struct TTFSize
    {
        int width, height;
    };

    struct TTFText : public Ctor
    {
        TTFText() {}
        TTFText(const std::string& text, const std::string& fontPath, int textSize) : text(text), fontPath(fontPath), textSize(textSize) {}

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

        std::string text;

        UiSize textWidth, textHeight;

        std::string fontPath;

        constant::Vector4D colors = {255, 255, 255, 255};

        int textSize = 18;

        EntitySystem * ecsRef = nullptr;

        _unique_id entityId = 0;

        bool changed = false;
    };

    struct TTFTextCall
    {
        TTFTextCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    template <>
    void serialize(Archive& archive, const TTFText& value);

    template <>
    TTFText deserialize(const UnserializedObject& serializedString);

    struct TTFTextSystem : public AbstractRenderer, System<Own<TTFText>, Own<TTFTextCall>, Ref<UiComponent>, Listener<EntityChangedEvent>, NamedSystem, InitSys>
    {
        TTFTextSystem(MasterRenderer *renderer);

        virtual std::string getSystemName() const override { return "TTFText System"; }

        virtual void init() override;

        virtual void onEvent(const EntityChangedEvent& event) override;

        void onEventUpdate(_unique_id entityId);

        virtual void execute() override;

        RenderCall createRenderCall(CompRef<UiComponent> ui, CompRef<TTFText> obj);

        // Use this material preset if a material is not specified when creating a ttf component !
        Material baseMaterialPreset;

        /** Keep track of the size of a TTF texture */
        std::unordered_map<std::string, TTFSize> sizeMap;

        FT_Library ft;
    };

    template <typename Type>
    CompList<UiComponent, TTFText> makeTTFText(Type *ecs, float x, float y, const std::string& fontPath, const std::string& text, int textSize = 18)
    {
        LOG_THIS("TTFText System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<UiComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto sentence = ecs->template attach<TTFText>(entity, text, fontPath, textSize);

        ui->setWidth(&sentence->textWidth);
        ui->setHeight(&sentence->textHeight);

        return CompList<UiComponent, TTFText>(entity, ui, sentence);
    }
}