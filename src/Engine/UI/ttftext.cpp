
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

        constexpr float ATLAS_WIDTH = 1024.0f;
        constexpr float ATLAS_HEIGHT = 1024.0f;
    }

    template <>
    void serialize(Archive& archive, const TTFText& value)
    {
        archive.startSerialization(TTFText::getType());

        serialize(archive, "text", value.text);
        serialize(archive, "scale", value.scale);
        serialize(archive, "colors", value.colors);
        serialize(archive, "fontPath", value.fontPath);
        serialize(archive, "wrap", value.wrap);

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
            data.fontPath = deserialize<std::string>(serializedString["fontPath"]);

            defaultDeserialize(serializedString, "scale", data.scale);
            defaultDeserialize(serializedString, "colors", data.colors);
            defaultDeserialize(serializedString, "wrap", data.wrap);

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

        baseMaterialPreset.setSimpleMesh({3, 2, 1, 1, 3, 1, 4});

        auto group = registerGroup<PositionComponent, TTFText>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - ttf group !");

            textUpdateQueue.push(entity->id);

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - ttf group !");

            auto entity = ecsRef->getEntity(id);

            if (entity->has<TTFTextCall>())
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
        auto f = [fontPath, size, this](size_t oldId)
        {
            // Initialize and load a font face.
            FT_Face face;
            if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
                LOG_ERROR("TTFText", "Failed to load font");
                return OpenGLTexture{};
            }

            FT_Set_Pixel_Sizes(face, 0, size);

            // glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

            // Define atlas size (for now, fixed).
            const int atlasWidth = ATLAS_WIDTH;
            const int atlasHeight = ATLAS_HEIGHT;
            std::vector<unsigned char> atlasBuffer(atlasWidth * atlasHeight, 0);

            // Simple rectangle packing initializations.
            int currentX = 0;
            int currentY = 0;
            int rowHeight = 0;

            // For each glyph in the chosen character set:
            for (unsigned char c = 32; c < 127; c++) {
                if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                    LOG_ERROR("TTFText", "Failed to load Glyph for: " << c);
                    continue;
                }

                // If the glyph won't fit in the current row, move to next row.
                if (currentX + face->glyph->bitmap.width > atlasWidth) {
                    currentX = 0;
                    currentY += rowHeight;
                    rowHeight = 0;
                }

                if (currentY + face->glyph->bitmap.rows > atlasHeight) {
                    LOG_ERROR("TTFText", "Atlas size exceeded!");
                    break;
                }

                // Copy glyph bitmap into the atlas buffer at position (currentX, currentY).
                for (size_t y = 0; y < face->glyph->bitmap.rows; y++) {
                    for (size_t x = 0; x < face->glyph->bitmap.width; x++) {
                        int atlasIndex = (currentY + y) * atlasWidth + (currentX + x);
                        atlasBuffer[atlasIndex] = face->glyph->bitmap.buffer[y * face->glyph->bitmap.width + x];
                    }
                }

                // Store glyph info including UVs in charactersMap.
                Character character;
                character.size = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows);
                character.bearing = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
                character.advance = face->glyph->advance.x;
                // Compute UV coordinates.
                float u1 = float(currentX) / atlasWidth;
                float v1 = float(currentY) / atlasHeight;
                float u2 = float(currentX + face->glyph->bitmap.width) / atlasWidth;
                float v2 = float(currentY + face->glyph->bitmap.rows) / atlasHeight;

                character.uvTopLeft = glm::vec2(u1, v1);
                character.uvBottomRight = glm::vec2(u2, v2);

                charactersMap[fontPath][c] = character;

                // Update currentX and rowHeight.
                currentX += face->glyph->bitmap.width;
                if (static_cast<int>(face->glyph->bitmap.rows) > rowHeight)
                    rowHeight = face->glyph->bitmap.rows;
            }

            // Now upload 'atlasBuffer' to OpenGL as a texture.
            unsigned int texture;
            if (oldId)
            {
                texture = oldId;
                glBindTexture(GL_TEXTURE_2D, texture);
            }
            else
            {
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
            }

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBuffer.data());
            // Set texture parameters.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Save atlasTexture in your renderer/material preset.
            // Free up the font face if necessary.
            FT_Done_Face(face);

            OpenGLTexture fontTexture;

            fontTexture.id = texture;
            fontTexture.transparent = true;

            return fontTexture;
        };

        auto textureName = "TTFText_" + fontPath;

        LOG_INFO(DOM, "Registering texture: " << textureName);

        masterRenderer->queueRegisterTexture(textureName, f);
    }

    void TTFTextSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(entityId);

        if (not entity or not entity->has<PositionComponent>() or not entity->has<TTFTextCall>())
            return;

        textUpdateQueue.push(entityId);

        changed = true;
    }

    void TTFTextSystem::execute()
    {
        if (not changed)
            return;

        renderCallList.clear();
        currentLoadedMaterialId.clear();

        const auto& renderCallView = view<TTFTextCall>();

        renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            renderCallList.insert(renderCallList.end(), renderCall->calls.begin(), renderCall->calls.end());
        }

        while (not textUpdateQueue.empty())
        {
            auto entityId = textUpdateQueue.front();

            auto entity = ecsRef->getEntity(entityId);

            if (not entity)
            {
                textUpdateQueue.pop();
                continue;
            }

            auto ui = entity->get<PositionComponent>();
            auto obj = entity->get<TTFText>();

            if (entity->has<TTFTextCall>())
            {
                entity->get<TTFTextCall>()->calls = createRenderCall(ui, obj);
            }
            else
            {
                ecsRef->attach<TTFTextCall>(entity, createRenderCall(ui, obj));
            }

            textUpdateQueue.pop();
        }

        changed = false;
    }

    // Helper: Computes the maximum line height based on the font's glyph heights.
    float TTFTextSystem::computeLineHeight(const std::string& text, const std::string& fontPath, float scale)
    {
        float lineHeight = 0.0f;

        for (char c : text)
        {
            Character ch = charactersMap[fontPath][c];
            float chHeight = ch.size.y * scale;

            if (chHeight > lineHeight)
                lineHeight = chHeight;
        }

        return lineHeight;
    }

    // Helper: Returns the advance (width) for a single glyph.
    float TTFTextSystem::getGlyphAdvance(char c, const std::string& fontPath, float scale)
    {
        Character ch = charactersMap[fontPath][c];

        // Right-shift advance by 6 to convert from 1/64 pixels to pixels.
        return (ch.advance >> 6) * scale;
    }

    // Helper: Computes the total width of a word.
    float TTFTextSystem::computeWordWidth(const std::string& word, const std::string& fontPath, float scale)
    {
        float width = 0.0f;

        for (char c : word)
        {
            width += getGlyphAdvance(c, fontPath, scale);
        }

        return width;
    }

    // Helper: Creates a RenderCall for a single glyph character.
    RenderCall TTFTextSystem::createGlyphRenderCall(CompRef<PositionComponent> ui, const std::string& fontPath, size_t materialId, char c, float currentX, float currentY, float z, float scale, float lineHeight, const constant::Vector4D &colors)
    {
        RenderCall call;
        call.processPositionComponent(ui);

        // Build a unique texture name for the glyph.
        Character ch = charactersMap[fontPath][c];

        float xPos = currentX + ch.bearing.x * scale;
        float yPos = currentY - ch.bearing.y * scale + lineHeight;
        float w = ch.size.x * scale;
        float h = ch.size.y * scale;

        call.setMaterial(materialId);

        call.setOpacity(OpacityType::Additive);
        call.setRenderStage(renderStage);

        // Resize data array and assign properties.
        call.data.resize(15);

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

        call.data[11] = ch.uvTopLeft.x;
        call.data[12] = ch.uvTopLeft.y;
        call.data[13] = ch.uvBottomRight.x;
        call.data[14] = ch.uvBottomRight.y;

        return call;
    }

    // Helper: Adds a space glyph RenderCall and advances the cursor.
    void TTFTextSystem::addSpaceRenderCall(std::vector<RenderCall>& calls, CompRef<PositionComponent> ui, const std::string& fontPath, size_t materialId, float& currentX, float& currentLineWidth, float currentY, float z, float scale, float lineHeight, const constant::Vector4D& colors)
    {
        char spaceChar = ' ';
        RenderCall spaceCall = createGlyphRenderCall(ui, fontPath, materialId, spaceChar, currentX, currentY, z, scale, lineHeight, colors);
        calls.push_back(spaceCall);
        float spaceAdvance = getGlyphAdvance(spaceChar, fontPath, scale);

        currentX += spaceAdvance;
        currentLineWidth += spaceAdvance;
    }

    std::vector<RenderCall> TTFTextSystem::createRenderCall(CompRef<PositionComponent> ui, CompRef<TTFText> obj)
    {
        if (obj->wrap)
            return createWrappedRenderCall(ui, obj);
        else
            return createNormalRenderCall(ui, obj);
    }

    std::vector<RenderCall> TTFTextSystem::createWrappedRenderCall(CompRef<PositionComponent> ui, CompRef<TTFText> obj)
    {
        std::vector<RenderCall> calls;

        float startX = ui->x;
        float startY = ui->y;
        float z = ui->z;
        float scale = obj->scale;
        auto colors = obj->colors;

        std::string text = obj->text;
        std::string fontPath = obj->fontPath;

        size_t materialId = getMaterialId(fontPath);

        // Compute line height and determine maximum allowed width.
        float lineHeight = computeLineHeight(text, fontPath, scale);
        float maxWidth = (ui->width > 0) ? ui->width : 10000.0f;

        // Initialize positions and dimensions.
        float currentX = startX;
        float currentY = startY;
        float currentLineWidth = 0.0f;
        float maxLineWidth = 0.0f;
        int lineCount = 1;
        bool firstWordOfLine = true;

        // Split the text by newline characters.
        std::istringstream textStream(text);
        std::vector<std::string> rawLines;
        std::string rawLine;

        while (std::getline(textStream, rawLine, '\n'))
        {
            rawLines.push_back(rawLine);
        }

        // Process each raw line separately.
        for (size_t i = 0; i < rawLines.size(); i++)
        {
            const std::string& line = rawLines[i];

            // If the raw line is empty, force a newline.
            if (line.empty())
            {
                currentY += lineHeight;
                lineCount++;
                currentX = startX;
                currentLineWidth = 0.0f;
                firstWordOfLine = true;
                continue;
            }

            // Wrap the current raw line.
            std::istringstream iss(line);
            std::string word;
            while (iss >> word)
            {
                float wordWidth = computeWordWidth(word, fontPath, scale);
                float spaceWidth = (not firstWordOfLine) ? getGlyphAdvance(' ', fontPath, scale) : 0.0f;

                // If adding this word would exceed the max width, wrap to a new line.
                if (currentLineWidth + spaceWidth + wordWidth > maxWidth)
                {
                    if (currentLineWidth > maxLineWidth)
                        maxLineWidth = currentLineWidth;

                    currentY += lineHeight;
                    currentX = startX;
                    currentLineWidth = 0.0f;
                    firstWordOfLine = true;
                    lineCount++;
                }

                // Insert a space if not the first word on the line.
                if (not firstWordOfLine)
                    addSpaceRenderCall(calls, ui, fontPath, materialId, currentX, currentLineWidth, currentY, z, scale, lineHeight, colors);

                // Render each character in the word.
                for (char c : word)
                {
                    RenderCall call = createGlyphRenderCall(ui, fontPath, materialId, c, currentX, currentY, z, scale, lineHeight, colors);
                    calls.push_back(call);
                    float advance = getGlyphAdvance(c, fontPath, scale);
                    currentX += advance;
                    currentLineWidth += advance;
                }

                firstWordOfLine = false;
            }

            // After processing a raw line, if there are more raw lines, force a newline.
            if (i < rawLines.size() - 1)
            {
                if (currentLineWidth > maxLineWidth)
                    maxLineWidth = currentLineWidth;

                currentY += lineHeight;
                currentX = startX;
                currentLineWidth = 0.0f;
                firstWordOfLine = true;
                lineCount++;
            }
        }

        if (currentLineWidth > maxLineWidth)
            maxLineWidth = currentLineWidth;

        float totalTextWidth = maxLineWidth;
        float totalTextHeight = lineCount * lineHeight;

        if (obj->textWidth != totalTextWidth)
        {
            obj->textWidth = totalTextWidth;
        }

        if (obj->textHeight != totalTextHeight)
        {
            obj->textHeight = totalTextHeight;
            ui->setHeight(totalTextHeight);
        }

        return calls;
    }

    // Todo add \n text splitting for normal ttf render call (currently only available for wrapped text rendering)
    std::vector<RenderCall> TTFTextSystem::createNormalRenderCall(CompRef<PositionComponent> ui, CompRef<TTFText> obj)
    {
        // Todo fix position issue right here !
        LOG_THIS_MEMBER(DOM);

        std::vector<RenderCall> calls;

        float x = ui->x;
        float y = ui->y;
        float z = ui->z;

        float scale = obj->scale;

        auto colors = obj->colors;

        std::string text = obj->text;
        std::string fontPath = obj->fontPath;

        size_t materialId = getMaterialId(fontPath);

        float textWidth = 0.0f;
        float textHeight = computeLineHeight(text, fontPath, scale);

        std::string::const_iterator c;
        for (c = text.begin(); c != text.end(); c++)
        {
            Character ch = charactersMap[obj->fontPath][*c];
            // textWidth += w;
            textWidth += (ch.advance >> 6) * scale;

            RenderCall call = createGlyphRenderCall(ui, fontPath, materialId, *c, x, y, z, scale, textHeight, colors);

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

    size_t TTFTextSystem::getMaterialId(const std::string& fontPath)
    {
        std::string textureName = "TTFText_" + fontPath;

        size_t materialId = 0;

        auto it = currentLoadedMaterialId.find(textureName);

        if (it != currentLoadedMaterialId.end())
        {
            materialId = it->second;
        }
        else
        {
            if (masterRenderer->hasMaterial(textureName))
                materialId = masterRenderer->getMaterialID(textureName);
            else
            {
                Material simpleShapeMaterial = baseMaterialPreset;
                simpleShapeMaterial.textureId[0] = masterRenderer->getTexture(textureName).id;
                materialId = masterRenderer->registerMaterial(textureName, simpleShapeMaterial);
            }

            currentLoadedMaterialId[textureName] = materialId;
        }

        return materialId;
    }
}