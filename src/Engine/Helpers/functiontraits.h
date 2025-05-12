#pragma once

#include <functional>
#include <tuple>
#include <type_traits>

namespace pg
{
    // primary template left undefined
    template <typename>
    struct function_traits;

    // partial specialization for std::function<R(Args...)>
    template <typename R, typename... Args>
    struct function_traits<std::function<R(Args...)>>
    {
        using return_type = R;
        static constexpr std::size_t arity = sizeof...(Args);
        using args = std::tuple<Args...>;

        template<std::size_t N>
        using arg = std::tuple_element_t<N, args>;
    };

    // generic callable
    template <typename T>
    struct function_traits : public function_traits<decltype(&T::operator())>{ };

    // member‐function pointer specialization
    template <typename C, typename R, typename... Args>
    struct function_traits<R(C::*)(Args...) const>
    {
        using return_type = R;
        static constexpr std::size_t arity = sizeof...(Args);
        using args = std::tuple<Args...>;

        template<std::size_t N>
        using arg = std::tuple_element_t<N, args>;
    };
}