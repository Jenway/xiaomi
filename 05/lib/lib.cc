#include "lib.hpp"
#include <iostream>

namespace helloShared {

void say_hello()
{
    std::cout << "Hello from shared library!" << std::endl;
}
}