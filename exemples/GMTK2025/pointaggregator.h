#pragma once

#include "window.h"

#include "Systems/basicsystems.h"
#include "Systems/tween.h"

#include "UI/ttftext.h"

#include "2D/simple2dobject.h"

#include "2D/position.h"

#include "ribbonmesh.h"
#include "polygonmesh.h"
#include "enemy.h"

namespace pg {

    bool doSegmentsIntersect(const pg::Segment2D& s1, const pg::Segment2D& s2)
    {
        auto orientation = [](const pg::Point2D& a, const pg::Point2D& b, const pg::Point2D& c) {
            float val = (b.y - a.y) * (c.x - b.x) - 
                        (b.x - a.x) * (c.y - b.y);
            if (std::abs(val) < 1e-6f)
                return 0;  // colinear

            return (val > 0) ? 1 : 2;             // clockwise or counterclockwise
        };

        auto onSegment = [](const pg::Point2D& p, const pg::Point2D& q, const pg::Point2D& r) {
            return q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
                q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y);
        };

        const pg::Point2D& p1 = s1.start;
        const pg::Point2D& q1 = s1.end;
        const pg::Point2D& p2 = s2.start;
        const pg::Point2D& q2 = s2.end;

        int o1 = orientation(p1, q1, p2);
        int o2 = orientation(p1, q1, q2);
        int o3 = orientation(p2, q2, p1);
        int o4 = orientation(p2, q2, q1);

        // General case
        if (o1 != o2 && o3 != o4)
            return true;

        // Special cases (colinear overlaps)
        if (o1 == 0 && onSegment(p1, p2, q1)) return true;
        if (o2 == 0 && onSegment(p1, q2, q1)) return true;
        if (o3 == 0 && onSegment(p2, p1, q2)) return true;
        if (o4 == 0 && onSegment(p2, q1, q2)) return true;

