#pragma once

#include <functional>
#include <string>

#include "Versioning/manifest.h"

namespace pg
{
    class SaveManager;
    class EntitySystem;

    class VersionManager
    {
    public:
        VersionManager();

        Manifest& getManifest() { return manifest; }
        const Manifest& getManifest() const { return manifest; }

        VersionCheckResult initialize(const std::string& manifestPath, EntitySystem& ecs,
                                      bool autoWipe = true, bool autoMigrate = true);

        const VersionCheckResult& getLastCheckResult() const { return lastResult; }

        void stampVersionInSave(SaveManager& saveManager);

        void wipeSave(EntitySystem& ecs);

        size_t runMigrations(SaveManager& saveManager);

    private:
        Manifest manifest;
        VersionCheckResult lastResult;
    };
}
