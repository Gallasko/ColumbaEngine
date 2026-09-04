#pragma once

#include <string>

namespace pg
{
    /**
     * Demangle a C++ mangled type name (as returned by typeid().name())
     * and strip all "pg::" namespace prefixes.
     *
     * On compilers without abi::__cxa_demangle the mangled name is
     * returned as-is (minus the "pg::" prefixes).
     */
    std::string prettyTypeName(const char* mangled);
}
