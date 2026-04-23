#pragma once

#include <functional>
#include <string>

#include "Versioning/manifest.h"

namespace pg
{
    class SaveManager;

    class VersionManager
    {
    public:
        VersionManager();

        Manifest& getManifest() { return manifest; }
        const Manifest& getManifest() const { return manifest; }

        VersionCheckResult initialize(const std::string& manifestPath, SaveManager& saveManager,
                                      bool autoWipe = true, bool autoMigrate = true);

        const VersionCheckResult& getLastCheckResult() const { return lastResult; }

        void stampVersionInSave(SaveManager& saveManager);

        void wipeSave(SaveManager& saveManager);

        size_t runMigrations(SaveManager& saveManager);

    private:
        Manifest manifest;
        VersionCheckResult lastResult;
    };
}
