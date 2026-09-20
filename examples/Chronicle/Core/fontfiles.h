#pragma once

#include <map>
#include <string>

namespace chronicle
{
    // A font is identified by the tokens' family id, a numeric weight and italic flag.
    struct FontKey
    {
        std::string family;
        int weight;
        bool italic;

        bool operator<(const FontKey& other) const
        {
            if (family != other.family)
                return family < other.family;
            if (weight != other.weight)
                return weight < other.weight;
            return italic < other.italic;
        }
    };

    // The only place a font file name appears. Paths are relative to the font root.
    inline const std::map<FontKey, std::string>& fontFiles()
    {
        static const std::map<FontKey, std::string> files = {
            {{"display", 600, false}, "CormorantGaramond/CormorantGaramond-SemiBold.ttf"},
            {{"text",    400, false}, "EBGaramond/EBGaramond-Regular.ttf"},
            {{"text",    400, true }, "EBGaramond/EBGaramond-Italic.ttf"},
            {{"text",    500, false}, "EBGaramond/EBGaramond-Medium.ttf"},
            {{"text",    600, false}, "EBGaramond/EBGaramond-SemiBold.ttf"},
        };

        return files;
    }
}
