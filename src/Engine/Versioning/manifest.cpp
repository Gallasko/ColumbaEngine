#include "stdafx.h"
#include "Versioning/manifest.h"
#include "Helpers/json.hpp"
#include "Files/filemanager.h"
#include "ECS/savemanager.h"
#include "logger.h"

#include <algorithm>

using json = nlohmann::json;

namespace pg
{
    namespace
    {
        static constexpr const char* DOM = "Manifest";
    }

    Manifest::Manifest()
    {
        setDefaults();
    }

    void Manifest::setDefaults()
    {
        version = SemanticVersion(1, 0, 0);
        description = "";
        changelog.clear();
        loaded = false;
    }

    bool Manifest::loadFromFile(const std::string& filepath)
    {
        LOG_INFO(DOM, "Loading manifest from: " << filepath);

        if (!UniversalFileAccessor::exists(filepath))
        {
            LOG_WARNING(DOM,
                "Manifest file not found at '" << filepath
                << "', using default version 1.0.0");
            setDefaults();
            return false;
        }

        auto textFile = UniversalFileAccessor::openTextFile(filepath);

        if (textFile.data.empty())
        {
            LOG_WARNING(DOM,
                "Manifest file is empty at '" << filepath
                << "', using default version 1.0.0");
            setDefaults();
            return false;
        }

        try
        {
            json j = json::parse(textFile.data);

            if (j.contains("version") && j["version"].is_string())
            {
                if (!version.parse(j["version"].get<std::string>()))
                {
                    LOG_WARNING(DOM,
                        "Failed to parse version string: " << j["version"].get<std::string>());
                    setDefaults();
                    return false;
                }
            }

            if (j.contains("description") && j["description"].is_string())
            {
                description = j["description"].get<std::string>();
            }

            if (j.contains("changelog") && j["changelog"].is_array())
            {
                for (const auto& entry : j["changelog"])
                {
                    ChangelogEntry ce;

                    if (entry.contains("version") && entry["version"].is_string())
                    {
                        ce.version = entry["version"].get<std::string>();
                    }

                    if (entry.contains("changes") && entry["changes"].is_array())
                    {
                        for (const auto& change : entry["changes"])
                        {
                            if (change.is_string())
                                ce.changes.push_back(change.get<std::string>());
                        }
                    }

                    changelog.push_back(std::move(ce));
                }
            }

            loaded = true;
            LOG_INFO(DOM,
                "Manifest loaded: version=" << version.toString()
                << ", description=" << description
                << ", changelog entries=" << changelog.size());
            return true;
        }
        catch (const json::exception& e)
        {
            LOG_ERROR(DOM, "JSON parse error in manifest: " << e.what());
            setDefaults();
            return false;
        }
    }

    const ChangelogEntry* Manifest::getChangelogForVersion(const SemanticVersion& ver) const
    {
        std::string verStr = ver.toString();

        for (const auto& entry : changelog)
        {
            if (entry.version == verStr)
                return &entry;
        }

        return nullptr;
    }

    std::vector<ChangelogEntry> Manifest::getChangelogBetween(
        const SemanticVersion& from, const SemanticVersion& to) const
    {
        std::vector<ChangelogEntry> result;

        for (const auto& entry : changelog)
        {
            SemanticVersion entryVer(entry.version);

            if (entryVer > from && entryVer <= to)
            {
                result.push_back(entry);
            }
        }

        std::sort(result.begin(), result.end(),
            [](const ChangelogEntry& a, const ChangelogEntry& b) {
                return SemanticVersion(a.version) < SemanticVersion(b.version);
            });

        return result;
    }

    VersionCheckResult Manifest::checkVersion(const SemanticVersion& savedVersion) const
    {
        VersionCheckResult result;
        result.oldVersion = savedVersion;
        result.newVersion = version;

        if (savedVersion == SemanticVersion(0, 0, 0))
        {
            result.isNewInstall = true;
        }
        else if (version == savedVersion)
        {
            result.isSameVersion = true;
        }
        else if (version > savedVersion)
        {
            result.isMajorBump = version.isMajorBumpFrom(savedVersion);
            result.isMinorBump = version.isMinorBumpFrom(savedVersion);
            result.isPatchBump = version.isPatchBumpFrom(savedVersion);
            result.relevantChangelog = getChangelogBetween(savedVersion, version);
        }
        else
        {
            result.isDowngrade = true;
        }

        return result;
    }

    VersionCheckResult Manifest::checkVersion(const std::string& savedVersionStr) const
    {
        if (savedVersionStr.empty())
        {
            VersionCheckResult result;
            result.oldVersion = SemanticVersion(0, 0, 0);
            result.newVersion = version;
            result.isNewInstall = true;
            return result;
        }

        return checkVersion(SemanticVersion(savedVersionStr));
    }

    void Manifest::registerMigration(const SemanticVersion& targetVersion, MigrationCallback callback)
    {
        migrations[targetVersion].push_back(std::move(callback));
    }

    void Manifest::registerMigration(const std::string& targetVersionStr, MigrationCallback callback)
    {
        registerMigration(SemanticVersion(targetVersionStr), std::move(callback));
    }

    size_t Manifest::runMigrations(const SemanticVersion& oldVersion, SaveManager& saveManager) const
    {
        size_t count = 0;

        for (const auto& [targetVersion, callbacks] : migrations)
        {
            if (targetVersion > oldVersion && targetVersion <= version)
            {
                LOG_INFO(DOM,
                    "Running " << callbacks.size() << " migration(s) for version "
                    << targetVersion.toString());

                for (const auto& callback : callbacks)
                {
                    callback(saveManager);
                    ++count;
                }
            }
        }

        return count;
    }
}
