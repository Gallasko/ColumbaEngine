#include "stdafx.h"

#include "uianimation.h"

namespace pg
{
    UiPosition Sequence::getPos(const unsigned int& elapsedTime)
    {
        auto keyPointsVecSize = keyPoints.size();

        if(keyPointsVecSize <= 0) // the sequence is empty <- currently it should be impossible
            return UiPosition();

        // TODO Make sure that current index is always in bound
        if(keyPoints[currentIndex].time == elapsedTime)
        {
            return origin + keyPoints[currentIndex].pos;
            //return keyPoints[currentIndex].pos;
        }

        if(keyPoints[currentIndex].time < elapsedTime)
        {
            auto nextIndex = currentIndex + 1;

            if(nextIndex >= keyPointsVecSize) // this means that currentIndex is the pointing to the last element of the sequence
                //return keyPoints[currentIndex].pos;
                return origin + keyPoints[currentIndex].pos;

            if(keyPoints[nextIndex].time > elapsedTime) // TODO here we should interpolate the value between cIndex and nIndex
            {
                //This is the linear interpolation
                float delta = (elapsedTime - keyPoints[currentIndex].time) / static_cast<float>(keyPoints[nextIndex].time - keyPoints[currentIndex].time);
                float dx = keyPoints[nextIndex].pos.x - keyPoints[currentIndex].pos.x;
                float dy = keyPoints[nextIndex].pos.y - keyPoints[currentIndex].pos.y;
                float dz = keyPoints[nextIndex].pos.z - keyPoints[currentIndex].pos.z;

                UiPosition pos;

                pos.x = keyPoints[currentIndex].pos.x + (dx * delta);
                pos.y = keyPoints[currentIndex].pos.y + (dy * delta);
                pos.z = keyPoints[currentIndex].pos.z + (dz * delta);

                //TODO add support for other type of interpolation and interpolation registering

                return origin + pos;
                //return pos;
            }
            else if(keyPoints[nextIndex].time <= elapsedTime)
            {
                currentIndex = nextIndex;
                return getPos(elapsedTime);
            }

            //currentIndex = nextIndex;
            //return getPos(elapsedTime);
        }
        else if(keyPoints[currentIndex].time >= elapsedTime)
        {
            //TODO check if this is really necessary
            if(currentIndex == 0)
                return UiPosition();

            currentIndex -= 1;
            return getPos(elapsedTime);
        }

        // Todo Error should never arrive here
        return UiPosition();
    }

    std::vector<AnimationComponent*> AnimationComponent::runningQueue;

    void AnimationComponent::tick(const unsigned int& tickRate)
    {
        elapsedTime += tickRate;

        if(elapsedTime > animationSequence.duration && !looping)
            running = false;
        else if (elapsedTime > animationSequence.duration && looping)
            elapsedTime = animationSequence.duration - elapsedTime;

        if(pos != nullptr)
            *pos = animationSequence.getPos(elapsedTime);
    }
}