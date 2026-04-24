#pragma once

#include <string>
#include <sstream>

namespace pg
{
    struct SemanticVersion
    {
        int major = 1;
        int minor = 0;
        int patch = 0;

        SemanticVersion() = default;
        SemanticVersion(int major, int minor, int patch) : major(major), minor(minor), patch(patch) {}

        explicit SemanticVersion(const std::string& versionString)
        {
            parse(versionString);
        }

        bool parse(const std::string& versionString)
        {
            char dot1, dot2;
            std::istringstream ss(versionString);

            if (ss >> major >> dot1 >> minor >> dot2 >> patch and
                dot1 == '.' and dot2 == '.')
            {
                return true;
            }

            // Fallback to defaults on parse failure
            major = 1; minor = 0; patch = 0;

            return false;
        }

        std::string toString() const
        {
            return std::to_string(major) + "." +
                   std::to_string(minor) + "." +
                   std::to_string(patch);
        }

        bool operator==(const SemanticVersion& other) const
        {
            return major == other.major and
                   minor == other.minor and
                   patch == other.patch;
        }

        bool operator!=(const SemanticVersion& other) const { return !(*this == other); }

        bool operator<(const SemanticVersion& other) const
        {
            if (major != other.major)
                return major < other.major;

            if (minor != other.minor)
                return minor < other.minor;

            return patch < other.patch;
        }

        bool operator>(const SemanticVersion& other) const  { return other < *this; }
        bool operator<=(const SemanticVersion& other) const { return !(other < *this); }
        bool operator>=(const SemanticVersion& other) const { return !(*this < other); }

        bool isMajorBumpFrom(const SemanticVersion& oldVersion) const
        {
            return major > oldVersion.major;
        }

        bool isMinorBumpFrom(const SemanticVersion& oldVersion) const
        {
            return major == oldVersion.major and minor > oldVersion.minor;
        }

        bool isPatchBumpFrom(const SemanticVersion& oldVersion) const
        {
            return major == oldVersion.major and
                   minor == oldVersion.minor and
                   patch > oldVersion.patch;
        }
    };
}
