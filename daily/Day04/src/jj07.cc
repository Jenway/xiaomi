#include "MyStrNoMove.hpp"
#include "MyString.hpp"
#include "helper.hpp"
#include <cstdio> //snprintf()
#include <cstdlib> //abort()
#include <ctime>
#include <iostream>
#include <map>

using namespace std;

namespace jj07 {
void test_multimap(long& value)
{
    cout << "\ntest_multimap().......... \n";
    multimap<long, string> c;
    char buf[10];
    clock_t timeStart = clock();
    for (long i = 0; i < value; ++i) {
        try {
            snprintf(buf, 10, "%d", rand()); // multimap 不可使⽤ [] 做 insertion
            c.insert(pair<long, string>(i, buf));
        } catch (exception& p) {
            cout << "i=" << i << " " << p.what() << endl;
            abort();
        }
    }
    cout << "milli-seconds : " << (clock() - timeStart) << endl;
    cout << "multimap.size()= " << c.size() << endl;
    cout << "multimap.max_size()= " << c.max_size() << endl;
    long target = get_a_target_long();
    timeStart = clock();
    auto pItem = c.find(target);
    cout << "c.find(), milli-seconds : " << (clock() - timeStart) << endl;
    if (pItem != c.end())
        cout << "found, value=" << (*pItem).second << endl;
    else
        cout << "not found! " << endl;
    c.clear();
}
}