#pragma once

#include <functional>
#include <map>
#include <string>

namespace pg { class SceneElementSystem; }

namespace chronicle
{
    class Tokens;
    class TextStyles;

    using DevSceneLoader = std::function<void(pg::SceneElementSystem*, Tokens*, TextStyles*)>;

    // name -> loader; std::map so an unknown --dev lists the known names sorted.
    const std::map<std::string, DevSceneLoader>& devScenes();

    // Loads the named scene; returns false if the name is unknown.
    bool loadDevScene(pg::SceneElementSystem*, const std::string& name, Tokens*, TextStyles*);
}
