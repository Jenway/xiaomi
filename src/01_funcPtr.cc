#include <array>
#include <iostream>
#include <string>

using FuncPtr = int (*)(int, int);

int add(int a, int b)
{
    return a + b;
}

int subtract(int a, int b)
{
    return a - b;
}

constexpr int FUNC_COUNT = 4;

int main()
{
    FuncPtr* funcPtrArray = new FuncPtr[FUNC_COUNT];
    auto funcNames = std::array<std::string, FUNC_COUNT> {
        "lambda (add)", "lambda (subtract)", "add", "subtract"
    };
    funcPtrArray[0] = [](int a, int b) { return a + b; };
    funcPtrArray[1] = [](int a, int b) { return a - b; };
    funcPtrArray[2] = add;
    funcPtrArray[3] = subtract;

    int x = 4;
    int y = 2;

    std::cout << "Apply functions to: a = " << x << " and b = " << y << ":" << std::endl;

    for (int i = 0; i < FUNC_COUNT; ++i) {
        std::cout << funcNames[i] << " result: " << funcPtrArray[i](x, y) << std::endl;
    }

    return 0;
}
