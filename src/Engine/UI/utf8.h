#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pg
{
    namespace utf8
    {
        /// Decodes a UTF-8 string into Unicode code points. Invalid or truncated sequences yield U+FFFD.
        inline std::vector<uint32_t> decode(const std::string& text)
        {
            std::vector<uint32_t> codepoints;
            codepoints.reserve(text.size());

            const size_t n = text.size();
            size_t i = 0;

            while (i < n)
            {
                const unsigned char lead = static_cast<unsigned char>(text[i]);

                uint32_t cp = 0;
                int extra = 0;

                if (lead < 0x80)
                {
                    cp = lead;
                    extra = 0;
                }
                else if ((lead & 0xE0) == 0xC0)
                {
                    cp = lead & 0x1F;
                    extra = 1;
                }
                else if ((lead & 0xF0) == 0xE0)
                {
                    cp = lead & 0x0F;
                    extra = 2;
                }
                else if ((lead & 0xF8) == 0xF0)
                {
                    cp = lead & 0x07;
                    extra = 3;
                }
                else
                {
                    codepoints.push_back(0xFFFD);
                    ++i;
                    continue;
                }

                if (i + static_cast<size_t>(extra) >= n)
                {
                    codepoints.push_back(0xFFFD);
                    ++i;
                    continue;
                }

                bool valid = true;
                for (int k = 1; k <= extra; ++k)
                {
                    const unsigned char continuation = static_cast<unsigned char>(text[i + k]);
                    if ((continuation & 0xC0) != 0x80)
                    {
                        valid = false;
                        break;
                    }
                    cp = (cp << 6) | (continuation & 0x3F);
                }

                if (not valid)
                {
                    codepoints.push_back(0xFFFD);
                    ++i;
                    continue;
                }

                codepoints.push_back(cp);
                i += static_cast<size_t>(extra) + 1;
            }

            return codepoints;
        }
    }
}
