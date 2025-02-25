#include "positionmodule.h"

namespace pg
{
    std::vector<SysListElement> uiElementFunctionsList(const Function *caller, EntitySystem *ecsRef, CompRef<PositionComponent> comp)
    {
        return {{"id", comp.entityId},
                {"x", makeFun<PosGetX>(caller, "getX", ecsRef, comp)},
                {"z", makeFun<PosGetZ>(caller, "getZ", ecsRef, comp)},
                {"w", makeFun<PosGetWidth>(caller, "getWidth", ecsRef, comp)},
                {"h", makeFun<PosGetHeight>(caller, "getHeight", ecsRef, comp)},
                {"setVisibility", makeFun<PosSetVisible>(caller, "setVisibility", ecsRef, comp)},
                {"setX", makeFun<PosSetX>(caller, "setX", ecsRef, comp)},
                {"setY", makeFun<PosSetY>(caller, "setY", ecsRef, comp)},
                {"setZ", makeFun<PosSetY>(caller, "setZ", ecsRef, comp)},
                {"setWidth", makeFun<PosSetW>(caller, "setWidth", ecsRef, comp)},
                {"setHeight", makeFun<PosSetH>(caller, "setHeight", ecsRef, comp)}};
    }
}