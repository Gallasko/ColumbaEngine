#include "devscenes.h"

#include "Scene/scenemanager.h"

#include "typespecimen.h"
#include "labelgallery.h"
#include "markgallery.h"
#include "ornamentgallery.h"
#include "panelgallery.h"
#include "buttongallery.h"
#include "tabsglossgallery.h"
#include "progressgallery.h"
#include "statgallery.h"
#include "requirementgallery.h"
#include "prefabfilegallery.h"
#include "clockgallery.h"
#include "activitygallery.h"
#include "windowgallery.h"
#include "ledgergallery.h"
#include "lifescene.h"
#include "loggallery.h"

namespace chronicle
{
    const std::map<std::string, DevSceneLoader>& devScenes()
    {
        static const std::map<std::string, DevSceneLoader> scenes = {
            {"TypeSpecimen", [](pg::SceneElementSystem* s) { s->loadSystemScene<TypeSpecimen>(); }},
            {"LabelGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<LabelGallery>(); }},
            {"MarkGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<MarkGallery>(); }},
            {"OrnamentGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<OrnamentGallery>(); }},
            {"PanelGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<PanelGallery>(); }},
            {"ButtonGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<ButtonGallery>(); }},
            {"TabsGlossGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<TabsGlossGallery>(); }},
            {"ProgressGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<ProgressGallery>(); }},
            {"StatGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<StatGallery>(); }},
            {"RequirementGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<RequirementGallery>(); }},
            {"PrefabFileGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<PrefabFileGallery>(); }},
            {"ClockGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<ClockGallery>(); }},
            {"ActivityGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<ActivityGallery>(); }},
            {"WindowGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<WindowGallery>(); }},
            {"LedgerGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<LedgerGallery>(); }},
            {"LifeScene", [](pg::SceneElementSystem* s) { s->loadSystemScene<LifeScene>(LifeSceneOptions{}); }},
            {"LogGallery", [](pg::SceneElementSystem* s) { s->loadSystemScene<LogGallery>(); }},
        };

        return scenes;
    }

    bool loadDevScene(pg::SceneElementSystem* sceneSystem, const std::string& name)
    {
        auto it = devScenes().find(name);
        if (it == devScenes().end())
            return false;

        it->second(sceneSystem);
        return true;
    }
}
