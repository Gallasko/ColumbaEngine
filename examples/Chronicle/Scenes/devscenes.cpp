#include "devscenes.h"

#include "Scene/scenemanager.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

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

namespace chronicle
{
    const std::map<std::string, DevSceneLoader>& devScenes()
    {
        static const std::map<std::string, DevSceneLoader> scenes = {
            {"TypeSpecimen", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<TypeSpecimen>(t, st); }},
            {"LabelGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<LabelGallery>(t, st); }},
            {"MarkGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<MarkGallery>(t, st); }},
            {"OrnamentGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<OrnamentGallery>(t, st); }},
            {"PanelGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<PanelGallery>(t, st); }},
            {"ButtonGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<ButtonGallery>(t, st); }},
            {"TabsGlossGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<TabsGlossGallery>(t, st); }},
            {"ProgressGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<ProgressGallery>(t, st); }},
            {"StatGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<StatGallery>(t, st); }},
            {"RequirementGallery", [](pg::SceneElementSystem* s, Tokens* t, TextStyles* st) { s->loadSystemScene<RequirementGallery>(t, st); }},
        };

        return scenes;
    }

    bool loadDevScene(pg::SceneElementSystem* sceneSystem, const std::string& name, Tokens* tokens, TextStyles* styles)
    {
        auto it = devScenes().find(name);
        if (it == devScenes().end())
            return false;

        it->second(sceneSystem, tokens, styles);
        return true;
    }
}
