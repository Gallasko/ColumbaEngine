#include "sentencesystem.h"

#include "Helpers/openglobject.h"
#include "Files/fileparser.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "Sentence System";

        constexpr unsigned int NBATTRIBUTES = 22;
    }

    template <>
    void serialize(Archive& archive, const SentenceText& value)
    {
        archive.startSerialization(SentenceText::getType());

        //TODO put all the variable to be serialized
        serialize(archive, "text", value.text);
        serialize(archive, "scale", value.scale);
        serialize(archive, "mainColor", value.mainColor);
        serialize(archive, "outline1", value.outline1);
        serialize(archive, "outline2", value.outline2);

        archive.endSerialization();
    }

    template <>
    SentenceText deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Sentence Text");

            SentenceText data;

            data.text = deserialize<std::string>(serializedString["text"]);
            data.scale = deserialize<float>(serializedString["scale"]);
            data.mainColor = deserialize<constant::Vector4D>(serializedString["mainColor"]);
            data.outline1 = deserialize<constant::Vector4D>(serializedString["outline1"]);
            data.outline2 = deserialize<constant::Vector4D>(serializedString["outline2"]);

            return data;
        }

        return SentenceText{};
    }

    SentenceSystem::SentenceSystem(MasterRenderer *renderer, const std::string& fontPath) : AbstractRenderer(renderer, RenderStage::Render),
        // Todo fix this
        // font(fontPath, std::unordered_map<std::string, ParserCallback> {
        //     {"Opacity 1", [this](FileParser& parser, const std::string&){ LOG_INFO(DOM, "0"); opacity[0] = std::stoi(parser.getNextLine());}},
        //     {"Opacity 2", [this](FileParser& parser, const std::string&){ LOG_INFO(DOM, "1"); opacity[1] = std::stoi(parser.getNextLine());}},
        //     {"Opacity 3", [this](FileParser& parser, const std::string&){ LOG_INFO(DOM, "2"); opacity[2] = std::stoi(parser.getNextLine());}}})
        font(fontPath)
    {
        LOG_INFO(DOM, opacity[0]);
        LOG_INFO(DOM, opacity[1]);
        LOG_INFO(DOM, opacity[2]);
    }

    void SentenceSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        Material simpleShapeMaterial;

        simpleShapeMaterial.shader = masterRenderer->getShader("text");

        simpleShapeMaterial.nbTextures = 1;

        simpleShapeMaterial.nbAttributes = NBATTRIBUTES;

        simpleShapeMaterial.textureId[0] = masterRenderer->getTexture("font").id;

        simpleShapeMaterial.uniformMap.emplace("sWidth", "ScreenWidth");
        simpleShapeMaterial.uniformMap.emplace("sHeight", "ScreenHeight");

        simpleShapeMaterial.uniformMap.emplace("tWidth", static_cast<float>(font.getAtlasWidth()));
        simpleShapeMaterial.uniformMap.emplace("tHeight", static_cast<float>(font.getAtlasHeight()));

        simpleShapeMaterial.uniformMap.emplace("time", static_cast<int>(0 % 314159));

        simpleShapeMaterial.mesh = std::make_shared<SimpleSquareMesh>(std::vector<size_t>{3, 4, 2, 4, 4, 4, 1});

        materialId = masterRenderer->registerMaterial(simpleShapeMaterial);

        auto group = registerGroup<UiComponent, SentenceText>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - sent group !");

            auto ui = entity->get<UiComponent>();
            auto sentence = entity->get<SentenceText>();

            ecsRef->attachGeneric<SentenceRenderCall>(entity, createRenderCall(ui, sentence));

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - sent group !");

            auto entity = ecsRef->getEntity(id);

            ecsRef->detach<SentenceRenderCall>(entity);

            changed = true;
        });
    }

    void SentenceSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void SentenceSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(entityId);

        if (not entity or not entity->has<UiComponent>() or not entity->has<SentenceRenderCall>())
            return;

        auto ui = entity->get<UiComponent>();
        auto shape = entity->get<SentenceText>();

        entity->get<SentenceRenderCall>()->call = createRenderCall(ui, shape);

        changed = true;
    }

    void SentenceSystem::execute()
    {
        if (not changed)
            return;

        renderCallList.clear();

        const auto& renderCallView = view<SentenceRenderCall>();

        renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            renderCallList.push_back(renderCall->call);
        }

        finishChanges();
    }

    RenderCall SentenceSystem::createRenderCall(CompRef<UiComponent> ui, CompRef<SentenceText> obj)
    {
        LOG_THIS_MEMBER(DOM);

        SentenceText *sent = obj;

        generateLetters(sent, font);

        RenderCall call;

        call.processUiComponent(ui);

        call.setOpacity(OpacityType::Additive);

        call.setRenderStage(renderStage);

        call.setMaterial(materialId);

        call.data.resize(obj->letters.size() * NBATTRIBUTES);

        float currentX = 0.0f;
        float outO = 0.0f;    //Outline Offset

        float x = ui->pos.x;
        float y = ui->pos.y;
        float z = ui->pos.z;

        size_t i = 0;

        for (const auto& letter : obj->letters)
        {
            auto fontInfo = letter.font;

            const auto& textureLimit = fontInfo->getTextureLimit();

            auto w = fontInfo->getWidth()  * obj->scale;
            auto h = fontInfo->getHeight() * obj->scale;
            auto o = fontInfo->getOffset() * obj->scale;

            outO = 0.0f;

            if (obj->outline1.w == 0.0f)
                outO += 1.0 * obj->scale;

            if (obj->outline2.w == 0.0f)
                outO += 1.0 * obj->scale;

            // World pos (x, y, z) 3 floats
            call.data[i * NBATTRIBUTES + 0] = x + currentX + outO;
            call.data[i * NBATTRIBUTES + 1] = y + o + outO;
            call.data[i * NBATTRIBUTES + 2] = z;
            // Texture coord (xmin, ymin, xmax, ymax) 4 floats
            call.data[i * NBATTRIBUTES + 3] = textureLimit.x;
            call.data[i * NBATTRIBUTES + 4] = textureLimit.y;
            call.data[i * NBATTRIBUTES + 5] = textureLimit.z;
            call.data[i * NBATTRIBUTES + 6] = textureLimit.w;
            // Size (w, h) 2 floats
            call.data[i * NBATTRIBUTES + 7] = w - outO;
            call.data[i * NBATTRIBUTES + 8] = h - outO;
            // Color 1 (r, g, b, a) 4 floats
            call.data[i * NBATTRIBUTES + 9] = obj->mainColor.x;
            call.data[i * NBATTRIBUTES + 10] = obj->mainColor.y;
            call.data[i * NBATTRIBUTES + 11] = obj->mainColor.z;
            call.data[i * NBATTRIBUTES + 12] = obj->mainColor.w;
            // Color 2 (r, g, b, a) 4 floats
            call.data[i * NBATTRIBUTES + 13] = obj->outline1.x;
            call.data[i * NBATTRIBUTES + 14] = obj->outline1.y;
            call.data[i * NBATTRIBUTES + 15] = obj->outline1.z;
            call.data[i * NBATTRIBUTES + 16] = obj->outline1.w;
            // Color 3 (r, g, b, a) 4 floats
            call.data[i * NBATTRIBUTES + 17] = obj->outline2.x;
            call.data[i * NBATTRIBUTES + 18] = obj->outline2.y;
            call.data[i * NBATTRIBUTES + 19] = obj->outline2.z;
            call.data[i * NBATTRIBUTES + 20] = obj->outline2.w;
            // Effect 1 float
            call.data[i * NBATTRIBUTES + 21] = static_cast<float>(obj->effect);

            i++;
            currentX += w - (2.0 * outO) + 1;
        }

        return call;
    }

    void SentenceSystem::generateLetters(SentenceText* sentence, const LoadedAtlas& font)
    {
        LOG_THIS(DOM);

        if (sentence == nullptr)
        {
            LOG_INFO("Meshbuilder", "Empty");
        }

        LOG_MILE("MeshBuilder", "Creating a new sentence texture mesh: " << sentence->text);

        if (font.isEmpty())
        {
            LOG_ERROR("MeshBuilder", "No font loaded");
            return;
        }

        auto nbChara = sentence->text.length();

        float currentX = 0.0f;
        // float outO = 0.0f;    //Outline Offset

        sentence->letters.clear();

        for (size_t i = 0; i < nbChara; i++)
        {
            // Todo fix this for emscripten
            const auto& letter = font.getTexture(std::string(1, sentence->text.at(i)));

            sentence->letters.emplace_back(&letter);

            // outO = 0.0f;

            auto w = letter.getWidth() * sentence->scale;
            auto h = letter.getHeight() * sentence->scale;
            auto o = letter.getOffset() * sentence->scale;

            // if (sentence->outline1.w == 0.0f)
            //     outO += 1.0 * sentence->scale;

            // if (sentence->outline2.w == 0.0f)
            //     outO += 1.0 * sentence->scale;

            // Todo maybe fix this
            currentX += w + 1;

            if (h + o > sentence->textHeight)
                sentence->textHeight = h + o;

        }

        sentence->textWidth = currentX;
    }
}