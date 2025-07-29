#include "animalProxy.h"
#include <iostream>

struct Dog {
    void say() const
    {
        std::cout << "Bark" << std::endl;
    }
};

struct Cat {
    void say() const
    {
        std::cout << "Meow" << std::endl;
    }
};

int main()
{
    AnimalProxy d = Dog {};
    AnimalProxy c = Cat {};

    d.say(); // Bark
    c.say(); // Meow

    return 0;
}
