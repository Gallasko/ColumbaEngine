#include "projectmanager.h"

#include "UI/ttftext.h"

namespace pg
{
    namespace
    {

    }

    void ProjectManagerSystem::onEvent(const LoadProjectData& event)
    {

    }

    void ProjectSelectorScene::init()
    {
        makeTTFText(this, 80, 100, "res/font/Inter/static/Inter_28pt-Bold.ttf", "PG Engine", 1);

        makeTTFText(this, 80, 175, "res/font/Inter/static/Inter_28pt-Light.ttf", "Start:", 0.4);

        makeTTFText(this, 100, 220, "res/font/Inter/static/Inter_28pt-Italic.ttf", "Create Project...", 0.5);
        makeTTFText(this, 100, 250, "res/font/Inter/static/Inter_28pt-Italic.ttf", "Open Project...", 0.5);

        makeTTFText(this, 80, 300, "res/font/Inter/static/Inter_28pt-Light.ttf", "Recent:", 0.4);
        // Todo
        makeTTFText(this, 100, 350, "res/font/Inter/static/Inter_28pt-Italic.ttf", "res/a.pg", 0.5);

    }

    void ProjectSelectorScene::startUp()
    {

    }
}