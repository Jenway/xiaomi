#include "helper.hpp"
#include <cstdio> //snprintf()
#include <cstdlib> //abort()
#include <ctime>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace std;

namespace jj09 {
void test_unordered_multimap(long& value)
{
    cout << "\ntest_unordered_multimap().......... \n";
    unordered_multimap<long, string> c;
    char buf[10];
    clock_t timeStart = clock();
    for (long i = 0; i < value; ++i) {
        try {
            snprintf(buf, 10, "%d", rand());
            // multimap 不可使⽤ [] 進⾏ insertionc.insert(pair<long,string>(i,buf));
        } catch (exception& p) {
            cout << "i=" << i << " " << p.what() << endl;
            abort();
        }
    }
    cout << "milli-seconds : " << (clock() - timeStart) << endl;
    cout << "unordered_multimap.size()= " << c.size() << endl;
    cout << "unordered_multimap.max_size()= " << c.max_size() << endl;
    long target = get_a_target_long();
    timeStart = clock();
    auto pItem = c.find(target);
    cout << "c.find(), milli-seconds : " << (clock() - timeStart) << endl;
    if (pItem != c.end())
        cout << "found, value=" << (*pItem).second << endl;
    else
        cout << "not found! " << endl;
}
}