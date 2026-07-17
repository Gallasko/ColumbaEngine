#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>

#include "Versioning/semanticversion.h"

namespace pg
{
    class SaveManager;

    struct ChangelogEntry
    {
        std::string version;
        std::vector<std::string> changes;
    };

    struct VersionCheckResult
    {
        SemanticVersion oldVersion;
        SemanticVersion newVersion;

        bool isNewInstall = false;
        bool isMajorBump = false;
        bool isMinorBump = false;
        bool isPatchBump = false;
        bool isSameVersion = false;
        bool isDowngrade = false;

        std::vector<ChangelogEntry> relevantChangelog;
    };

    using MigrationCallback = std::function<void(SaveManager&)>;

    class Manifest
    {
    public:
        Manifest();

        bool loadFromFile(const std::string& filepath);

        const SemanticVersion& getVersion() const { return version; }
        const std::string& getDescription() const { return description; }
        const std::vector<ChangelogEntry>& getChangelog() const { return changelog; }
        bool isLoaded() const { return loaded; }

        const ChangelogEntry* getChangelogForVersion(const SemanticVersion& ver) const;

        std::vector<ChangelogEntry> getChangelogBetween(
            const SemanticVersion& from, const SemanticVersion& to) const;

        VersionCheckResult checkVersion(const SemanticVersion& savedVersion) const;
        VersionCheckResult checkVersion(const std::string& savedVersionStr) const;

        void registerMigration(const SemanticVersion& targetVersion, MigrationCallback callback);
        void registerMigration(const std::string& targetVersionStr, MigrationCallback callback);

        size_t runMigrations(const SemanticVersion& oldVersion, SaveManager& saveManager) const;

    private:
        void setDefaults();

        SemanticVersion version;
        std::string description;
        std::vector<ChangelogEntry> changelog;
        bool loaded = false;

        std::map<SemanticVersion, std::vector<MigrationCallback>> migrations;
    };

    namespace manifest_keys
    {
        inline constexpr const char* SAVE_VERSION_KEY = "__pg_game_version";
    }
}
