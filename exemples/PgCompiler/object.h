#pragma once

#include <string>

#include "chunk.h"

namespace pg
{
    struct ObjFunction
    {
        Chunk chunk;
        int arity; // Number of parameters
        std::string name;
    };
}