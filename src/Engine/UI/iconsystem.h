#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "2D/position.h"
#include "Renderer/renderer.h"
#include "UI/iconatlas.h"

#include "Components/IconComponent.generated.h"
#include "Components/ViewportComponent.generated.h"

namespace pg
{
    struct IconSystem : public AbstractRenderer, System<Own<IconComponent>, Ref<PositionComponent>,
        Listener<PositionSettledEvent>, Listener<IconChangedEvent>, Listener<ViewportComponentChangedEvent>, InitSys>
    {
        IconSystem(MasterRenderer* renderer);

        virtual std::string getSystemName() const override { return "Icon System"; }

        virtual void init() override;
        virtual void execute() override;

        virtual void onEvent(const PositionSettledEvent& event) override;
        virtual void onEvent(const IconChangedEvent& event) override;
        virtual void onEvent(const ViewportComponentChangedEvent& event) override;

        /// Rasterises every .svg in `svgPaths` at each size, packs them, and queues the atlas upload as texture "IconAtlas_<setName>".
        /// Entries are available immediately (CPU side); the texture appears when the renderer processes its queue.
        void registerIconSet(const std::string& setName, const std::vector<std::string>& svgPaths, const std::vector<int>& sizes);

        /// Test hook: install entries without any GL. Only for tests.
        void registerEntriesForTest(const std::string& setName, const std::vector<IconEntry>& entries, int atlasW, int atlasH);

        /// Test hook: read the render call built for an entity, or null if none.
        const RenderCall* getRenderCall(_unique_id id) const;

        RenderCall createRenderCall(CompRef<IconComponent> icon, CompRef<PositionComponent> ui, size_t viewport);

        Material baseMaterialPreset;

    private:
        const IconEntry* pickEntry(const std::string& setName, const std::string& name, float requestedPx) const;
        size_t getMaterialId(const std::string& setName);

        void markDirty(_unique_id id);
        void rebuildEntity(_unique_id id);

        std::unordered_map<std::string, std::vector<IconEntry>> setEntries;
        std::unordered_map<std::string, glm::ivec2> setAtlasSizes;
        std::unordered_map<std::string, size_t> materialIds;
        std::unordered_map<_unique_id, RenderCall> entityRenderCalls;
        std::vector<_unique_id> dirtyEntities;
        std::unordered_set<std::string> loggedMissing;
    };

    template <typename Type>
    CompList<PositionComponent, UiAnchor, ViewportComponent, IconComponent> makeIcon(Type* ecs, const std::string& iconSet, const std::string& iconName, float sizePx, constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f})
    {
        auto entity = ecs->createEntity();
        auto ui = ecs->template attach<PositionComponent>(entity);
        ui->setWidth(sizePx);
        ui->setHeight(sizePx);
        auto anchor = ecs->template attach<UiAnchor>(entity);
        auto vp = ecs->template attach<ViewportComponent>(entity);
        auto icon = ecs->template attach<IconComponent>(entity, iconSet, iconName, colors);
        return {entity, ui, anchor, vp, icon};
    }
}
