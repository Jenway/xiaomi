#include "animals.h"
#include <iostream>

void animalSay(const Animal& animal)
{
    std::cout << "  - call say()         : ";
    animal.say(); // 多态：调用实际类型的 say()
    std::cout << "  - call wronglySay()  : ";
    animal.wronglySay(); // 静态绑定：调用 Animal::wronglySay()
}

int main()
{
    Dog dog;
    Cat cat;

    std::cout << "- call animalSay() on Dog:" << std::endl;
    animalSay(dog);

    std::cout << "- call animalSay() on Cat:" << std::endl;
    animalSay(cat);
}
