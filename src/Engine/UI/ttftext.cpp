
#include "stdafx.h"

#include "ttftext.h"

#include "ECS/entitysystem.h"

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

namespace pg
{
    namespace
    {
        constexpr const char * const DOM = "TTFText System";

        constexpr float ATLAS_WIDTH = 1024.0f;
        constexpr float ATLAS_HEIGHT = 1024.0f;
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

        auto group = registerGroup<PositionComponent, TTFText, ViewportComponent>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - ttf group !");

            auto ui = entity->get<PositionComponent>();
            auto obj = entity->get<TTFText>();
            size_t viewport = entity->has<ViewportComponent>() ? entity->get<ViewportComponent>()->viewport : 0;

            if (ui and obj)
            {
                entityGlyphTemplates[entity->id] = buildGlyphTemplates(ui, obj, viewport);
                entityRenderCalls[entity->id] = createRenderCall(ui, entityGlyphTemplates[entity->id]);
                entitiesInRenderGroup.push_back(entity->id);
                std::sort(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end());
            }

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - ttf group !");

            entityRenderCalls.erase(id);
            entityGlyphTemplates.erase(id);
            entitiesInRenderGroup.erase(
                std::remove(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(), id),
                entitiesInRenderGroup.end()
            );

            changed = true;
        });
    }

    void TTFTextSystem::onEvent(const PositionComponentChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        // Only queue a position-only update if a full rebuild isn't already pending.
        if (textContentUpdateSet.find(event.id) == textContentUpdateSet.end())
            positionUpdateSet.insert(event.id);

        changed = true;
    }

    void TTFTextSystem::onEvent(const TTFTextChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        // Full rebuild needed; remove from position-only set to avoid redundant work.
        positionUpdateSet.erase(event.id);
        textContentUpdateSet.insert(event.id);

        changed = true;
    }

    void TTFTextSystem::onEvent(const ViewportComponentChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        // Viewport changed — needs full rebuild to update glyph viewport values.
        positionUpdateSet.erase(event.id);
        textContentUpdateSet.insert(event.id);

        changed = true;
    }

    void TTFTextSystem::registerFont(const std::string& fontPath, const std::string& fontName, int size)
    {
        auto f = [fontPath, fontName, size, this](size_t oldId)
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

            auto texName = (fontName == "" ? fontPath : fontName);

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

                charactersMap[texName][c] = character;

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

            // glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBuffer.data());
#ifdef __EMSCRIPTEN__
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, atlasWidth, atlasHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, atlasBuffer.data());
#else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBuffer.data());
#endif
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

        auto textureName = "TTFText_" + (fontName == "" ? fontPath : fontName);

        LOG_INFO(DOM, "Registering texture: " << textureName);

        masterRenderer->queueRegisterTexture(textureName, f);
    }

    void TTFTextSystem::execute()
    {
        if (not changed)
            return;

        // Snapshot and clear textContentUpdateSet, intersect with render group.
        std::vector<_unique_id> fullRebuildQueue;
        {
            std::vector<_unique_id> temp;
            temp.assign(textContentUpdateSet.begin(), textContentUpdateSet.end());
            std::sort(temp.begin(), temp.end());
            textContentUpdateSet.clear();

            std::set_intersection(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(),
                                  temp.begin(), temp.end(),
                                  std::back_inserter(fullRebuildQueue));
        }

        // Snapshot and clear positionUpdateSet, intersect with render group,
        // then exclude entities already scheduled for a full rebuild.
        std::vector<_unique_id> positionOnlyQueue;
        {
            std::vector<_unique_id> temp;
            temp.assign(positionUpdateSet.begin(), positionUpdateSet.end());
            std::sort(temp.begin(), temp.end());
            positionUpdateSet.clear();

            std::vector<_unique_id> inGroup;
            std::set_intersection(entitiesInRenderGroup.begin(), entitiesInRenderGroup.end(),
                                  temp.begin(), temp.end(),
                                  std::back_inserter(inGroup));

            std::set_difference(inGroup.begin(), inGroup.end(),
                                fullRebuildQueue.begin(), fullRebuildQueue.end(),
                                std::back_inserter(positionOnlyQueue));
        }

        // Full rebuild: text content changed — redo glyph layout and render calls.
        for (const auto& entityId : fullRebuildQueue)
        {
            auto entity = ecsRef->getEntity(entityId);
            if (not entity)
                continue;

            auto ui = entity->get<PositionComponent>();
            auto obj = entity->get<TTFText>();
            size_t viewport = entity->has<ViewportComponent>() ? entity->get<ViewportComponent>()->viewport : 0;

            LOG_MILE(DOM, "Full rebuild entity " << entityId << ", with text: " << obj->text);

            entityGlyphTemplates[entityId] = buildGlyphTemplates(ui, obj, viewport);
            entityRenderCalls[entityId] = createRenderCall(ui, entityGlyphTemplates[entityId]);
        }

        // Fast-path: position only changed — reuse glyph templates, apply new position.
        for (const auto& entityId : positionOnlyQueue)
        {
            auto entity = ecsRef->getEntity(entityId);
            if (not entity)
                continue;

            auto ui = entity->get<PositionComponent>();

            LOG_MILE(DOM, "Position update entity " << entityId);

            applyPositionUpdate(entityId, ui);
        }

        // Rebuild renderCallList from the system map.
        renderCallList.clear();

        size_t totalCalls = 0;
        for (const auto& [entityId, calls] : entityRenderCalls)
            totalCalls += calls.size();
        renderCallList.reserve(totalCalls);

        for (const auto& [entityId, calls] : entityRenderCalls)
            renderCallList.insert(renderCallList.end(), calls.begin(), calls.end());

        finishChanges();
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

    std::vector<TTFTextSystem::GlyphRenderData> TTFTextSystem::buildGlyphTemplates(CompRef<PositionComponent> ui, CompRef<TTFText> obj, size_t viewport)
    {
        std::vector<GlyphRenderData> glyphs;

        std::vector<TTFText> segments = parseFormattedText(*obj);

        float startX = ui->x;
        float startY = ui->y;
        float scale = obj->scale;
        bool wrap = obj->wrap;
        std::string fontPath = obj->fontPath;
        size_t materialId = getMaterialId(fontPath);

        float lineHeight = computeLineHeight(obj->text, fontPath, scale) + obj->spacing;
        float maxWidth = (ui->width > 0) ? ui->width : 10000.0f;

        float currentX = startX;
        float currentY = startY;

        for (const auto& seg : segments)
        {
            if (seg.text == "\n")
            {
                currentY += lineHeight;
                currentX = startX;
                continue;
            }

            for (size_t charIndex = 0; charIndex < seg.text.length(); charIndex++)
            {
                char c = seg.text[charIndex];

                if (c == ' ')
                {
                    currentX += getGlyphAdvance(' ', fontPath, scale);
                }
                else
                {
                    if (charIndex == 0 || seg.text[charIndex - 1] == ' ')
                    {
                        std::string currentWord;
                        size_t wordEnd = charIndex;
                        while (wordEnd < seg.text.length() && seg.text[wordEnd] != ' ')
                        {
                            currentWord += seg.text[wordEnd];
                            wordEnd++;
                        }

                        float wordWidth = computeWordWidth(currentWord, fontPath, scale);

                        if (wrap && (currentX - startX + wordWidth > maxWidth))
                        {
                            currentY += lineHeight;
                            currentX = startX;
                        }
                    }

                    Character ch = charactersMap[fontPath][c];

                    GlyphRenderData glyph;
                    glyph.relX = (currentX - startX) + ch.bearing.x * scale;
                    glyph.relY = (currentY - startY) - ch.bearing.y * scale + lineHeight;
                    glyph.w = ch.size.x * scale;
                    glyph.h = ch.size.y * scale;
                    glyph.a = seg.colors.w;
                    glyph.r = seg.colors.x;
                    glyph.g = seg.colors.y;
                    glyph.b = seg.colors.z;
                    glyph.uvX0 = ch.uvTopLeft.x;
                    glyph.uvY0 = ch.uvTopLeft.y;
                    glyph.uvX1 = ch.uvBottomRight.x;
                    glyph.uvY1 = ch.uvBottomRight.y;
                    glyph.materialId = materialId;
                    glyph.viewport = viewport;

                    glyphs.push_back(glyph);
                    currentX += getGlyphAdvance(c, fontPath, scale);
                }
            }
        }

        float totalWidth = currentX - startX;
        float totalHeight = (currentY - startY) + lineHeight;

        if (areNotAlmostEqual(obj->textWidth, totalWidth))
        {
            obj->textWidth = totalWidth;
            ui->setWidth(totalWidth);
        }

        if (areNotAlmostEqual(obj->textHeight, totalHeight))
        {
            obj->textHeight = totalHeight;
            ui->setHeight(totalHeight);
        }

        return glyphs;
    }

    std::vector<RenderCall> TTFTextSystem::createRenderCall(CompRef<PositionComponent> ui, const std::vector<GlyphRenderData>& glyphs)
    {
        std::vector<RenderCall> calls;
        calls.reserve(glyphs.size());

        for (const auto& glyph : glyphs)
        {
            RenderCall call;
            call.processPositionComponent(ui);

            call.setMaterial(glyph.materialId);
            call.setOpacity(OpacityType::Additive);
            call.setRenderStage(renderStage);
            call.setViewport(glyph.viewport);

            call.data.resize(15);
            call.data[0] = ui->x + glyph.relX;
            call.data[1] = ui->y + glyph.relY;
            call.data[2] = ui->z;
            call.data[3] = glyph.w;
            call.data[4] = glyph.h;
            call.data[5] = ui->rotation;
            call.data[6] = glyph.a;
            call.data[7] = glyph.r;
            call.data[8] = glyph.g;
            call.data[9] = glyph.b;
            call.data[10] = 1.0f;
            call.data[11] = glyph.uvX0;
            call.data[12] = glyph.uvY0;
            call.data[13] = glyph.uvX1;
            call.data[14] = glyph.uvY1;

            calls.push_back(std::move(call));
        }

        return calls;
    }

    void TTFTextSystem::applyPositionUpdate(_unique_id entityId, CompRef<PositionComponent> ui)
    {
        auto it = entityGlyphTemplates.find(entityId);
        if (it == entityGlyphTemplates.end())
            return;

        entityRenderCalls[entityId] = createRenderCall(ui, it->second);
    }

    // Parses inline formatting commands (such as \n for newline and \c{r,g,b,a} for color changes)
    // and returns a vector of TTFText segments (each segment is a copy of the original, with its text and color set).
    std::vector<TTFText> TTFTextSystem::parseFormattedText(const TTFText& original)
    {
        std::vector<TTFText> segments;
        std::string currentSegment;
        // Start with the original color.
        constant::Vector4D currentColor = original.colors;

        size_t i = 0;
        while (i < original.text.size())
        {
            if (original.text[i] == '\n')
            {
                // If we have accumulated text, flush it into a segment.
                if (not currentSegment.empty())
                {
                    TTFText seg = original;
                    seg.text = currentSegment;
                    seg.colors = currentColor;
                    segments.push_back(seg);
                    currentSegment.clear();
                }
                // Insert a newline marker as a segment.
                TTFText newlineSeg = original;
                newlineSeg.text = "\n";
                newlineSeg.colors = currentColor;
                segments.push_back(newlineSeg);
                i += 1;  // Skip over "\n"
                continue;
            }

            // Check for an escape character.
            if (original.text[i] == '\\')
            {
                if (i + 1 < original.text.size())
                {
                    char nextChar = original.text[i + 1];
                    // Newline tag: "\n"
                    if (nextChar == 'n')
                    {
                        // If we have accumulated text, flush it into a segment.
                        if (not currentSegment.empty())
                        {
                            TTFText seg = original;
                            seg.text = currentSegment;
                            seg.colors = currentColor;
                            segments.push_back(seg);
                            currentSegment.clear();
                        }
                        // Insert a newline marker as a segment.
                        TTFText newlineSeg = original;
                        newlineSeg.text = "\n";
                        newlineSeg.colors = currentColor;
                        segments.push_back(newlineSeg);
                        i += 2;  // Skip over "\n"
                        continue;
                    }
                    // Color change tag: "\c{r,g,b,a}"
                    else if (nextChar == 'c' and i + 2 < original.text.size() and original.text[i + 2] == '{')
                    {
                        // Flush the current text.
                        if (not currentSegment.empty())
                        {
                            TTFText seg = original;
                            seg.text = currentSegment;
                            seg.colors = currentColor;
                            segments.push_back(seg);
                            currentSegment.clear();
                        }

                        // Jump past "\c{"
                        i += 3;
                        std::string colorSpec;
                        // Read until the closing '}'
                        while (i < original.text.size() and original.text[i] != '}')
                        {
                            colorSpec.push_back(original.text[i]);
                            i++;
                        }
                        if (i < original.text.size() and original.text[i] == '}')
                        {
                            i++; // Skip the closing '}'
                        }
                        // Parse the color spec (assumed CSV format)
                        std::istringstream iss(colorSpec);
                        std::string token;
                        std::vector<float> rgba;
                        while (std::getline(iss, token, ','))
                        {
                            try
                            {
                                rgba.push_back(std::stof(token));
                            }
                            catch (const std::exception&)
                            {
                                LOG_ERROR(DOM, "Failed to parse color value: " << token);
                            }
                        }
                        if (rgba.size() == 4)
                        {
                            currentColor = constant::Vector4D(rgba[0], rgba[1], rgba[2], rgba[3]);
                        }
                        else
                        {
                            // If parsing fails, fallback to default color.
                            currentColor = original.colors;
                        }
                        continue;
                    }
                }
            }
            // Regular character: add it to the current segment.
            currentSegment.push_back(original.text[i]);
            i++;
        }
        // Flush remaining text.
        if (not currentSegment.empty())
        {
            TTFText seg = original;
            seg.text = currentSegment;
            seg.colors = currentColor;
            segments.push_back(seg);
        }
        return segments;
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