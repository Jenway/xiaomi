#include "mySwap.h"
#include <cstdint>
#include <iostream>
#include <string>

using std::cout;
using std::string;

int main()
{
    int32_t x = 10;
    int32_t y = 20;
    cout << "Before swap: x = " << x << ", y = " << y << '\n';
    utils::mySwap(x, y);
    cout << "After swap: x = " << x << ", y = " << y << '\n';

    string s1 = "hello";
    string s2 = "world";
    cout << "Before swap: s1 = " << s1 << ", s2 = " << s2 << '\n';
    utils::mySwap(s1, s2);
    cout << "After swap: s1 = " << s1 << ", s2 = " << s2 << '\n';

    return 0;
}
