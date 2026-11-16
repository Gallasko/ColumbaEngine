#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace pg
{
    class EntitySystem;

    using _unique_id = uint64_t;

    // The system clipboard's text, for the one who asked (Clipboard::requestGet)
    struct ClipboardText
    {
        _unique_id requester;
        std::string text;
    };

    /**
     * The system clipboard, reached from any thread. SDL wants its clipboard used on the video
     * thread while systems run on worker threads: a system queues a set or a get here, and the
     * window's frame (main thread) performs them in `service`, sending a get's text back as a
     * ClipboardText event.
     */
    class Clipboard
    {
    public:
        static void set(const std::string& text);
        static void requestGet(_unique_id requester);

        // On the main thread, once a frame. `get` and `set` default to SDL's clipboard
        static void service(EntitySystem* ecs, const std::function<std::string()>& get = {}, const std::function<void(const std::string&)>& set = {});
    };
}
