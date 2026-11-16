#pragma once

#include "textbuffer.h"

#include <optional>
#include <string>
#include <vector>

namespace pg
{
    namespace texteditor
    {
        struct Match
        {
            size_t line = 0;
            size_t start = 0;
            size_t end = 0;
        };

        // Every occurrence of `query` in the lines, ignoring ASCII case, in order; none for ""
        std::vector<Match> findAll(const std::vector<std::string>& lines, const std::string& query);

        // The match after `from` (or before it, backwards), wrapping around; none when there is none
        std::optional<Match> nextMatch(const std::vector<Match>& matches, TextPos from, bool backwards);
    }
}
