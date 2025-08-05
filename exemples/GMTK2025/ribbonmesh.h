#pragma once

#include "Renderer/mesh.h"
#include "Maths/geometry.h"   // For Point2D, Segment2D
#include <vector>

#include "Renderer/renderer.h"

#include "constant.h"

#include "logger.h"

#include "simplerenderer.h"

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

    struct RibbonComponent : public Component
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

        DEFAULT_COMPONENT_MEMBERS(RibbonComponent)

        inline void setPath(const std::vector<Point2D>& newPath)
        {
            setValue(path, newPath);
        }

        std::string textureName;
        std::vector<Point2D> path;
        float width;
        bool repeatBySize;
        float repeatValue;
        size_t viewport;
    };

    struct TexturedRibbonComponentSystem : public SimpleRenderer<RibbonComponent>
    {
        TexturedRibbonComponentSystem(MasterRenderer* masterRenderer) : SimpleRenderer(masterRenderer)
        {

        }

        void setupRenderer() override
        {
            auto baseMaterial = newMaterial("defaultRibbon");

            // baseMaterial.shader = masterRenderer->getShader("ribbonShader");
            baseMaterial->shader = masterRenderer->getShader("lineShader");
            baseMaterial->nbTextures = 1;
            baseMaterial->uniformMap.emplace("sWidth", "ScreenWidth");
            baseMaterial->uniformMap.emplace("sHeight", "ScreenHeight");
        }

        std::string getSystemName() const override { return "Ribbon System"; }

        RenderCall createRenderCall(CompRef<RibbonComponent> ribbon) override
        {
            auto mesh = std::make_shared<TexturedRibbonMesh>(ribbon->path, ribbon->width * 0.5f, ribbon->repeatBySize, ribbon->repeatValue);

            RenderCall call(mesh);

            call.setRenderStage(renderStage);
            call.setViewport(ribbon->viewport);
            call.setOpacity(OpacityType::Opaque);

            applyMaterial(call, "defaultRibbon", {ribbon->textureName});

            return call;
        }
    };

} // namespace pg
