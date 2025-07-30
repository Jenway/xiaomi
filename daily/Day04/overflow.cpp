// overflow.cpp
#include <iostream>

int main() {
    int* arr = new int[5];
    arr[10] = 42; // 越界访问
    delete[] arr;
    return 0;
}

