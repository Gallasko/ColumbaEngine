#include "search.h"

#include <cctype>

namespace pg
{
    namespace texteditor
    {
        namespace
        {
            std::string lowered(const std::string& s)
            {
                std::string out = s;
                for (char& c : out)
                    if (c >= 'A' and c <= 'Z')
                        c = static_cast<char>(c - 'A' + 'a');
                return out;
            }
        }

        std::vector<Match> findAll(const std::vector<std::string>& lines, const std::string& query)
        {
            std::vector<Match> out;
            if (query.empty())
                return out;
            const std::string q = lowered(query);
            for (size_t l = 0; l < lines.size(); ++l)
            {
                const std::string line = lowered(lines[l]);
                size_t at = line.find(q);
                while (at != std::string::npos)
                {
                    out.push_back({l, at, at + q.size()});
                    at = line.find(q, at + q.size());
                }
            }
            return out;
        }

        std::optional<Match> nextMatch(const std::vector<Match>& matches, TextPos from, bool backwards)
        {
            if (matches.empty())
                return std::nullopt;
            if (backwards)
            {
                for (size_t i = matches.size(); i > 0; --i)
                {
                    const Match& m = matches[i - 1];
                    if (TextPos{m.line, m.start} < from)
                        return m;
                }
                return matches.back();
            }
            for (const Match& m : matches)
                if (from < TextPos{m.line, m.start})
                    return m;
            return matches.front();
        }
    }
}
