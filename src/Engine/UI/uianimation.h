// #pragma once

// #include "uiconstant.h"

// #include <vector>

// namespace pg
// {
//     struct UiKeyPoint
//     {
//         UiKeyPoint() { }
//         UiKeyPoint(const UiPosition& pos, const unsigned int& time) : pos(pos), time(time) {}
//         UiKeyPoint(const UiSize& x, const UiSize& y, const UiSize& z, const unsigned int& time) : time(time) { pos.x = x; pos.y = y; pos.z = z; }

//         UiPosition pos;
//         unsigned int time;
//     };

//     struct Sequence // TODO add interpolation support and default interpolation type
//     {
//         //TODO assert that the origin point is not related to the object the sequence is invoked to
//         //because it create an endless loop that crash the app !
//         typedef UiPosition OriginPoint;

//         template <typename... Args>
//         Sequence(OriginPoint origin = OriginPoint(0.0f, 0.0f, 0.0f), Args... args) : origin(origin) { add(args...); }

//         template <typename... Args>
//         Sequence(const UiKeyPoint& point, Args... args) { add(point); add(args...); }

//         //TODO check that the keypoint time is greater than the last when adding in to the Sequence
//         // or sort the whole list accordingly so that the last element is the duration of the sequence
//         // maybe replace the vector by a sorted map ?
//         template <typename... Args>
//         void add(const UiKeyPoint& point, Args... args) { keyPoints.push_back(point), duration = point.time; add(args...); }

//         void add(const UiKeyPoint& point) { keyPoints.push_back(point); duration = point.time; }

//         //void add() { } Add this if i want to allow empty sequences

//         UiPosition getPos(const unsigned int& elapsedTime);

//         OriginPoint origin;
//         std::vector<UiKeyPoint> keyPoints;
//         unsigned int currentIndex = 0;
//         unsigned int duration = 0;
//     };

//     class AnimationComponent
//     {
//     public:
//         static std::vector<AnimationComponent*> runningQueue;

//         template <typename Object>
//         AnimationComponent(Object* obj, Sequence aSeq, bool looping = false) : pos(&(obj->pos)), animationSequence(aSeq), looping(looping) {}

//         inline bool isRunning() const { return running; }

//         inline void setLooping(bool looping) { this->looping = looping; }

//         void start()  { if(pos != nullptr) *pos = animationSequence.getPos(0); elapsedTime = 0; resume(); }
//         void pause()  { running = false; }
//         void resume() { if(!running) runningQueue.push_back(this); running = true; }
//         void stop()   { running = false; }

//         //todo if running is false then cancel the animation and put it in the stopped list
//         void tick(const unsigned int& tickRate);

//     private:
//         UiPosition *pos;
//         Sequence animationSequence;

//         unsigned int elapsedTime = 0;
//         bool running = false;
//         bool looping = false;
//     };
// }