#pragma once

#include <string>

namespace pg
{
    class EntitySystem;
    class Window;

    /**
     * @brief Create the in-engine visual profiler overlay.
     *
     * Registers a ProfilerOverlaySystem into the ECS (and a TTFTextSystem
     * with the given font if none exists yet). Must be called during setup,
     * before the ECS is started.
     *
     * Controls: F10 show/hide, F11 pause/resume capture view, F12 export CSV,
     * Left/Right scrub the timeline, +/- zoom, click the timeline to center.
     *
     * No-op in builds without -DPROFILE (PG_PROFILE=OFF).
     */
    void createProfilerOverlay(EntitySystem& ecs, Window& window,
                               const std::string& fontPath = "res/font/Inter/static/Inter_28pt-Light.ttf");
}
