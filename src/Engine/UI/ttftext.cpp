
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

        group->removeOfGroup([this](EntitySystem*, _unique_id id) {
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

    void TTFTextSystem::onEvent(const PositionSettledEvent& event)
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
        const std::string alias = (fontName == "" ? fontPath : fontName);

        // FreeType work is synchronous and CPU-only, so glyphs and metrics are
        // available before the first frame; only the GL upload stays deferred.
        FontAtlas atlas;
        if (not atlas.build(ft, fontPath, size, defaultCharset()))
        {
            LOG_ERROR(DOM, "Failed to build font atlas for: " << fontPath);
            return;
        }

        fonts[alias] = std::move(atlas);

        // Copy the atlas bytes into the upload lambda; the atlas itself stays on the CPU for measuring.
        std::vector<unsigned char> atlasBuffer = fonts[alias].buffer();

        auto f = [atlasBuffer](size_t oldId) -> OpenGLTexture
        {
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

#ifdef __EMSCRIPTEN__
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, FontAtlas::Width, FontAtlas::Height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, atlasBuffer.data());
#else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FontAtlas::Width, FontAtlas::Height, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBuffer.data());
#endif
            // Text is drawn at its rasterised size, so no mipmaps (mipmaps blurred it).
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            OpenGLTexture fontTexture;
            fontTexture.id = texture;
            fontTexture.transparent = true;

            return fontTexture;
        };

        const std::string textureName = "TTFText_" + alias;

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

    TextMetrics TTFTextSystem::layoutText(const FontAtlas& atlas, const std::vector<TTFText>& segments, float scale, float maxWidth, float spacing, const GlyphEmitter& emit) const
    {
        const FontMetrics& fm = atlas.metrics();

        const float lineAdvance = fm.lineHeight * scale + spacing;
        const float ascender = fm.ascender;
        const float spaceAdvance = atlas.glyphOrNotdef(0x20).advance * scale;

        const bool wrapEnabled = maxWidth > 0.0f;

        float penX = 0.0f;
        int lineIndex = 0;
        float maxLineWidth = 0.0f;
        uint32_t prev = 0;   // previous code point, for kerning; 0 = start of line

        for (const auto& seg : segments)
        {
            if (seg.text == "\n")
            {
                penX = 0.0f;
                ++lineIndex;
                prev = 0;
                continue;
            }

            const std::string& s = seg.text;

            for (size_t i = 0; i < s.length(); ++i)
            {
                const unsigned char byte = static_cast<unsigned char>(s[i]);

                if (byte == ' ')
                {
                    penX += spaceAdvance;
                    prev = byte;
                    continue;
                }

                // At a word start, wrap the whole word down a line if it would overflow.
                const bool wordStart = (i == 0) or (s[i - 1] == ' ');
                if (wrapEnabled and wordStart)
                {
                    float wordWidth = 0.0f;
                    for (size_t j = i; j < s.length() and s[j] != ' '; ++j)
                        wordWidth += atlas.glyphOrNotdef(static_cast<unsigned char>(s[j])).advance * scale;

                    if (penX > 0.0f and penX + wordWidth > maxWidth)
                    {
                        penX = 0.0f;
                        ++lineIndex;
                        prev = 0;
                    }
                }

                const uint32_t codepoint = static_cast<uint32_t>(byte);

                // Kerning keeps sub-pixel precision on the pen; only the glyph origin snaps.
                if (fm.hasKerning and prev != 0)
                    penX += atlas.kerning(prev, codepoint) * scale;

                const GlyphInfo& glyph = atlas.glyphOrNotdef(codepoint);

                const float relX = std::round(penX + glyph.bearing.x * scale);
                const float relY = lineIndex * lineAdvance + (ascender - glyph.bearing.y) * scale;

                if (emit)
                    emit(codepoint, relX, relY, glyph, seg.colors);

                penX += glyph.advance * scale;
                prev = codepoint;

                if (penX > maxLineWidth)
                    maxLineWidth = penX;
            }
        }

        TextMetrics metrics;
        metrics.width = maxLineWidth;
        metrics.lineHeight = fm.lineHeight * scale;
        metrics.ascender = ascender * scale;
        metrics.lineCount = lineIndex + 1;
        metrics.height = metrics.lineCount * (metrics.lineHeight + spacing);

        return metrics;
    }

    TextMetrics TTFTextSystem::measureText(const std::string& font, const std::string& text, float scale, float maxWidth, float spacing) const
    {
        auto it = fonts.find(font);
        if (it == fonts.end())
            return TextMetrics{};

        TTFText temp;
        temp.text = text;
        temp.colors = constant::Vector4D{255.0f, 255.0f, 255.0f, 255.0f};

        std::vector<TTFText> segments = parseFormattedText(temp);

        return layoutText(it->second, segments, scale, maxWidth, spacing, nullptr);
    }

    std::vector<TTFTextSystem::GlyphRenderData> TTFTextSystem::buildGlyphTemplates(CompRef<PositionComponent> ui, CompRef<TTFText> obj, size_t viewport)
    {
        std::vector<GlyphRenderData> glyphs;

        auto fontIt = fonts.find(obj->fontPath);
        if (fontIt == fonts.end())
            return glyphs;

        const FontAtlas& atlas = fontIt->second;

        std::vector<TTFText> segments = parseFormattedText(*obj);

        const float scale = obj->scale;
        const float maxWidth = obj->wrap ? ui->width : 0.0f;
        const size_t materialId = getMaterialId(obj->fontPath);

        auto emit = [&](uint32_t, float relX, float relY, const GlyphInfo& glyph, const constant::Vector4D& color)
        {
            GlyphRenderData glyphData;
            glyphData.relX = relX;
            glyphData.relY = relY;
            glyphData.w = glyph.size.x * scale;
            glyphData.h = glyph.size.y * scale;
            glyphData.a = color.w / 255.0f;
            glyphData.r = color.x / 255.0f;
            glyphData.g = color.y / 255.0f;
            glyphData.b = color.z / 255.0f;
            glyphData.uvX0 = glyph.uvTopLeft.x;
            glyphData.uvY0 = glyph.uvTopLeft.y;
            glyphData.uvX1 = glyph.uvBottomRight.x;
            glyphData.uvY1 = glyph.uvBottomRight.y;
            glyphData.materialId = materialId;
            glyphData.viewport = viewport;

            glyphs.push_back(glyphData);
        };

        TextMetrics metrics = layoutText(atlas, segments, scale, maxWidth, obj->spacing, emit);

        if (areNotAlmostEqual(obj->textWidth, metrics.width))
        {
            obj->textWidth = metrics.width;
            ui->setWidth(metrics.width);
        }

        if (areNotAlmostEqual(obj->textHeight, metrics.height))
        {
            obj->textHeight = metrics.height;
            ui->setHeight(metrics.height);
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
    std::vector<TTFText> TTFTextSystem::parseFormattedText(const TTFText& original) const
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
