#include <iostream>

class Animal {
public:
    void wronglySay() const
    {
        std::cout << "Nothing" << std::endl;
    }

    virtual void say() const
    {
        std::cout << "Nothing" << std::endl;
    }

    virtual ~Animal() = default;
};

class Dog : public Animal {
public:
    void wronglySay() const
    {
        std::cout << "Bark" << std::endl;
    }

    virtual void say() const override
    {
        std::cout << "Bark" << std::endl;
    }
};

class Cat : public Animal {
public:
    void wronglySay() const
    {
        std::cout << "Meow" << std::endl;
    }

    virtual void say() const override
    {
        std::cout << "Meow" << std::endl;
    }
};
