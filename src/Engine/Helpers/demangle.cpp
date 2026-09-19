#include "demangle.h"

#include <cstdlib>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

namespace pg
{
    std::string prettyTypeName(const char* mangled)
    {
#if defined(__GNUC__) || defined(__clang__)
        int status = 0;
        char* buf = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
        std::string s = (status == 0 && buf) ? buf : mangled;
        if (buf) free(buf);
#else
        std::string s = mangled;
#endif
        std::string clean;
        for (size_t i = 0; i < s.size(); )
        {
            if (s.compare(i, 4, "pg::") == 0) i += 4;
            else clean += s[i++];
        }
        return clean;
    }
}
