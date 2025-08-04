#pragma once

#include "Renderer/mesh.h"
#include "Maths/geometry.h"   // For Point2D
#include <vector>

#include "Renderer/renderer.h"

#include "constant.h"

#include "logger.h"

#include "Systems/basicsystems.h"

#include "simplerenderer.h"

namespace pg
{

    //-----------------------------------------------------------------------------
    // Utility: Point-in-Polygon (ray-casting)
    //-----------------------------------------------------------------------------
    // Returns true if point P is inside the simple polygon poly (closed, non-self-intersecting).
    static bool pointInPolygon(const std::vector<Point2D>& poly, const Point2D& P)
    {
        bool inside = false;
        size_t n = poly.size();

        for (size_t i = 0, j = n - 1; i < n; j = i++)
        {
            const Point2D& Pi = poly[i];
            const Point2D& Pj = poly[j];

            bool intersect = ((Pi.y > P.y) != (Pj.y > P.y)) &&
                            (P.x < (Pj.x - Pi.x) * (P.y - Pi.y) / (Pj.y - Pi.y) + Pi.x);

            if (intersect)
                inside = !inside;
        }

        return inside;
    }

    // 1) Generate polygon data
    struct PolygonData
    {
        PolygonData() = default;
        PolygonData(const PolygonData&) = default;
        PolygonData& operator=(const PolygonData&) = default;

        PolygonData(PolygonData&&) = default;
        PolygonData& operator=(PolygonData&&) = default;

        ~PolygonData() = default;
        std::vector<float> verts;
        std::vector<unsigned int> idx;
    };

    /**
     * @brief A dynamic polygon mesh that triangulates a 2D polygon (possibly concave).
     */
    struct PolygonMesh : public Mesh
    {
        PolygonMesh(const std::vector<Point2D>& polygon) : Mesh(), m_polygon(polygon)
        {
            // No pre-population of modelInfo here; done in generateMesh()
        }

        virtual ~PolygonMesh() {}

        PolygonData prepareMesh();

        /**
         * Build the polygon vertex & index buffers and upload to OpenGL.
         */
        void generateMesh() override;

    private:
        std::vector<Point2D> m_polygon;

        // Ear clipping triangulation helpers
        bool isEar(const std::vector<Point2D>& poly, int prev, int curr, int next) const;
        bool isPointInTriangle(const Point2D& p, const Point2D& a, const Point2D& b, const Point2D& c) const;
        float cross2D(const Point2D& a, const Point2D& b, const Point2D& c) const;
        bool isConvexVertex(const std::vector<Point2D>& poly, int prev, int curr, int next) const;
        std::vector<unsigned int> triangulateEarClipping(const std::vector<Point2D>& polygon) const;
    };

    struct PolygonComponent : public Ctor
    {
        PolygonComponent(const std::vector<Point2D>& polygon, size_t viewport = 0) :
                        polygon(polygon), viewport(viewport), opacity(1.0f)
        {}

        PolygonComponent(const PolygonComponent& other) : ecsRef(other.ecsRef), entityId(other.entityId),
            polygon(other.polygon), viewport(other.viewport), opacity(other.opacity)
        {}

        PolygonComponent& operator=(const PolygonComponent& other)
        {
            ecsRef = other.ecsRef;
            entityId = other.entityId;
            polygon = other.polygon;
            viewport = other.viewport;
            opacity = other.opacity;

            return *this;
        }

        virtual void onCreation(EntityRef entity)
        {
            ecsRef = entity->world();
            entityId = entity->id;
        }

        void setPolygon(const std::vector<Point2D>& newPolygon)
        {
            polygon = newPolygon;

            if (ecsRef)
            {
                ecsRef->sendEvent(EntityChangedEvent{entityId});
            }
        }

        EntitySystem* ecsRef;
        _unique_id entityId;

        std::vector<Point2D> polygon;
        size_t viewport;

        // Todo add a color or material to the polygon
        // Todo add a z-index to the polygon

        float opacity = 1.0f; // Default opacity

        float aliveTime = 0.0f; // Time the polygon has been alive (for animations or effects)

        inline void setOpacity(float newOpacity)
        {
            opacity = newOpacity;

            if (ecsRef)
            {
                ecsRef->sendEvent(EntityChangedEvent{entityId});
            }
        }
    };

    struct PolygonComponentSystem : public SimpleRenderer<PolygonComponent>
    {
        PolygonComponentSystem(MasterRenderer* masterRenderer) : SimpleRenderer(masterRenderer)
        {

        }

        void setupRenderer() override
        {
            auto baseMaterial = newMaterial("defaultPolygon");

            baseMaterial->shader = masterRenderer->getShader("polygonShader");
            baseMaterial->nbTextures = 0; // No textures needed for solid polygons

            baseMaterial->uniformMap.emplace("sWidth", "ScreenWidth");
            baseMaterial->uniformMap.emplace("sHeight", "ScreenHeight");

            baseMaterial->nbAttributes = 1;
        }

        std::string getSystemName() const override { return "Polygon Component System"; }

        RenderCall createRenderCall(CompRef<PolygonComponent> polygon) override
        {
            auto mesh = std::make_shared<PolygonMesh>(polygon->polygon);

            RenderCall call(mesh);

            call.setRenderStage(renderStage);
            call.setViewport(polygon->viewport);
            call.setOpacity(OpacityType::Additive);

            applyMaterial(call, "defaultPolygon");

            call.data[0] = polygon->opacity; // Store opacity in the data vector

            return call;
        }
    };


} // namespace pg