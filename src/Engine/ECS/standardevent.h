#pragma once

#include <unordered_map>
#include <type_traits>

#include "Memory/elementtype.h"

// ============================================================================
// Compile-time guard for automatic C++ event to StandardEvent conversion
// ============================================================================
// Define PG_AUTO_CONVERT_EVENTS_TO_STANDARD to enable automatic conversion
// of C++ typed events to StandardEvents when using sendEvent().
//
// To enable: Add -DPG_AUTO_CONVERT_EVENTS_TO_STANDARD to your compiler flags
// To disable: Simply don't define this macro (default behavior - no overhead)
//
// When enabled, any C++ event with a toStandardEvent() method will
// automatically be converted and dispatched as a StandardEvent in addition
// to the typed event dispatch.
// ============================================================================
// #define PG_AUTO_CONVERT_EVENTS_TO_STANDARD

namespace pg
{
    // ============================================================================
    // Type trait to detect if an event type has toStandardEvent() method
    // ============================================================================
    template <typename Event, typename = void>
    struct has_to_standard_event : std::false_type {};

    template <typename Event>
    struct has_to_standard_event<Event,
        std::void_t<decltype(std::declval<Event>().toStandardEvent())>>
        : std::true_type {};

    template <typename Event>
    inline constexpr bool has_to_standard_event_v = has_to_standard_event<Event>::value;

    // ============================================================================
    // Helper macro to mark events as convertible to StandardEvent
    // ============================================================================
    // Usage in your event struct:
    //   struct MyEvent {
    //       int value;
    //       STANDARD_EVENT_CONVERTIBLE(MyEvent)
    //   };
    //
    // Then implement in .cpp file:
    //   STANDARD_EVENT_CONVERSION_IMPL(MyEvent) {
    //       StandardEvent event("MyEvent");
    //       event.values["value"] = ElementType{value};
    //       return event;
    //   }
    #define STANDARD_EVENT_CONVERTIBLE(EventType) \
        StandardEvent toStandardEvent() const;

    #define STANDARD_EVENT_CONVERSION_IMPL(EventType) \
        StandardEvent EventType::toStandardEvent() const

    struct StandardEvent
    {
        StandardEvent(const std::string& name = "Noop") : name(name) {}

        StandardEvent(const std::string& name, const std::string& valueName, const ElementType& value) : name(name)
        {
            values[valueName] = value;
        }

        template <typename... Args>
        StandardEvent(const std::string& name, const std::string& valueName, const ElementType& value, const Args&... args) : StandardEvent(name, args...)
        {
            values[valueName] = value;
        }

        template <typename Type>
        StandardEvent(const std::string& name, const std::string& valueName, const Type& value) : name(name)
        {
            values[valueName] = ElementType{value};
        }

        template <typename Type, typename... Args>
        StandardEvent(const std::string& name, const std::string& valueName, const Type& value, const Args&... args) : StandardEvent(name, args...)
        {
            values[valueName] = ElementType{value};
        }

        // Todo need to make a ElementType ctor to avoid a copy

        StandardEvent(const StandardEvent& other) : name(other.name), values(other.values) {}

        StandardEvent& operator=(const StandardEvent& other)
        {
            name = other.name;
            values = other.values;

            return *this;
        }

        bool has(const std::string& valueName) const
        {
            return values.find(valueName) != values.end();
        }

        template <typename T>
        T get(const std::string& valueName) const
        {
            return values.at(valueName).get<T>();
        }

        ElementType getElement(const std::string& valueName) const
        {
            return values.at(valueName);
        }

        std::string name;

        ElementMap values;
    };
}