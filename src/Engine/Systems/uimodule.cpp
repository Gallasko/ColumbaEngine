#include "stdafx.h"

#include "uimodule.h"

namespace pg
{
    std::vector<SysListElement> uiElementFunctionsList(const Function *caller, EntitySystem *ecsRef, CompRef<UiComponent> comp)
    {
        return {{"id", comp.entityId},
                {"x", makeFun<GetX>(caller, "getX", ecsRef, comp)},
                {"y", makeFun<GetY>(caller, "getY", ecsRef, comp)},
                {"z", makeFun<GetZ>(caller, "getZ", ecsRef, comp)},
                {"w", makeFun<GetWidth>(caller, "getWidth", ecsRef, comp)},
                {"h", makeFun<GetHeight>(caller, "getHeight", ecsRef, comp)},
                {"setVisibility", makeFun<SetVisible>(caller, "setVisibility", ecsRef, comp)},
                {"setX", makeFun<SetX>(caller, "setX", ecsRef, comp)},
                {"setY", makeFun<SetY>(caller, "setY", ecsRef, comp)},
                {"setZ", makeFun<SetY>(caller, "setZ", ecsRef, comp)},
                {"setWidth", makeFun<SetW>(caller, "setWidth", ecsRef, comp)},
                {"setHeight", makeFun<SetH>(caller, "setHeight", ecsRef, comp)}};
    }
}