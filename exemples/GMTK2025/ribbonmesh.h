#pragma once

#include "Renderer/mesh.h"
#include "Maths/geometry.h"   // For Point2D, Segment2D
#include <vector>

#include "Renderer/renderer.h"

#include "constant.h"

#include "logger.h"

namespace pg {

    // 1) Generate ribbon data
    struct RibbonData
    {
        std::vector<float> verts;
        std::vector<unsigned int> idx;
    };

    /**
     * @brief A dynamic textured ribbon mesh that follows a polyline path.
     */
    struct TexturedRibbonMesh : public Mesh
    {
        TexturedRibbonMesh(const std::vector<Point2D>& path, float halfWidth, bool repeatBySize = false, float repeatValue = 1.0f) : Mesh(),
            m_path(path),
            m_halfWidth(halfWidth),
            m_repeatBySize(repeatBySize),
            m_repeatValue(repeatValue)
        {
            // No pre-population of modelInfo here; done in generateMesh()
        }

        virtual ~TexturedRibbonMesh() {}

        RibbonData prepareMesh();

        /**
         * Build the ribbon vertex & index buffers and upload to OpenGL.
         */
        void generateMesh() override;

    private:
        std::vector<Point2D> m_path;
        float m_halfWidth;
        bool  m_repeatBySize;
        float m_repeatValue;
    };

    struct RibbonComponent : public Ctor
    {
        RibbonComponent(const std::string& textureName,
                        const std::vector<Point2D>& path,
                        float width,
                        bool repeatBySize,
                        float repeatValue,
                        size_t viewport = 0) :
                        textureName(textureName), path(path), width(width),
                        repeatBySize(repeatBySize), repeatValue(repeatValue), viewport(viewport)
        {}

        RibbonComponent(const RibbonComponent& other) : ecsRef(other.ecsRef), entityId(other.entityId),
            textureName(other.textureName), path(other.path), width(other.width),
            repeatBySize(other.repeatBySize), repeatValue(other.repeatValue),
            viewport(other.viewport), clean(other.clean)
        {}

        RibbonComponent& operator=(const RibbonComponent& other)
        {
            ecsRef = other.ecsRef;
            entityId = other.entityId;
            textureName = other.textureName;
            path = other.path;
            width = other.width;
            repeatBySize = other.repeatBySize;
            repeatValue = other.repeatValue;
            viewport = other.viewport;

            clean = other.clean;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        void setPath(const std::vector<Point2D>& newPath)
        {
            path = newPath;

            clean = false; // Mark as dirty to regenerate the mesh
        }

        EntitySystem* ecsRef;
        _unique_id entityId;

        std::string textureName;
        std::vector<Point2D> path;
        float width;
        bool repeatBySize;
        float repeatValue;
        size_t viewport;

        bool clean = false;
    };

    struct MeshRenderCall
    {
        MeshRenderCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    struct TexturedRibbonComponentSystem : public AbstractRenderer, public System<Own<RibbonComponent>, Own<MeshRenderCall>, Ref<PositionComponent>, InitSys>
    {
        TexturedRibbonComponentSystem(MasterRenderer* master): AbstractRenderer(master, RenderStage::Render) {}

        std::string getSystemName() const override { return "Ribbon System"; }

        void init() override
        {
            // baseMaterial.shader = masterRenderer->getShader("ribbonShader");
            baseMaterial.shader = masterRenderer->getShader("lineShader");
            baseMaterial.nbTextures = 1;
            baseMaterial.uniformMap.emplace("sWidth", "ScreenWidth");
            baseMaterial.uniformMap.emplace("sHeight", "ScreenHeight");
        }

        void execute() override
        {
            renderCallList.clear();

            const auto& renderCallView = view<MeshRenderCall>();

            renderCallList.reserve(renderCallView.nbComponents());

            for (const auto& renderCall : renderCallView)
            {
                renderCallList.push_back(renderCall->call);
            }

            for (auto* comp : view<RibbonComponent>())
            {
                if (comp->clean)
                {
                    continue;
                }

                auto ent = ecsRef->getEntity(comp->entityId);

                if (not ent)
                {
                    LOG_ERROR("RibbonComponentSystem", "Entity with ID " << comp->entityId << " not found for RibbonComponent.");
                    continue;
                }

                RenderCall rc = createRenderCall(comp);

                if (ent->has<MeshRenderCall>())
                    ent->get<MeshRenderCall>()->call = rc;
                else
                    ecsRef->_attach<MeshRenderCall>(ent, rc);

                comp->clean = true;
            }

            finishChanges();
            changed = false;
        }

    private:
        RenderCall createRenderCall(RibbonComponent* ribbon)
        {
            auto mesh = std::make_shared<TexturedRibbonMesh>(ribbon->path, ribbon->width * 0.5f, ribbon->repeatBySize, ribbon->repeatValue);

            RenderCall call(mesh);

            call.setRenderStage(renderStage);
            call.setViewport(ribbon->viewport);
            call.setOpacity(OpacityType::Opaque);

            if (masterRenderer->hasMaterial(ribbon->textureName))
            {
                call.setMaterial(masterRenderer->getMaterialID(ribbon->textureName));
            }
            else
            {
                Material simpleShapeMaterial = baseMaterial;

                simpleShapeMaterial.textureId[0] = masterRenderer->getTexture(ribbon->textureName).id;

                call.setMaterial(masterRenderer->registerMaterial(ribbon->textureName, simpleShapeMaterial));
            }

            return call;
        }

        Material baseMaterial;
        std::queue<_unique_id> updateQueue;
        bool changed = false;
    };

} // namespace pg
