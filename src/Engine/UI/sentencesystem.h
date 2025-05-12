#pragma once

#include <string>
#include <vector>

#include "uisystem.h"
#include "Loaders/atlasloader.h"
#include "Renderer/renderer.h"
#include "constant.h"

#include "serialization.h"

namespace pg
{
    enum class SentenceEffect : uint8_t
    {
        NOEFFCT     = 0x00,
        TEXTWIGGLE  = 0x01,
        COLORSCROLL = 0x10,

        NUM_TYPES
    };

    struct Letter
    {
        Letter(const AtlasTexture * const font) : font(font) { }
        Letter(const Letter& rhs) : font(rhs.font) { }

        void operator=(const Letter& rhs)
        {
            font = rhs.font;
        }

        const AtlasTexture * font;
    };

    struct SentenceText : public Ctor
    {
        std::string text = "";
        float scale = 2.0f;
        constant::Vector4D mainColor = constant::Vector4D(255.0f, 255.0f, 255.0f, 255.0f);
        constant::Vector4D outline1  = constant::Vector4D(  0.0f,   0.0f,   0.0f, 205.0f);
        constant::Vector4D outline2  = constant::Vector4D(255.0f, 255.0f, 255.0f, 180.0f);

        SentenceEffect effect = SentenceEffect::NOEFFCT;

        UiSize textWidth, textHeight;

        std::vector<Letter> letters;

        _unique_id entityId = 0;

        EntitySystem *ecsRef = nullptr;

        inline static std::string getType() { return "SentenceText"; } 

        SentenceText() {}
        SentenceText(const SentenceText& other) : 
            text(other.text),
            scale(other.scale),
            mainColor(other.mainColor),
            outline1(other.outline1),
            outline2(other.outline2),
            effect(other.effect),
            textWidth(other.textWidth),
            textHeight(other.textHeight),
            entityId(other.entityId),
            ecsRef(other.ecsRef)
            {}

        SentenceText(const std::string& text) : text(text) {}
        SentenceText(const std::string& text, float scale, const constant::Vector4D& color1, const SentenceEffect& effect = SentenceEffect::NOEFFCT) : text(text), scale(scale), mainColor(color1), effect(effect) {}
        SentenceText(const std::string& text, float scale, const constant::Vector4D& color1, const constant::Vector4D& color2, const constant::Vector4D& color3 = constant::Vector4D(255.0f, 255.0f, 255.0f, 180.0f), const SentenceEffect& effect = SentenceEffect::NOEFFCT) : text(text), scale(scale), mainColor(color1), outline1(color2), outline2(color3), effect(effect) {}

        virtual void onCreation(EntityRef entity) override
        {
            ecsRef = entity->world();

            entityId = entity->id;
        }

        inline void operator=(const SentenceText &rhs)
        {
            this->text       = rhs.text;
            this->scale      = rhs.scale;
            this->mainColor  = rhs.mainColor;
            this->outline1   = rhs.outline1;
            this->outline2   = rhs.outline2;
            this->effect     = rhs.effect;
            this->textWidth  = rhs.textWidth;
            this->textHeight = rhs.textHeight;
            this->letters    = rhs.letters;
            this->entityId   = rhs.entityId;
            this->ecsRef     = rhs.ecsRef;
        }

        inline bool operator==(const SentenceText &rhs) const
        {
            return this->text == rhs.text && this->scale == rhs.scale && this->mainColor == rhs.mainColor && this->outline1 == rhs.outline1 && this->outline2 == rhs.outline2 && this->effect == rhs.effect;
        }

        inline bool operator!=(const SentenceText &rhs) const
        {
            return !(*this == rhs);
        }

        inline void setText(const std::string &text)
        {
            if (this->text != text)
            {
                this->text = text;

                if (ecsRef)
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
            }
        }

        inline std::string getText() const { return text; }

        inline void changeMainColor(const constant::Vector4D& color)
        {
            if (this->mainColor != color)
            {
                this->mainColor = color;

                if (ecsRef)
                    ecsRef->sendEvent(EntityChangedEvent{entityId});
            }
        }
    };

    template <>
    void serialize(Archive& archive, const SentenceText& value);

    template <>
    SentenceText deserialize(const UnserializedObject& serializedString);

    struct SentenceRenderCall
    {
        SentenceRenderCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    struct SentenceSystem : public AbstractRenderer, System<Own<SentenceText>, Own<SentenceRenderCall>, Ref<UiComponent>, Listener<EntityChangedEvent>, InitSys>
    {
        SentenceSystem(MasterRenderer *renderer, const std::string& fontPath);

        virtual std::string getSystemName() const override { return "Sentence System"; }

        virtual void init() override;

        virtual void onEvent(const EntityChangedEvent& event) override;

        void onEventUpdate(_unique_id entityId);

        virtual void execute() override;

        RenderCall createRenderCall(CompRef<UiComponent> ui, CompRef<SentenceText> obj);

        void generateLetters(SentenceText* sentence, const LoadedAtlas& font);

        int opacity[3] = { -1, -1, -1 };

        LoadedAtlas font;

        uint64_t materialId = 0;
    };

    /** Helper that create an entity with an Ui component and a Texture component */
    template <typename Type>
    CompList<UiComponent, SentenceText> makeSentence(Type *ecs, float x, float y, const SentenceText& text)
    {
        LOG_THIS("Sentence System");

        auto entity = ecs->createEntity();

        auto ui = ecs->template attach<UiComponent>(entity);

        ui->setX(x);
        ui->setY(y);

        auto sentence = ecs->template attach<SentenceText>(entity, text);

        ui->setWidth(&sentence->textWidth);
        ui->setHeight(&sentence->textHeight);

        return CompList<UiComponent, SentenceText>(entity, ui, sentence);
    }
}