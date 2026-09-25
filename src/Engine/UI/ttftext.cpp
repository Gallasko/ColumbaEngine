
#include "stdafx.h"

#include "ttftext.h"

#include "ECS/entitysystem.h"

#include "Helpers/helpers.h"

#include "UI/utf8.h"

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
            lastLayoutWidth.erase(id);
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
        {
            // Wrapped or elided text lays out against its box width: when the settled
            // width differs from the one last laid out against, the glyphs must be
            // rebuilt, not moved. The rebuild only re-sets the height for such text,
            // so the follow-up settle arrives with an unchanged width and takes the
            // position-only path — no rebuild loop.
            auto entity = ecsRef->getEntity(event.id);
            if (entity and entity->has<TTFText>() and entity->has<PositionComponent>()
                and entity->get<TTFText>()->overflow != TextOverflow::Grow)
            {
                auto it = lastLayoutWidth.find(event.id);
                if (it == lastLayoutWidth.end() or areNotAlmostEqual(it->second, entity->get<PositionComponent>()->width))
                {
                    textContentUpdateSet.insert(event.id);
                    changed = true;
                    return;
                }
            }

            positionUpdateSet.insert(event.id);
        }

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
        std::vector<_unique_id> fullRebuildQueue = drainIntersectSorted(textContentUpdateSet, entitiesInRenderGroup);

        // Snapshot and clear positionUpdateSet, intersect with render group,
        // then exclude entities already scheduled for a full rebuild.
        std::vector<_unique_id> positionOnlyQueue;
        {
            std::vector<_unique_id> inGroup = drainIntersectSorted(positionUpdateSet, entitiesInRenderGroup);

            std::set_difference(inGroup.begin(), inGroup.end(),
                                fullRebuildQueue.begin(), fullRebuildQueue.end(),
                                std::back_inserter(positionOnlyQueue));
        }

        // Full rebuild: text content changed, redo glyph layout and render calls.
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

        // Fast-path: position only changed, reuse glyph templates, apply new position.
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

    TextMetrics TTFTextSystem::layoutText(const FontAtlas& atlas, const std::vector<TTFText>& segments, const TextLayoutParams& p, const GlyphEmitter& emit) const
    {
        const FontMetrics& fm = atlas.metrics();

        const float scale = p.scale;
        const float lineAdvance = fm.lineHeight * scale + p.spacing;
        const float ascender = fm.ascender;
        const float trackedSpaceAdvance = (atlas.glyphOrNotdef(0x20).advance + p.letterSpacing) * scale;

        // Grow ignores the box entirely; Wrap breaks lines at it; Ellipsis is a
        // single line cut at it. Alignment applies whenever the box width rules.
        const bool fixedWidth = p.overflow != TextOverflow::Grow and p.maxWidth > 0.0f;
        const bool wrapEnabled = p.overflow == TextOverflow::Wrap and p.maxWidth > 0.0f;

        // Faces without U+2026 elide with three full stops instead.
        const GlyphInfo* horizontalEllipsis = atlas.glyph(0x2026);
        const uint32_t ellipsisCp = horizontalEllipsis ? 0x2026 : 0x2E;
        const int ellipsisRepeat = horizontalEllipsis ? 1 : 3;
        const GlyphInfo& ellipsisGlyph = horizontalEllipsis ? *horizontalEllipsis : atlas.glyphOrNotdef(0x2E);
        const float ellipsisStep = (ellipsisGlyph.advance + p.letterSpacing) * scale;
        const float ellipsisAdvance = ellipsisStep * ellipsisRepeat;

        // A line is buffered before it is emitted: alignment needs the finished line's
        // width, and elision needs to know the line was cut, so glyphs can only be
        // placed once the line is complete. Spaces never enter the buffer (they only
        // advance the pen), so cutting the buffer trims trailing spaces for free.
        struct PendingGlyph
        {
            uint32_t cp;
            float penX;                // pen at glyph start, before alignment
            const GlyphInfo* glyph;
            constant::Vector4D color;
            float penAfter;            // pen after advance + tracking
        };

        std::vector<PendingGlyph> linebuf;
        float penX = 0.0f;
        int lineIndex = 0;
        float maxLineWidth = 0.0f;
        uint32_t prev = 0;   // previous code point, for kerning; 0 = start of line
        bool elided = false;
        bool done = false;   // an elision cut discards all remaining input
        size_t bestFit = 0;  // longest linebuf prefix that still leaves room for the ellipsis
        constant::Vector4D currentColor{255.0f, 255.0f, 255.0f, 255.0f};

        // The line that must absorb an overflow with an ellipsis: every line in
        // Ellipsis mode, the maxLines-th line in Wrap mode.
        auto onLastLine = [&]()
        {
            if (not fixedWidth)
                return false;
            if (p.overflow == TextOverflow::Ellipsis)
                return true;
            return p.maxLines > 0 and lineIndex == p.maxLines - 1;
        };

        auto flushLine = [&](bool withEllipsis)
        {
            if (withEllipsis)
            {
                linebuf.resize(bestFit);

                // When not even the ellipsis fits, the line stays empty: a box too
                // narrow for one glyph is the caller's bug.
                if (ellipsisAdvance <= p.maxWidth)
                {
                    float pen = 0.0f;
                    constant::Vector4D color = currentColor;

                    if (not linebuf.empty())
                    {
                        const PendingGlyph& last = linebuf.back();
                        pen = last.penAfter;
                        if (fm.hasKerning)
                            pen += atlas.kerning(last.cp, ellipsisCp) * scale;
                        color = last.color;
                    }

                    for (int k = 0; k < ellipsisRepeat; ++k)
                    {
                        linebuf.push_back(PendingGlyph{ellipsisCp, pen, &ellipsisGlyph, color, pen + ellipsisStep});
                        pen += ellipsisStep;
                    }
                }

                elided = true;
            }

            const float lineWidth = linebuf.empty() ? 0.0f : linebuf.back().penAfter;
            if (lineWidth > maxLineWidth)
                maxLineWidth = lineWidth;

            // Alignment offset, rounded once per line so glyph origins stay integral.
            float offset = 0.0f;
            if (fixedWidth and p.align != TextAlign::Left)
                offset = std::round(p.align == TextAlign::Centre ? (p.maxWidth - lineWidth) * 0.5f : p.maxWidth - lineWidth);

            if (emit)
            {
                for (const auto& g : linebuf)
                {
                    // Kerning keeps sub-pixel precision on the pen; only the glyph origin snaps.
                    const float relX = std::round(g.penX + g.glyph->bearing.x * scale) + offset;
                    const float relY = lineIndex * lineAdvance + (ascender - g.glyph->bearing.y) * scale;
                    emit(g.cp, relX, relY, *g.glyph, g.color);
                }
            }

            linebuf.clear();
            penX = 0.0f;
            prev = 0;
            bestFit = 0;
        };

        for (const auto& seg : segments)
        {
            if (done)
                break;

            if (seg.text == "\n")
            {
                // A newline past the last permitted line means unshowable content.
                if (onLastLine())
                {
                    flushLine(true);
                    done = true;
                    break;
                }

                flushLine(false);
                ++lineIndex;
                continue;
            }

            currentColor = seg.colors;

            // Decode once per segment; markup was already split off as ASCII, so the
            // remaining bytes are text. Word boundaries stay U+0020.
            const std::vector<uint32_t> codepoints = utf8::decode(seg.text);

            for (size_t i = 0; i < codepoints.size(); ++i)
            {
                const uint32_t codepoint = codepoints[i];

                if (codepoint == 0x20)
                {
                    penX += trackedSpaceAdvance;
                    prev = codepoint;
                    continue;
                }

                // At a word start, wrap the whole word down a line if it would overflow.
                // The last permitted line no longer wraps: it fills and elides instead.
                const bool wordStart = (i == 0) or (codepoints[i - 1] == 0x20);
                if (wrapEnabled and wordStart and not onLastLine())
                {
                    float wordWidth = 0.0f;
                    for (size_t j = i; j < codepoints.size() and codepoints[j] != 0x20; ++j)
                        wordWidth += (atlas.glyphOrNotdef(codepoints[j]).advance + p.letterSpacing) * scale;

                    if (penX > 0.0f and penX + wordWidth > p.maxWidth)
                    {
                        flushLine(false);
                        ++lineIndex;
                    }
                }

                if (fm.hasKerning and prev != 0)
                    penX += atlas.kerning(prev, codepoint) * scale;

                const GlyphInfo& glyph = atlas.glyphOrNotdef(codepoint);

                // Advance the pen by the glyph plus tracking; the last glyph is tracked
                // too, matching CSS, so measurement and drawing stay identical.
                const float penAfter = penX + (glyph.advance + p.letterSpacing) * scale;

                linebuf.push_back(PendingGlyph{codepoint, penX, &glyph, seg.colors, penAfter});

                const bool lastLine = onLastLine();
                if (lastLine)
                {
                    float candidate = penAfter + ellipsisAdvance;
                    if (fm.hasKerning)
                        candidate += atlas.kerning(codepoint, ellipsisCp) * scale;

                    if (candidate <= p.maxWidth)
                        bestFit = linebuf.size();
                }

                penX = penAfter;
                prev = codepoint;

                // Past the box on the last permitted line: the rest of the input can
                // only widen the line, so cut here.
                if (lastLine and penX > p.maxWidth)
                {
                    flushLine(true);
                    done = true;
                    break;
                }
            }
        }

        if (not done)
            flushLine(false);

        TextMetrics metrics;
        metrics.width = maxLineWidth;
        metrics.lineHeight = fm.lineHeight * scale;
        metrics.ascender = ascender * scale;
        metrics.lineCount = lineIndex + 1;
        metrics.height = metrics.lineCount * (metrics.lineHeight + p.spacing);
        metrics.elided = elided;

        return metrics;
    }

    TextMetrics TTFTextSystem::measureText(const std::string& font, const std::string& text, float scale, float maxWidth, float spacing, float letterSpacing) const
    {
        TextLayoutParams params;
        params.scale = scale;
        params.maxWidth = maxWidth;
        params.spacing = spacing;
        params.letterSpacing = letterSpacing;
        params.overflow = maxWidth > 0.0f ? TextOverflow::Wrap : TextOverflow::Grow;

        return measureText(font, text, params);
    }

    TextMetrics TTFTextSystem::measureText(const std::string& font, const std::string& text, const TextLayoutParams& params) const
    {
        auto it = fonts.find(font);
        if (it == fonts.end())
            return TextMetrics{};

        TTFText temp;
        temp.text = text;
        temp.colors = constant::Vector4D{255.0f, 255.0f, 255.0f, 255.0f};

        std::vector<TTFText> segments = parseFormattedText(temp);

        return layoutText(it->second, segments, params, nullptr);
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
        const size_t materialId = getMaterialId(obj->fontPath);

        TextLayoutParams params;
        params.scale = scale;
        params.maxWidth = obj->overflow != TextOverflow::Grow ? ui->width : 0.0f;
        params.spacing = obj->spacing;
        params.letterSpacing = obj->letterSpacing;
        params.overflow = obj->overflow;
        params.align = obj->align;
        params.maxLines = obj->maxLines;

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

        TextMetrics metrics = layoutText(atlas, segments, params, emit);

        if (areNotAlmostEqual(obj->textWidth, metrics.width))
        {
            obj->textWidth = metrics.width;

            // Under a box constraint (Wrap/Ellipsis), ui->width must stay put; textWidth
            // just reports the widest line. Only Grow text sizes its box to fit.
            if (obj->overflow == TextOverflow::Grow)
                ui->setWidth(metrics.width);
        }

        if (areNotAlmostEqual(obj->textHeight, metrics.height))
        {
            obj->textHeight = metrics.height;
            ui->setHeight(metrics.height);
        }

        lastLayoutWidth[obj->entityId] = ui->width;

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
