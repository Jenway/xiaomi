#include "MyStrNoMove.hpp"
#include "MyString.hpp"
#include "helper.hpp"
#include <algorithm>
#include <cstdio> //snprintf()
#include <cstdlib> //abort()
#include <ctime>
#include <iostream>
#include <set>

using namespace std;

namespace jj06 {
void test_multiset(long& value)
{
    cout << "\ntest_multiset().......... \n";
    multiset<string> c;
    char buf[10];
    clock_t timeStart = clock();
    for (long i = 0; i < value; ++i) {
        try {
            snprintf(buf, 10, "%d", rand());
            c.insert(string(buf));
        } catch (exception& p) {
            cout << "i=" << i << " " << p.what() << endl;
            abort();
        }
    }
    cout << "milli-seconds : " << (clock() - timeStart) << endl;
    cout << "multiset.size()= " << c.size() << endl;
    cout << "multiset.max_size()= " << c.max_size() << endl;
    // 214748
    string target = get_a_target_string();
    {
        timeStart = clock();
        auto pItem = find(c.begin(), c.end(), target);
        // ⽐ c.find(...) 慢很多
        cout << "std::find(), milli-seconds : " << (clock() - timeStart) << endl;
        if (pItem != c.end())
            cout << "found, " << *pItem << endl;
        else
            cout << "not found! " << endl;
    }
    {
        timeStart = clock();
        auto pItem = c.find(target);
        // ⽐ std::find(...) 快很多
        cout << "c.find(), milli-seconds : " << (clock() - timeStart) << endl;
        if (pItem != c.end())
            cout << "found, " << *pItem << endl;
        else
            cout << "not found! " << endl;
    }
    c.clear();
    test_moveable(multiset<MyString>(), multiset<MyStrNoMove>(), value);
}
}