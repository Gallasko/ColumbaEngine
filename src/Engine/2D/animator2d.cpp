#include "stdafx.h"

#include "animator2d.h"

#include "Loaders/Aseprite/asepriteloader.h"

namespace pg
{
    std::vector<Animation2DKeyPoint> getAnimationKeypoint(const std::vector<AsepriteFrame>& frames)
    {
        std::vector<Animation2DKeyPoint> keypoints;

        size_t cumulativeAnimationDuration = 0;

        for (const auto& anim : frames)
        {
            keypoints.push_back({cumulativeAnimationDuration, {anim.textureName, 1}});

            cumulativeAnimationDuration += anim.durationInMilliseconds;
        }

        keypoints.push_back({cumulativeAnimationDuration, {frames.back().textureName, 1}});

        return keypoints;
    }

    Texture2DAnimationComponent::Texture2DAnimationComponent(const std::vector<AsepriteFrame>& keypoints, bool runningOnStartup, bool loop) : running(runningOnStartup), looping(loop)
    {
        this->keypoints = getAnimationKeypoint(keypoints);
    }

    void Texture2DAnimatorSystem::onProcessEvent(const ChangeAsepriteTexture2DAnimationEvent& event)
    {
        auto ent = ecsRef->getEntity(event.id);

        auto sys = ecsRef->getSystem<AsepriteLoader>();

        if (not ent or not ent->has<Texture2DAnimationComponent>() or not sys)
            return;

        auto texAnim = ent->get<Texture2DAnimationComponent>();

        auto name = event.animName;

        size_t pos = name.find('/');

        if (pos != std::string::npos)
        {
            std::string path = name.substr(0, pos);      // "before"
            std::string animName = name.substr(pos + 1); // "after"

            auto& frames = sys->getAnimationFrames(path, animName);

            auto keypoints = getAnimationKeypoint(frames);

            // Todo maybe set the texture to the first frame right away
            // sys->getFirstFrame(path);

            texAnim->keypoints = keypoints;
            texAnim->startId = -1;
            texAnim->elapsedTime = 0;
        }
        else
        {
            LOG_ERROR("Texture2DAnimatorSystem", "Invalid Aseprite animation name format: " << name << ", expected format: <path>/<animName>");
        }
    }
}