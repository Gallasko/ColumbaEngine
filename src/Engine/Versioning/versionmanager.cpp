#include "stdafx.h"

#include "Versioning/versionmanager.h"
#include "ECS/savemanager.h"
#include "ECS/entitysystem.h"
#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr const char* DOM = "VersionManager";
    }

    VersionManager::VersionManager() = default;

    VersionCheckResult VersionManager::initialize(const std::string& manifestPath, EntitySystem& ecs, bool autoWipe, bool autoMigrate)
    {
        LOG_INFO(DOM, "Initializing version manager...");

        manifest.loadFromFile(manifestPath);

        LOG_INFO(DOM, "Current game version: " << manifest.getVersion().toString());

        auto& saveManager = ecs.getSaveManager();

        // Read saved version from the save file
        auto savedVersionElement = saveManager.getValue(manifest_keys::SAVE_VERSION_KEY);
        std::string savedVersionStr = "";

        if (not savedVersionElement.isEmpty() and savedVersionElement.isLitteral())
        {
            savedVersionStr = savedVersionElement.get<std::string>();
        }

        LOG_INFO(DOM, "Saved version: " << (savedVersionStr.empty() ? "(none - new install)" : savedVersionStr));

        lastResult = manifest.checkVersion(savedVersionStr);

        if (lastResult.isMajorBump)
        {
            LOG_WARNING(DOM, "Major version bump detected (" << lastResult.oldVersion.toString() <<
                " -> " << lastResult.newVersion.toString() << ")");

            if (autoWipe)
            {
                wipeSave(ecs);
            }
        }
        else if (not lastResult.isSameVersion and not lastResult.isNewInstall and not lastResult.isDowngrade)
        {
            if (autoMigrate)
            {
                size_t migrationsRun = runMigrations(saveManager);
                LOG_INFO(DOM, "Ran " << migrationsRun << " migration(s)");
            }
        }

        stampVersionInSave(saveManager);

        return lastResult;
    }

    void VersionManager::stampVersionInSave(SaveManager& saveManager)
    {
        saveManager.onProcessEvent(SaveElementEvent(manifest_keys::SAVE_VERSION_KEY, manifest.getVersion().toString()));
    }

    void VersionManager::wipeSave(EntitySystem& ecs)
    {
        LOG_WARNING(DOM, "Wiping save data due to major version bump");

        // Clear both the simple key-value save data AND system serialized data
        ecs.clearAllSaveData();

        stampVersionInSave(ecs.getSaveManager());
        ecs.getSaveManager().forceSave();
    }

    size_t VersionManager::runMigrations(SaveManager& saveManager)
    {
        return manifest.runMigrations(lastResult.oldVersion, saveManager);
    }
}
