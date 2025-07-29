#include "math/add.h"
#include "math/sub.h"
#include <cstdint>
#include <iostream>

int main()
{
    int32_t a { 42 };
    int32_t b { 22 };
    std::cout << a << " + " << b << "= " << add(a, b) << "\n";
    std::cout << a << " - " << b << "= " << sub(a, b) << "\n";

    return 0;
}
