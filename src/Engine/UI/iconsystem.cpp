#include "stdafx.h"

#include "iconsystem.h"

#include "ECS/entitysystem.h"

#ifdef __EMSCRIPTEN__
#define GL_GLEXT_PROTOTYPES 1
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengl.h>
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
        constexpr const char * const DOM = "Icon System";

        // File stem of a path: "res/icons/chronicle/time.svg" -> "time".
        std::string iconNameFromPath(const std::string& path)
        {
            const size_t slash = path.find_last_of("/\\");
            const size_t start = (slash == std::string::npos) ? 0 : slash + 1;

            size_t dot = path.find_last_of('.');
            if (dot == std::string::npos or dot < start)
                dot = path.size();

            return path.substr(start, dot - start);
        }
    }

    IconSystem::IconSystem(MasterRenderer* renderer) : AbstractRenderer(renderer, RenderStage::Render)
    {
    }

    void IconSystem::init()
    {
        LOG_THIS_MEMBER(DOM);

        baseMaterialPreset.shader = masterRenderer->getShader("ttfTexture");

        baseMaterialPreset.nbTextures = 1;

        baseMaterialPreset.uniformMap.emplace("sWidth", "ScreenWidth");
        baseMaterialPreset.uniformMap.emplace("sHeight", "ScreenHeight");

        baseMaterialPreset.setSimpleMesh({3, 2, 1, 1, 3, 1, 4});

        auto group = registerGroup<PositionComponent, IconComponent, ViewportComponent>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE(DOM, "Add entity " << entity->id << " to ui - icon group !");

            auto ui = entity->get<PositionComponent>();
            auto icon = entity->get<IconComponent>();
            size_t viewport = entity->has<ViewportComponent>() ? entity->get<ViewportComponent>()->viewport : 0;

            if (ui and icon)
            {
                RenderCall call = createRenderCall(icon, ui, viewport);

                if (not call.data.empty())
                    entityRenderCalls[entity->id] = std::move(call);
            }

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem*, _unique_id id) {
            LOG_MILE(DOM, "Remove entity " << id << " of ui - icon group !");

            entityRenderCalls.erase(id);

            changed = true;
        });
    }

    void IconSystem::onEvent(const PositionSettledEvent& event)
    {
        markDirty(event.id);
    }

    void IconSystem::onEvent(const IconChangedEvent& event)
    {
        markDirty(event.id);
    }

    void IconSystem::onEvent(const ViewportComponentChangedEvent& event)
    {
        markDirty(event.id);
    }

    void IconSystem::markDirty(_unique_id id)
    {
        dirtyEntities.push_back(id);

        changed = true;
    }

    void IconSystem::execute()
    {
        if (not changed)
            return;

        for (const auto& id : dirtyEntities)
        {
            rebuildEntity(id);
        }
        dirtyEntities.clear();

        renderCallList.clear();
        renderCallList.reserve(entityRenderCalls.size());

        for (const auto& [id, call] : entityRenderCalls)
            renderCallList.push_back(call);

        finishChanges();
    }

    void IconSystem::rebuildEntity(_unique_id id)
    {
        auto entity = ecsRef->getEntity(id);
        if (not entity)
        {
            entityRenderCalls.erase(id);
            return;
        }

        auto ui = entity->get<PositionComponent>();
        auto icon = entity->get<IconComponent>();

        // The event may target a non-icon entity (PositionSettledEvent fires for
        // every settled position); drop any stale call and move on.
        if (not ui or not icon)
        {
            entityRenderCalls.erase(id);
            return;
        }

        size_t viewport = entity->has<ViewportComponent>() ? entity->get<ViewportComponent>()->viewport : 0;

        RenderCall call = createRenderCall(icon, ui, viewport);

        if (call.data.empty())
            entityRenderCalls.erase(id);
        else
            entityRenderCalls[id] = std::move(call);
    }

    const IconEntry* IconSystem::pickEntry(const std::string& setName, const std::string& name, float requestedPx) const
    {
        auto setIt = setEntries.find(setName);
        if (setIt == setEntries.end())
            return nullptr;

        const std::vector<IconEntry>& entries = setIt->second;

        const IconEntry* smallestFit = nullptr;
        const IconEntry* largest = nullptr;

        for (const auto& entry : entries)
        {
            if (entry.name != name)
                continue;

            // Largest registered size is the fallback when nothing is big enough.
            if (not largest or entry.size > largest->size)
                largest = &entry;

            // Smallest registered size that still covers the requested pixels.
            const bool fits = static_cast<float>(entry.size) >= requestedPx;
            if (fits and (not smallestFit or entry.size < smallestFit->size))
                smallestFit = &entry;
        }

        if (smallestFit)
            return smallestFit;

        return largest;
    }

    size_t IconSystem::getMaterialId(const std::string& setName)
    {
        auto it = materialIds.find(setName);
        if (it != materialIds.end())
            return it->second;

        std::string textureName = "IconAtlas_" + setName;

        size_t materialId = 0;

        if (masterRenderer->hasMaterial(textureName))
        {
            materialId = masterRenderer->getMaterialID(textureName);
        }
        else
        {
            Material material = baseMaterialPreset;
            material.textureId[0] = masterRenderer->getTexture(textureName).id;
            materialId = masterRenderer->registerMaterial(textureName, material);
        }

        materialIds[setName] = materialId;

        return materialId;
    }

    RenderCall IconSystem::createRenderCall(CompRef<IconComponent> icon, CompRef<PositionComponent> ui, size_t viewport)
    {
        RenderCall call;

        const IconEntry* entry = pickEntry(icon->iconSet, icon->iconName, ui->width);

        if (not entry)
        {
            const std::string key = icon->iconSet + "/" + icon->iconName;

            if (loggedMissing.insert(key).second)
                LOG_ERROR(DOM, "No icon named '" << icon->iconName << "' in set '" << icon->iconSet << "'");

            return call;
        }

        size_t materialId = getMaterialId(icon->iconSet);

        call.processPositionComponent(ui);

        call.setMaterial(materialId);
        call.setOpacity(OpacityType::Additive);
        call.setRenderStage(renderStage);
        call.setViewport(viewport);

        const constant::Vector4D& colors = icon->colors;

        // The text shader expects colours in 0-1; components store them in 0-255.
        call.data.resize(15);
        call.data[0]  = ui->x;
        call.data[1]  = ui->y;
        call.data[2]  = ui->z;
        call.data[3]  = ui->width;
        call.data[4]  = ui->height;
        call.data[5]  = ui->rotation;
        call.data[6]  = colors.w / 255.0f;
        call.data[7]  = colors.x / 255.0f;
        call.data[8]  = colors.y / 255.0f;
        call.data[9]  = colors.z / 255.0f;
        call.data[10] = 1.0f;
        call.data[11] = entry->uvTopLeft.x;
        call.data[12] = entry->uvTopLeft.y;
        call.data[13] = entry->uvBottomRight.x;
        call.data[14] = entry->uvBottomRight.y;

        return call;
    }

    void IconSystem::registerIconSet(const std::string& setName, const std::vector<std::string>& svgPaths, const std::vector<int>& sizes)
    {
        LOG_THIS_MEMBER(DOM);

        IconAtlasBuilder builder;

        for (const auto& path : svgPaths)
        {
            auto doc = SvgLoader::parseFile(path);
            if (not doc.has_value())
            {
                LOG_ERROR(DOM, "Failed to load icon: " << path);
                continue;
            }

            const std::string name = iconNameFromPath(path);

            for (int size : sizes)
            {
                SvgCoverage coverage = SvgLoader::rasterizeCoverage(*doc, size);

                if (not builder.add(name, size, coverage))
                    LOG_ERROR(DOM, "Icon atlas full, dropped: " << name << "@" << size);
            }
        }

        setEntries[setName] = builder.entries();
        setAtlasSizes[setName] = {builder.width(), builder.height()};

        // Copy the atlas out; the builder is gone by the time the queue runs.
        std::vector<unsigned char> atlasBuffer = builder.buffer();
        const int atlasWidth = builder.width();
        const int atlasHeight = builder.height();

        auto callback = [atlasBuffer, atlasWidth, atlasHeight](size_t oldId) -> OpenGLTexture
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
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, atlasWidth, atlasHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, atlasBuffer.data());
#else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, atlasBuffer.data());
#endif
            // Icons are drawn at their rasterised size, so no mipmaps.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            OpenGLTexture iconTexture;
            iconTexture.id = texture;
            iconTexture.transparent = true;

            return iconTexture;
        };

        masterRenderer->queueRegisterTexture("IconAtlas_" + setName, callback);
    }

    void IconSystem::registerEntriesForTest(const std::string& setName, const std::vector<IconEntry>& entries, int atlasW, int atlasH)
    {
        setEntries[setName] = entries;
        setAtlasSizes[setName] = {atlasW, atlasH};
    }

    const RenderCall* IconSystem::getRenderCall(_unique_id id) const
    {
        auto it = entityRenderCalls.find(id);

        if (it == entityRenderCalls.end())
            return nullptr;

        return &it->second;
    }
}
