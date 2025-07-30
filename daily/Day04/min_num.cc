#include <iostream>
#include <string>
namespace demo {

template <typename T>
T min(T a, T b)
{
    return a < b ? a : b;
}

template <typename T, typename Compare>
T min(T a, T b, Compare comp)
{
    return comp(a, b) ? a : b;
}

}

/*
课堂练习
编写一个模版函数，实现比较两个班级的人数，包括总人数，男女生人数
1..求两个输入的最大值
2.支持自定义比较函数
*/

struct ClassStat {
    int total;
    int male;
    int female;
};

struct CompareTotal {
    bool operator()(const ClassStat& a, const ClassStat& b) const
    {
        return a.total < b.total;
    }
};

struct CompareMale {
    bool operator()(const ClassStat& a, const ClassStat& b) const
    {
        return a.male < b.male;
    }
};

struct CompareFemale {
    bool operator()(const ClassStat& a, const ClassStat& b) const
    {
        return a.female < b.female;
    }
};

int main()
{
    int a = 5, b = 3;
    std::cout << "min(" << a << ", " << b << ") = "
              << demo::min(a, b) << std::endl;

    double x = 2.5, y = 3.7;
    std::cout << "min(" << x << ", " << y << ") = "
              << demo::min(x, y) << std::endl;

    std::string str1 = "apple", str2 = "banana";
    std::cout << "min(\"" << str1 << "\", \"" << str2 << "\") = "
              << demo::min(str1, str2) << std::endl;

    std::cout << R"(min("hi", "aello") = )"
              << demo::min(std::string("hi"), std::string("aello")) << std::endl;

    std::cout << R"(longer_than min("hello", "world") = )"
              << demo::min("hello", "world", CompareFemale {}) << std::endl;

    return 0;
}
