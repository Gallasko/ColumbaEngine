#include "stdafx.h"
#include "Versioning/versionmanager.h"
#include "ECS/savemanager.h"
#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr const char* DOM = "VersionManager";
    }

    VersionManager::VersionManager() = default;

    VersionCheckResult VersionManager::initialize(
        const std::string& manifestPath, SaveManager& saveManager,
        bool autoWipe, bool autoMigrate)
    {
        LOG_INFO(DOM, "Initializing version manager...");

        manifest.loadFromFile(manifestPath);

        LOG_INFO(DOM, "Current game version: " << manifest.getVersion().toString());

        // Read saved version from the save file
        auto savedVersionElement = saveManager.getValue(manifest_keys::SAVE_VERSION_KEY);
        std::string savedVersionStr = "";

        if (!savedVersionElement.isEmpty() && savedVersionElement.isLitteral())
        {
            savedVersionStr = savedVersionElement.get<std::string>();
        }

        LOG_INFO(DOM, "Saved version: " <<
            (savedVersionStr.empty() ? "(none - new install)" : savedVersionStr));

        lastResult = manifest.checkVersion(savedVersionStr);

        if (lastResult.isMajorBump)
        {
            LOG_WARNING(DOM,
                "Major version bump detected (" << lastResult.oldVersion.toString()
                << " -> " << lastResult.newVersion.toString() << ")");

            if (autoWipe)
            {
                wipeSave(saveManager);
            }
        }
        else if (!lastResult.isSameVersion && !lastResult.isNewInstall && !lastResult.isDowngrade)
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
        saveManager.onProcessEvent(
            SaveElementEvent(manifest_keys::SAVE_VERSION_KEY,
                             manifest.getVersion().toString()));
    }

    void VersionManager::wipeSave(SaveManager& saveManager)
    {
        LOG_WARNING(DOM, "Wiping save data due to major version bump");

        saveManager.clearSaveData();

        stampVersionInSave(saveManager);
        saveManager.forceSave();
    }

    size_t VersionManager::runMigrations(SaveManager& saveManager)
    {
        return manifest.runMigrations(lastResult.oldVersion, saveManager);
    }
}
