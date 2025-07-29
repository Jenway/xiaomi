#pragma once

namespace math_utils {

template <typename T>
T add(T a, T b)
{
    return a + b;
}

template <typename T>
T sub(T a, T b)
{
    return a - b;
}
template <typename T>
T mul(T a, T b)
{
    return a * b;
}

template <typename T>
T div(T a, T b)
{
    return a / b;
}
}