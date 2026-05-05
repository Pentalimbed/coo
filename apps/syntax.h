// Shorthands and custom syntax sugar for certain annoying situations
#pragma once

#include <utility>

// Scoped enum int conversion shorthand: +my_enum
//
// https://stackoverflow.com/questions/8357240/how-to-automatically-convert-strongly-typed-enum-into-int/42198760#42198760
template <typename T>
constexpr auto operator+(T a) noexcept
{
    return std::to_underlying(a);
}