        return false;
    }

    bool checkSegmentIntersectWithLoop(const Segment2D& segment, const std::vector<pg::Point2D>& path)
    {
        if (path.size() < 2)
            return false;

        for (size_t i = 0; i + 1 < path.size(); ++i)
        {
            pg::Segment2D loopSegment(path[i], path[i + 1]);

            if (doSegmentsIntersect(segment, loopSegment))
            {
                return true;
            }
        }

        return false;
    }

    std::optional<std::pair<size_t, size_t>> findLoopSegment(const std::vector<pg::Point2D>& path)
    {
        if (path.size() < 4) return std::nullopt;

        // Only check the latest segment against previous ones
        pg::Segment2D newSegment(path[path.size() - 2], path[path.size() - 1]);

        for (size_t i = 0; i + 2 < path.size() - 1; ++i) { // avoid checking with adjacent segments
            pg::Segment2D oldSegment(path[i], path[i + 1]);

            if (doSegmentsIntersect(newSegment, oldSegment)) {
                return std::make_pair(i, path.size() - 2); // intersection between [i, i+1] and last segment
            }
        }

        return std::nullopt; // No loop detected
    }

    //— 1. Cross‐product helper for 2D vectors
    static float cross(const Point2D& a, const Point2D& b) {
        return a.x * b.y - a.y * b.x;
    }

    //— 2. Compute intersection point of two segments (assumed to intersect)
    std::optional<Point2D> segmentIntersectionPoint(const Segment2D& s1,
                                                    const Segment2D& s2)
    {
        Point2D p = s1.start;
        Point2D r = Point2D(s1.end.x - s1.start.x, s1.end.y - s1.start.y);
        Point2D q = s2.start;
        Point2D s = Point2D(s2.end.x - s2.start.x, s2.end.y - s2.start.y);

        float rxs = cross(r, s);
        if (std::abs(rxs) < 1e-6f)
            return std::nullopt; // parallel or colinear

        Point2D qp = Point2D(q.x - p.x, q.y - p.y);
        float t = cross(qp, s) / rxs;
        // float u = cross(qp, r) / rxs; // could check 0<=u<=1

        return Point2D(p.x + t * r.x, p.y + t * r.y);
    }

    //— 3. Extract the loop polygon between indices [i+1 … j] plus the intersection
    std::vector<Point2D> extractLoopPolygon(const std::vector<Point2D>& path,
                                            size_t i, size_t j,
                                            const Point2D& ip)
    {
        assert(i + 1 < path.size() && j + 1 < path.size());
        // The loop polygon goes: ip → path[i+1] → … → path[j] → ip
        std::vector<Point2D> poly;
        poly.reserve((j - i) + 2);
        poly.push_back(ip);
        for (size_t k = i+1; k <= j; ++k)
            poly.push_back(path[k]);
        poly.push_back(ip);
        return poly;
    }

    //— 4. Build the clipped path: prefix [0…i], ip, suffix [j+1…end]
    std::vector<Point2D> buildClippedPath(const std::vector<Point2D>& path,
                                        size_t i, size_t j,
                                        const Point2D& ip)
    {
        std::vector<Point2D> out;
        out.reserve(i + 1 + (path.size() - (j+1)) + 1);
        // prefix up to i
        for (size_t k = 0; k <= i; ++k)
            out.push_back(path[k]);
        // intersection point
        out.push_back(ip);
        // suffix from j+1 to end
        for (size_t k = j+1; k < path.size(); ++k)
            out.push_back(path[k]);
        return out;
    }

    //— 5. High-level: detect loop, compute both results
    struct LoopResult {
        size_t segStart;               // i
        size_t segEnd;                 // j
        Point2D intersection;
        std::vector<Point2D> polygon;  // closed loop poly
        std::vector<Point2D> clipped;  // the new path
    };

    std::optional<LoopResult> detectAndClipLoop(const std::vector<Point2D>& path)
    {
        auto loopPair = findLoopSegment(path);
        if (!loopPair) return std::nullopt;
        size_t i = loopPair->first;    // old segment index
        size_t j = loopPair->second;   // new segment index

        Segment2D s1(path[i],   path[i+1]);
        Segment2D s2(path[j],   path[j+1]);

        auto ipOpt = segmentIntersectionPoint(s1, s2);
        if (!ipOpt) return std::nullopt;

        Point2D ip = *ipOpt;
        LoopResult result;
        result.segStart    = i;
        result.segEnd      = j;
        result.intersection = ip;
        result.polygon     = extractLoopPolygon(path, i, j, ip);
        result.clipped     = buildClippedPath(path, i, j, ip);
        return result;
    }

    // Todo add this event in the main ecs
    struct RemoveEntityEvent
    {
        RemoveEntityEvent(_unique_id prefabId) : prefabId(prefabId) {}
        RemoveEntityEvent(const RemoveEntityEvent& rhs) : prefabId(rhs.prefabId) {}

        RemoveEntityEvent& operator=(const RemoveEntityEvent& rhs)
        {
            prefabId = rhs.prefabId;
            return *this;
        }

        _unique_id prefabId;
    };

    struct EnemyLoopHitEvent {};

    // Todo add an integer to Listener and QueuedListener (Listener<Event, N>) with the N being the number of cycle or time
    // that we need to wait before the event becomes triggerable again.

    struct PointAggregator : public System<InitSys,
        Listener<OnMouseMove>, Listener<OnMouseClick>, Listener<OnMouseRelease>, Listener<TickEvent>, QueuedListener<RemoveEntityEvent>,
        Listener<EnemyLoopHitEvent>>
    {
        virtual void init() override
        {
            LOG_THIS_MEMBER("PointAggregator");

            registerGroup<PositionComponent, EnemyFlag>();

            auto text = makeTTFText(ecsRef, 340, 12, 1, "light", "30 : 00", 1.0f, {255.0f, 255.0f, 255.0f, 255.0f});

            timer = text.get<TTFText>();
        }

        virtual void onEvent(const OnMouseMove& event) override
        {
            if (pressed)
            {
                LOG_INFO("PointAggregator", "Mouse moved");

                currentMousePos = event.pos;
            }
        }

        virtual void onEvent(const OnMouseClick& event) override
        {
            LOG_INFO("PointAggregator", "Mouse clicked");

            if (event.button == SDL_BUTTON_LEFT)
            {
                tick = 0.0f;

                pressed = true;

                currentMousePos = event.pos;

                mousePosList.emplace_back(event.pos);

                auto ent = ecsRef->createEntity();

                ent.attach<RibbonComponent>("cursor", mousePosList, 10.0f, true, 1.0f);

                currentEnt = ent;
            }

            if (event.button == SDL_BUTTON_RIGHT)
            {
                LOG_INFO("PointAggregator", "Right click detected, creating test enemy");

                auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 50.0f, 50.0f, {0.0f, 0.0f, 255.0f, 255.0f});

                shape.get<PositionComponent>()->setX(event.pos.x - 25.0f);
                shape.get<PositionComponent>()->setY(event.pos.y - 25.0f);

                shape.attach<EnemyFlag>();
            }
        }

        virtual void onEvent(const OnMouseRelease& event) override
        {
            if (event.button == SDL_BUTTON_LEFT)
            {
                pressed = false;

                for (const auto& pos : mousePosList)
                {
                    LOG_INFO("PointAggregator", "-> Stored Mouse position: " << pos.x << ", " << pos.y);
                }

                mousePosList.clear();

                ecsRef->removeEntity(currentEnt);
            }
        }

        // Todo make this a base event that can be used by any system
        virtual void onProcessEvent(const RemoveEntityEvent& event) override
        {
            ecsRef->removeEntity(event.prefabId);
        }

        virtual void onEvent(const EnemyLoopHitEvent& event) override
        {
            LOG_INFO("PointAggregator", "Enemy hit the looped, processing event");
            // Handle the enemy loop hit event here if needed

            mousePosList = std::vector<Point2D>{ currentMousePos };

            if (currentEnt and currentEnt.has<RibbonComponent>())
            {
                currentEnt.get<RibbonComponent>()->setPath(mousePosList);
            }
            
            auto y = currentMousePos.y;
            auto popText = makeTTFText(ecsRef, currentMousePos.x, y - 10, 1, "light", "-2s", .4f, {255.0f, 0.0f, 0.0f, 255.0f});

            popText.attach<TweenComponent>(TweenComponent {
                0.0f, // Start opacity
                50.0f, // End opacity
                600.0f, // Duration in milliseconds
                [y, popText](const TweenValue& value) {
                    popText.get<PositionComponent>()->setY(y - 10 - std::get<float>(value));
                },
                makeCallable<RemoveEntityEvent>(popText.entity.id)
            });

            timerTime -= 2000.0f; // Decrease the timer by 2 seconds

            auto ent = ecsRef->createEntity();

            ecsRef->attach<TweenComponent>(ent, TweenComponent {
                0.0f,
                255.0f,
                500.0f, // Duration in milliseconds
                [this](const TweenValue& value) {
                    auto v = std::get<float>(value);
                    timer->setColor({255.0f, v, v, v}); },
                makeCallable<RemoveEntityEvent>(ent.id)
            });

            updateTimer();
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;

            if (pressed)
            {
                tick += event.tick;

                if (tick >= 40 and currentMousePos != mousePosList.back())
                {
                    tick = 0.0f;
                    LOG_INFO("PointAggregator", "Mouse moved to: " << currentMousePos.x << ", " << currentMousePos.y);
                    mousePosList.emplace_back(currentMousePos);

                    if (auto lr = detectAndClipLoop(mousePosList))
                    {
                        // The enclosed polygon:
                        auto polygon = lr->polygon;

                        auto ent = ecsRef->createEntity();
                        ent->attach<PolygonComponent>(polygon, 0);

                        ecsRef->attach<TweenComponent>(ent, TweenComponent {
                            1.0f, // Fully opaque
                            0.0f, // Fully transparent
                            400.0f, // Duration in milliseconds
                            [ent](const TweenValue& value) { ent.get<PolygonComponent>()->setOpacity(std::get<float>(value)); },
                            makeCallable<RemoveEntityEvent>(ent.id)
                        });

                        checkEnemyInLoop(polygon);

                        // The new, clipped path:
                        auto newPath = lr->clipped;       

                        // Now update your RibbonComponent:
                        // Todo do this if you want the clipped path to be used
                        // mousePosList = newPath;
                        // else if you just reset the whole path
                        mousePosList = std::vector<Point2D>{ currentMousePos };

                        // And (optionally) handle the polygon for game logic.
                    }

                    currentEnt.get<RibbonComponent>()->setPath(mousePosList);
                }
            }
        }

        void checkEnemyInLoop(const std::vector<Point2D>& loop)
        {
            int nbEnemiesCollected = 0;
            for (const auto& ent : viewGroup<PositionComponent, EnemyFlag>())
            {
                auto pos = ent->get<PositionComponent>();

                // Only check if the center of the enemy is inside the loop ( we don't check for the whole BB to avoid performance issues )
                if (pos and pointInPolygon(loop, Point2D(pos->x + 25, pos->y + 25)))
                {
                    nbEnemiesCollected++;

                    LOG_INFO("PointAggregator", "Enemy found in loop at: " << pos->x << ", " << pos->y);
                    // Do something with the enemy, like removing it or marking it
                    ecsRef->removeEntity(ent->entityId);

                    auto y = pos->y + 25 - 10;
                    auto popText = makeTTFText(ecsRef, pos->x + 25, y, 1, "light", "+" + std::to_string(nbEnemiesCollected) + "s", .4f, {0.0f, 255.0f, 0.0f, 255.0f});

                    popText.attach<TweenComponent>(TweenComponent {
                        0.0f, // Start opacity
                        50.0f, // End opacity
                        750.0f, // Duration in milliseconds
                        [y, popText](const TweenValue& value) {
                            popText.get<PositionComponent>()->setY(y - std::get<float>(value));
                        },
                        makeCallable<RemoveEntityEvent>(popText.entity.id)
                    });

                    timerTime += nbEnemiesCollected * 1000.0f; // Increase the timer by 1 second
                }
            }
        }

        virtual void execute() override
        {
            if (timerTime > 0.0f)
            {
                timerTime -= deltaTime;

                if (timerTime <= 0.0f)
                {
                    // lose condition
                }

                updateTimer();
            }

            deltaTime = 0.0f;
        }

        inline void updateTimer()
        {
            timer->setText(std::to_string(static_cast<int>(timerTime / 1000.0f)) + " : " + std::to_string((static_cast<int>(timerTime) % 1000) / 10));
        }

        bool pressed = false;

        float tick = 0.0f;

        Point2D currentMousePos;

        std::vector<Point2D> mousePosList;

        EntityRef currentEnt;

        float deltaTime = 0.0f;

        float timerTime = 30000.0f; // 30 seconds
        CompRef<TTFText> timer;
    };
}