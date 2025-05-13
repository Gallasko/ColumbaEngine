#pragma once

#include "Renderer/camera.h"
#include "Renderer/renderer.h"

#include "ECS/system.h"
#include "ECS/entitysystem.h"

#include "2D/position.h"

namespace pg
{
    struct FollowCamera2DChangedEvent { _unique_id id; };

    struct FollowCamera2D : public Ctor
    {
        FollowCamera2D() {}
        virtual ~FollowCamera2D() {}

        virtual void onCreation(EntityRef entity)
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        void setViewportWidth(float width)
        {
            if (width != viewportWidth)
            {
                viewportWidth = width;

                if (ecsRef)
                    ecsRef->sendEvent(FollowCamera2DChangedEvent{id});
            }
        }

        void setViewportHeight(float height)
        {
            if (height != viewportHeight)
            {
                viewportHeight = height;

                if (ecsRef)
                    ecsRef->sendEvent(FollowCamera2DChangedEvent{id});
            }
        }

        void setNear(float nearPlane)
        {
            if (nearPlane != this->nearPlane)
            {
                this->nearPlane = nearPlane;

                if (ecsRef)
                    ecsRef->sendEvent(FollowCamera2DChangedEvent{id});
            }
        }

        void setFar(float farPlane)
        {
            if (farPlane != this->farPlane)
            {
                this->farPlane = farPlane;

                if (ecsRef)
                    ecsRef->sendEvent(FollowCamera2DChangedEvent{id});
            }
        }

        float viewportWidth = 800.0f;
        float viewportHeight = 600.0f;
        
        float nearPlane = -1.0f;
        float farPlane = 1.0f;

        _unique_id id = 0;

        EntitySystem *ecsRef = nullptr;
    };

    struct FollowCamera2DSystem : public System<Listener<EntityChangedEvent>, Listener<FollowCamera2DChangedEvent>, Own<FollowCamera2D>, Own<PositionComponent>, InitSys>
    {
        virtual std::string getSystemName() const override { return "CameraSystem"; }

        FollowCamera2DSystem(MasterRenderer* masterRenderer) : masterRenderer(masterRenderer)
        {

        }

        virtual void init() override
        {
            auto group = registerGroup<FollowCamera2D, PositionComponent>();

            group->addOnGroup([this](EntityRef entity) {
                LOG_MILE(DOM, "Add entity " << entity->id << " to ui - camera group !");

                auto followCam = entity->get<FollowCamera2D>();
                auto pos = entity->get<PositionComponent>();

                auto cam = ecsRef->attach<BaseCamera2D>(entity);

                updateCamera(cam, pos, followCam);

                masterRenderer->queueRegisterCamera(entity->id);
            });

            group->removeOfGroup([](EntitySystem* ecsRef, _unique_id id) {
                LOG_MILE(DOM, "Remove entity " << id << " of ui - camera group !");

                auto ent = ecsRef->getEntity(id);

                if (ent)
                    ecsRef->detach<BaseCamera2D>(ent);
            });
        }

        void updateCamera(BaseCamera2D* cam, PositionComponent* pos, FollowCamera2D* followCam)
        {
            cam->x = pos->x + pos->width * 0.5f - followCam->viewportWidth  * 0.5f;
            cam->y = pos->y + pos->height * 0.5f - followCam->viewportHeight * 0.5f;

            cam->width = followCam->viewportWidth;
            cam->height = followCam->viewportHeight;

            cam->nearPlane = followCam->nearPlane;
            cam->farPlane= followCam->farPlane;
        }

        virtual void onEvent(const EntityChangedEvent& e) override
        {
            // if target entity moved, we want to recalc
            dirtyCameras.insert(e.id);
        }

        virtual void onEvent(const FollowCamera2DChangedEvent& e) override
        {
            dirtyCameras.insert(e.id);
        }

        virtual void execute() override
        {
            if (dirtyCameras.empty()) return;

            for (auto cam : dirtyCameras)
            {
                auto camera = ecsRef->getComponent<BaseCamera2D>(cam);
                auto pos = ecsRef->getComponent<PositionComponent>(cam);
                auto followCam = ecsRef->getComponent<FollowCamera2D>(cam);

                if (camera and pos and followCam)
                {
                    updateCamera(camera, pos, followCam);
                }
            }

            dirtyCameras.clear();
        }

    private:
        std::set<_unique_id> dirtyCameras;

        MasterRenderer *masterRenderer = nullptr;
    };

} // namespace pg
