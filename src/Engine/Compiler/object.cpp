#include "stdafx.h"

#include "object.h"

#include "chunk.h"

namespace pg
{
    Closure::Closure(ObjFunction* func) : function(func)
    {
        upvalues.resize(func->upvalueCount);
    }
}