#include "MyStrNoMove.hpp"
#include "MyString.hpp"
#include "jj.hpp"
#include <algorithm>
#include <cstdio> //snprintf()
#include <cstdlib> //RAND_MAX
#include <cstring> //strlen(), memcpy()
#include <forward_list>
#include <iostream>
#include <list>
#include <string>

using std::string;
using namespace std;

namespace jj00 {

namespace {
    bool strLonger(const string& s1, const string& s2)
    {
        return s1.size() < s2.size();
    }
}
void test_misc()
{
    cout << "\ntest_misc().......... \n";
    // 以下這些是標準庫的眾多容器的 max_size() 計算⽅式.
    cout << size_t(-1) << "\n";
    cout << size_t(-1) / sizeof(long) << "\n";
    cout << size_t(-1) / sizeof(string) << "\n";
    cout << size_t(-1) / sizeof(_List_node<string>) << "\n";
    cout << size_t(-1) / sizeof(_Fwd_list_node<string>) << "\n"; // 536870911cout << "RAND_MAX= " << RAND_MAX << "\n";
    cout << min({ 2, 5, 8, 9, 45, 0, 81 }) << "\n"; // 0cout << max( {2,5,8,9,45,0,81} ) << "\n"; //81vector<int> v {2,5,8,9,45,0,81};
    cout << "max of zoo and hello : "
         << max(string("zoo"), string("hello")) << "\n";
    cout << "longest of zoo and hello : "
         << max(string("zoo"), string("hello"), strLonger) << "\n"; // hell
    cout << hash<MyString>()(MyString("Ace")) << "\n";
    cout << hash<MyString>()(MyString("Stacy")) << "\n";

    // cout << "MyString(zoo) < MyString(hello) ==> " << (MyString("zoo") < MyScout << "MyString(zoo) == MyString(hello) ==> " << (MyString("zoo") == Mcout << "MyStrNoMove(zoo) < MyStrNoMove(hello) ==> " << (MyStrNoMove("zocout << "MyStrNoMove(zoo) == MyStrNoMove(hello) ==> " << (MyStrNoMove("z
    // 以上建構了 6 個 MyString objects 和 4 個 MyStrNoMove objects，都是暫時⽣命
}
}
