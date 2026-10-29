#pragma once

#include <string>

namespace pg
{
    namespace test
    {
        // The rule scripts compiled once for this test process, on the first call (maintest.cc): a
        // folder of .pgc of its own, so a fixture reads them in a few ms instead of compiling every
        // script again, and two test processes never write the same .pgc. If a script does not
        // compile, every call fails its test with the compiler's message and the folder is gone,
        // so no fixture can read a stale .pgc.
        const std::string& compiledRules();
    }
}
