#pragma once

#include "ECS/system.h"
#include "Scene/scenemanager.h"

namespace pg
{
    struct ProjectData
    {
        std::string path;
        std::string name;

        // Todo
        // std::vector openTabs
        // EditorPreferences
    };

    struct LoadProjectData
    {
        std::string projectPath;
    };

    struct NewProjectData
    {
        std::string projectPath;
        std::string projectName;
    };

    struct ProjectManagerSystem : public System<Listener<LoadProjectData>, StoragePolicy>
    {
        virtual void onEvent(const LoadProjectData& event) override;

        ProjectData currentProject;
    };

    struct ProjectSelectorScene : public Scene
    {
        virtual void init() override;
        virtual void startUp() override;
    };
}