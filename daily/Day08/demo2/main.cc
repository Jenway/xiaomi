#include "add.h"
#include "sub.h"

#include <iostream>

int main()
{
    std::cout << "3 + 2 -> " << add(3, 2) << "\n";
    std::cout << "3 - 2 -> " << sub(3, 2) << "\n";
}