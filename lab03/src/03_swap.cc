#include <iostream>
#include <tuple>

template <typename T>
void _swap(T& a, T& b)
{
    std::tie(a, b) = std::make_tuple(b, a);
}

template <typename T>
void _swap(T* a, T* b)
{
    T temp = *a;
    *a = *b;
    *b = temp;
}

int main(void)
{

    int x = 5, y = 10;
    std::cout << "Before swap: \n"
              << " - x = " << x << ", y = " << y << std::endl;
    _swap(x, y);
    std::cout << "After swap : \n"
              << " - x = " << x << ", y = " << y << std::endl;

    int* px = &x;
    int* py = &y;
    std::cout << "Before pointer swap:\n"
              << " - *px = " << *px << ", *py = " << *py << "\n"
              << " -  px -> " << px << ",  py -> " << py << std::endl;
    _swap(px, py);
    std::cout << "After pointer swap :\n"
              << " - *px = " << *px << ", *py = " << *py << "\n"
              << " -  px -> " << px << ",  py -> " << py << std::endl;

    return 0;
}