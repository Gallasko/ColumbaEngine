#include "scrollable.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr const char * const DOM = "Scrollable";
    }

    // void ScrollableSystem::addChild(EntityRef list, EntityRef child)
    // {
    //     LOG_THIS_MEMBER(DOM);

    //     if (not list->has<Scrollable>())
    //     {
    //         LOG_ERROR(DOM, "This entity[" << list.id << "] doesn't have a scrollable component !");
    //         return;
    //     }

    //     auto scroll = list->get<Scrollable>();

    //     scroll->entities.push_back(child);
    // }

    // void ScrollableSystem::removeChild(EntityRef list, EntityRef child)
    // {
    //     LOG_THIS_MEMBER(DOM);

    //     if (not list->has<Scrollable>())
    //     {
    //         LOG_ERROR(DOM, "This entity[" << list.id << "] doesn't have a scrollable component !");
    //         return;
    //     }

    //     auto scroll = list->get<Scrollable>();

    //     auto it = std::find(scroll->entities.begin(), scroll->entities.end(), child);

    //     if (it != scroll->entities.end())
    //     {
    //         scroll->entities.erase(it);
    //     }
    //     else
    //     {
    //         LOG_ERROR(DOM, "This scrollable[" << list.id << "] doesn't have entity[" << child.id << "] as a child component !");
    //     }
    // }
}