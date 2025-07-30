#include <cstdint>
#include <iostream>

struct Complex {
    double real;
    double imag;

    Complex operator+(const Complex& other) const
    {
        return { .real = real + other.real, .imag = imag + other.imag };
    }

    friend std::ostream& operator<<(std::ostream& os, const Complex& c)
    {
        return os << c.real << " + " << c.imag << "i";
    }
};

int main()
{
    auto c1 = Complex { .real = 1.0, .imag = 2.0 };
    auto c2 = Complex { .real = 3.0, .imag = 4.0 };
    auto c3 = c1 + c2;

    std::cout << "c1: " << c1 << '\n';
    std::cout << "c2: " << c2 << '\n';
    std::cout << "c3 (c1 + c2): " << c3 << '\n';

    return 0;
}