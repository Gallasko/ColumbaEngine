#include "stdafx.h"

#include "clipboard.h"

#include "ECS/entitysystem.h"

#include <mutex>
#include <optional>
#include <vector>

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

namespace pg
{
    namespace
    {
        std::mutex clipboardMutex;
        std::optional<std::string> pendingSet;
        std::vector<_unique_id> pendingGets;

        std::string sdlGet()
        {
            char* text = SDL_GetClipboardText();
            std::string out = text ? text : "";
            SDL_free(text);
            return out;
        }

        void sdlSet(const std::string& text)
        {
            SDL_SetClipboardText(text.c_str());
        }
    }

    void Clipboard::set(const std::string& text)
    {
        std::lock_guard<std::mutex> lock(clipboardMutex);
        pendingSet = text;
    }

    void Clipboard::requestGet(_unique_id requester)
    {
        std::lock_guard<std::mutex> lock(clipboardMutex);
        pendingGets.push_back(requester);
    }

    void Clipboard::service(EntitySystem* ecs, const std::function<std::string()>& get, const std::function<void(const std::string&)>& set)
    {
        std::optional<std::string> toSet;
        std::vector<_unique_id> gets;
        {
            std::lock_guard<std::mutex> lock(clipboardMutex);
            toSet.swap(pendingSet);
            gets.swap(pendingGets);
        }

        if (toSet)
        {
            if (set)
                set(*toSet);
            else
                sdlSet(*toSet);
        }

        if (gets.empty() or not ecs)
            return;

        const std::string text = get ? get() : sdlGet();
        for (_unique_id requester : gets)
            ecs->sendEvent(ClipboardText{requester, text});
    }
}
