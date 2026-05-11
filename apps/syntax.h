// Shorthands and custom syntax sugar for certain annoying situations
#pragma once

#include <cstddef>
#include <utility>

// Scoped enum int conversion shorthand: +my_enum
//
// https://stackoverflow.com/questions/8357240/how-to-automatically-convert-strongly-typed-enum-into-int/42198760#42198760
template <typename T>
constexpr auto operator+(T a) noexcept
{
    return std::to_underlying(a);
}

// Initialize array with first few items. Extending final value to the rest
//
// https://stackoverflow.com/questions/71013605/how-can-i-initialize-an-stdarray-of-a-class-without-a-default-constructor
template <typename T, size_t... Is, typename... Args>
constexpr std::array<T, sizeof...(Is)> makeArrayHelper(
    std::index_sequence<Is...> /*unused*/, Args&&... args) {
  return {(static_cast<void>(Is), T{std::forward<Args>(args)...}) ...};
}

template <typename T, size_t N, typename... Args>
constexpr std::array<T, N> makeArray(Args&&... args) {
  return makeArrayHelper<T>(std::make_index_sequence<N>{},
                            std::forward<Args>(args)...);
}