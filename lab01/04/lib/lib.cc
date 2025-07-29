#include "lib.hpp"
#include <iostream>

namespace helloStatic {

void say_hello()
{
    std::cout << "Hello from static library!" << std::endl;
}
}