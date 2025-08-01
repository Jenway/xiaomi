#include "MyStrNoMove.hpp"
#include "MyString.hpp"
#include "helper.hpp"
#include <algorithm>
#include <cstdio> //snprintf()
#include <cstdlib> //abort()
#include <ctime>
#include <iostream>
#include <unordered_set>

using namespace std;

namespace jj08 {
void test_unordered_multiset(long& value)
{
    cout << "\ntest_unordered_multiset().......... \n";
    unordered_multiset<string> c;
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
    cout << "unordered_multiset.size()= " << c.size() << endl;
    cout << "unordered_multiset.max_size()= " << c.max_size() << endl;
    cout << "unordered_multiset.bucket_count()= " << c.bucket_count() << endl;
    cout << "unordered_multiset.load_factor()= " << c.load_factor() << endl;
    cout << "unordered_multiset.max_load_factor()= " << c.max_load_factor();
    cout << "unordered_multiset.max_bucket_count()= " << c.max_bucket_count() << endl;
    for (unsigned i = 0; i < 20; ++i) {
        cout << "bucket #" << i << " has " << c.bucket_size(i) << " elements";
    }
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
    test_moveable(unordered_multiset<MyString>(), unordered_multiset<MyStrNoMove>(), value);
    cout << "unordered_multiset<MyString> :" << endl;
}
}