#pragma once

#include "Renderer/mesh.h"
#include "Maths/geometry.h"   // For Point2D
#include <vector>

#include "Renderer/renderer.h"

#include "constant.h"

#include "logger.h"

#include "Systems/basicsystems.h"

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

    struct PolygonRenderCall
    {
        PolygonRenderCall(const RenderCall& call) : call(call) {}

        RenderCall call;
    };

    struct PolyFlag : public Component {};

    struct PolygonComponentSystem : public AbstractRenderer, public System<Own<PolygonComponent>, Own<PolygonRenderCall>, Ref<PositionComponent>, InitSys, Listener<EntityChangedEvent>, Listener<TickEvent>>
    {
        PolygonComponentSystem(MasterRenderer* master): AbstractRenderer(master, RenderStage::Render) {}

        std::string getSystemName() const override { return "Polygon System"; }

        virtual void onEvent(const TickEvent& event) override
        {
            // std::vector<_unique_id> entitiesToRemove;
            // for (auto* poly : view<PolygonComponent>())
            // {
            //     if (not poly)
            //         continue;

            //     poly->aliveTime += event.tick;

            //     poly->setOpacity(255.0f * (1 - (poly->aliveTime / 450.0f)));

            //     if (poly->aliveTime > 450.0f)
            //     {
            //         entitiesToRemove.push_back(poly->entityId);
            //         continue;
            //     }

            // //     // Optionally, you can trigger an update if needed
            // //     updateQueue.push(poly->entityId);
            // }

            // for (auto entityId : entitiesToRemove)
            // {
            //     ecsRef->removeEntity(entityId);
            // }
        }

        void init() override
        {
            // Use a simple shader for solid polygon rendering
            baseMaterial.shader = masterRenderer->getShader("polygonShader");
            baseMaterial.nbTextures = 0; // No textures needed for solid polygons

            baseMaterial.uniformMap.emplace("sWidth", "ScreenWidth");
            baseMaterial.uniformMap.emplace("sHeight", "ScreenHeight");

            baseMaterial.nbAttributes = 1;

            auto group = registerGroup<PolygonComponent, PolyFlag>();

            group->addOnGroup([this](EntityRef entity)
            {
                LOG_MILE("Simple 2D Object System", "Add entity " << entity->id << " to ui - 2d shape group !");

                updateQueue.push(entity->id);

                changed = true;
            });

            group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id)
            {
                LOG_MILE("Simple 2D Object System", "Remove entity " << id << " of ui - 2d shape group !");

                auto entity = ecsRef->getEntity(id);

                ecsRef->detach<PolygonRenderCall>(entity);

                LOG_INFO("PolygonComponentSystem", "Removed render call for polygon entity " << id);

                changed = true;
            });
        }

        void execute() override
        {
            if (not changed)
                return;

            while (not updateQueue.empty())
            {
                auto entityId = updateQueue.front();

                auto entity = ecsRef->getEntity(entityId);

                if (not entity)
                {
                    updateQueue.pop();
                    continue;
                }

                auto comp = entity->get<PolygonComponent>();

                if (entity->has<PolygonRenderCall>())
                {
                    entity->get<PolygonRenderCall>()->call = createRenderCall(comp);
                }
                else
                {
                    ecsRef->_attach<PolygonRenderCall>(entity, createRenderCall(comp));
                }

                updateQueue.pop();
            }

            LOG_INFO("PolygonComponentSystem", "Updating render calls for polygons... changed: " << changed);

            renderCallList.clear();

            const auto& renderCallView = view<PolygonRenderCall>();

            renderCallList.reserve(renderCallView.nbComponents());

            for (const auto& renderCall : renderCallView)
            {
                renderCallList.push_back(renderCall->call);
            }

            finishChanges();
        }

        void onEvent(const EntityChangedEvent& event) override
        {
            auto entity = ecsRef->getEntity(event.id);

            if (not entity or not entity->has<PolygonComponent>())
                return;

            updateQueue.push(event.id);

            changed = true;
        }

    private:
        RenderCall createRenderCall(PolygonComponent* polygon)
        {
            auto mesh = std::make_shared<PolygonMesh>(polygon->polygon);

            RenderCall call(mesh);

            call.setRenderStage(renderStage);
            call.setViewport(polygon->viewport);
            call.setOpacity(OpacityType::Additive);

            // Use the base material (no texture needed for solid polygons)
            if (masterRenderer->hasMaterial("__defaultPolygon"))
            {
                call.setMaterial(masterRenderer->getMaterialID("__defaultPolygon"));
            }
            else
            {
                call.setMaterial(masterRenderer->registerMaterial("__defaultPolygon", baseMaterial));
            }

            call.data.resize(1);

            call.data[0] = polygon->opacity; // Store opacity in the data vector

            return call;
        }

        Material baseMaterial;
        std::queue<_unique_id> updateQueue;
    };

} // namespace pg