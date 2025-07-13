#pragma once

#include <concepts>
#include <utility>

namespace utils {

template <typename T>
concept Swappable = std::move_constructible<T> && std::assignable_from<T&, T>;

template <Swappable T>
void mySwap(T& a, T& b)
{
    T temp = std::move(a);
    a = std::move(b);
    b = std::move(temp);
}

} // namespace utils
