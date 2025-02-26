
#include "ttftext.h"

#ifdef __EMSCRIPTEN__
#define GL_GLEXT_PROTOTYPES 1
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengl.h>
// #include <SDL_opengl_glext.h>
#include <GLES2/gl2.h>
#include <GLFW/glfw3.h>
#else
#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>
#endif

#include <glm.hpp>

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "TTFText System";
    }

    template <>
    void serialize(Archive& archive, const TTFText& value)
    {
        archive.startSerialization(TTFText::getType());

        serialize(archive, "text", value.text);
        serialize(archive, "scale", value.scale);
        serialize(archive, "colors", value.colors);
        serialize(archive, "fontPath", value.fontPath);

        archive.endSerialization();
    }

    template <>
    TTFText deserialize(const UnserializedObject& serializedString)
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

            TTFText data;

            data.text = deserialize<std::string>(serializedString["text"]);
            data.scale = deserialize<float>(serializedString["scale"]);
            data.colors = deserialize<constant::Vector4D>(serializedString["colors"]);
            data.fontPath = deserialize<std::string>(serializedString["fontPath"]);

            return data;
        }

        return TTFText{};
    }


    TTFTextSystem::TTFTextSystem(MasterRenderer *renderer) : AbstractRenderer(renderer, RenderStage::Render)
    {
        if (FT_Init_FreeType(&ft))
        {
            LOG_ERROR(DOM, "ERROR::FREETYPE: Could not init FreeType Library");
        }
    }

    void TTFTextSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        baseMaterialPreset.shader = masterRenderer->getShader("ttfTexture");

        baseMaterialPreset.nbTextures = 1;

        baseMaterialPreset.uniformMap.emplace("sWidth", "ScreenWidth");
        baseMaterialPreset.uniformMap.emplace("sHeight", "ScreenHeight");

        baseMaterialPreset.setSimpleMesh({3, 2, 1, 1, 3, 1});

        auto group = registerGroup<PositionComponent, TTFText>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - ttf group !");

            auto ui = entity->get<PositionComponent>();
            auto sentence = entity->get<TTFText>();

            ecsRef->attach<TTFTextCall>(entity, createRenderCall(ui, sentence));

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - ttf group !");

            auto entity = ecsRef->getEntity(id);

            ecsRef->detach<TTFTextCall>(entity);

            changed = true;
        });
    }

    void TTFTextSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void TTFTextSystem::registerFont(const std::string& fontPath, int size)
    {
        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
        {
            LOG_ERROR(DOM, "Failed to load font");
            return;
        }
        
        // FT_Set_Char_Size(face, 0, size * 64, 300, 300);
        FT_Set_Pixel_Sizes(face, 0, size);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction
  
        for (unsigned char c = 0; c < 128; c++)
        {
            auto f = [fontPath, face, c, this](size_t oldId) {
                // load character glyph 
                if (FT_Load_Char(face, c, FT_LOAD_RENDER))
                {
                    LOG_ERROR(DOM, "Failed to load Glyph for: " << std::string(1, c));
                    return OpenGLTexture{};
                }

                // generate texture
                unsigned int texture;
                if (oldId)
                {
                    texture = oldId;
                    glBindTexture(GL_TEXTURE_2D, texture);
                }
                else
                {
                    glGenTextures(1, &texture);
                }
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RED,
                    face->glyph->bitmap.width,
                    face->glyph->bitmap.rows,
                    0,
                    GL_RED,
                    GL_UNSIGNED_BYTE,
                    face->glyph->bitmap.buffer
                );
                // set texture options
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // now store character for later use
                Character character = {
                    texture, 
                    glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                    glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                    static_cast<unsigned int>(face->glyph->advance.x)
                };

                LOG_MILE(DOM, "Character : " << c << " has id " << texture << " " << character.size.x << " " << character.size.y);

                charactersMap[fontPath].insert(std::pair<char, Character>(c, character));

                OpenGLTexture fontTexture;

                fontTexture.id = texture;
                fontTexture.transparent = true;

                return fontTexture;
            };

            auto textureName = "TTFText_" + fontPath + "_" + std::to_string(c);

            masterRenderer->queueRegisterTexture(textureName, f);

            // Todo free up the lib stuff once all the texture are registered ! (ft and face)
        }
    }

    void TTFTextSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(entityId);
        
        if (not entity or not entity->has<PositionComponent>() or not entity->has<TTFTextCall>())
            return; 

        auto ui = entity->get<PositionComponent>();
        auto shape = entity->get<TTFText>();

        entity->get<TTFTextCall>()->calls = createRenderCall(ui, shape);

        changed = true;
    }

    void TTFTextSystem::execute()
    {
        if (not changed)
            return;

        renderCallList.clear();

        // const auto& renderCallView = viewGroup<UiComponent, TTFText, TTFTextCall>();

        const auto& renderCallView = view<TTFTextCall>();

        renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            renderCallList.insert(renderCallList.end(), renderCall->calls.begin(), renderCall->calls.end());
        }
        
        changed = false;
    }

    std::vector<RenderCall> TTFTextSystem::createRenderCall(CompRef<PositionComponent> ui, CompRef<TTFText> obj)
    {
        // Todo fix position issue right here !
        LOG_THIS_MEMBER(DOM);

        std::vector<RenderCall> calls;

        auto textureName = "TTFText_" + obj->text + "_" + std::to_string(obj->scale);

        float x = ui->x;
        float y = ui->y;
        float z = ui->z;

        float scale = obj->scale;

        auto colors = obj->colors;

        float textWidth = 0.0f;
        float textHeight = 0.0f;

        std::string::const_iterator cha;
        for (cha = obj->text.begin(); cha != obj->text.end(); cha++)
        {
            Character ch = charactersMap[obj->fontPath][*cha];

            if (ch.size.y * scale > textHeight)
                textHeight = ch.size.y * scale;
        }

        std::string::const_iterator c;
        for (c = obj->text.begin(); c != obj->text.end(); c++)
        {
            auto textureName = "TTFText_" + obj->fontPath + "_" + std::to_string(*c);

            LOG_MILE(DOM, "Looking for texture: " << textureName);

            Character ch = charactersMap[obj->fontPath][*c];

            float xPos = x + ch.bearing.x * scale;
            // float yPos = y - (ch.size.y + ch.bearing.y) * scale / 2.0f;
            float yPos = y - ch.bearing.y * scale + textHeight;

            float w = ch.size.x * scale;
            float h = ch.size.y * scale;

            // textWidth += w;
            textWidth += (ch.advance >> 6) * scale;

            RenderCall call;

            call.processPositionComponent(ui);

            if (masterRenderer->hasMaterial(textureName))
            {
                call.setMaterial(masterRenderer->getMaterialID(textureName));
            }
            else
            {
                Material simpleShapeMaterial = baseMaterialPreset;

                simpleShapeMaterial.textureId[0] = masterRenderer->getTexture(textureName).id;

                call.setMaterial(masterRenderer->registerMaterial(textureName, simpleShapeMaterial));
            }

            call.setOpacity(OpacityType::Additive);

            call.setRenderStage(renderStage);

            call.data.resize(11);

            LOG_MILE(DOM, "Glyph: " << scale << " " << xPos << " " << yPos << " " << w << "," << h);

            call.data[0] = xPos;
            call.data[1] = yPos;
            call.data[2] = z;
            call.data[3] = w;
            call.data[4] = h;
            call.data[5] = ui->rotation;
            call.data[6] = colors.w;
            call.data[7] = colors.x;
            call.data[8] = colors.y;
            call.data[9] = colors.z;
            call.data[10] = 1.0f;

            calls.push_back(call);

            // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
            x += (ch.advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
            // x += 16; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
        }

        if (obj->textWidth != textWidth)
        {
            obj->textWidth = textWidth;
            ui->setWidth(textWidth);
        }

        if (obj->textHeight != textHeight)
        {
            obj->textHeight = textHeight;
            ui->setHeight(textHeight);
        }
        
        return calls;
    }
}