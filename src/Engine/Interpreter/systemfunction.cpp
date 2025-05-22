#include "stdafx.h"

#include "systemfunction.h"

#include "logger.h"

#include <chrono>
#include <memory>

namespace pg
{

    namespace
    {
        const char * DOM = "System function";
    }

    void ToString::setUp()
    {
        setArity(1, 1);
    }

    ValuablePtr ToString::call(ValuableQueue& args)
    {
        auto v = args.front()->getElement();
        args.pop();

        return makeVar(v.toString());
    }

    void LogInfo::setUp()
    {
        setArity(1, 1);
    }

    ValuablePtr LogInfo::call(ValuableQueue& args)
    {
        auto v = args.front()->getElement();
        args.pop();

        LOG_INFO(DOM, v.toString());

        return nullptr;
    }

    void HRClock::setUp()
    {
        setArity(0, 0);
    }

    #include <chrono>
    ValuablePtr HRClock::call(ValuableQueue&)
    {
        const auto p1 = std::chrono::system_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(p1.time_since_epoch()).count();

        return makeVar(static_cast<int>(elapsed));
    }
}