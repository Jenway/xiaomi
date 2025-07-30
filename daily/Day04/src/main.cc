#include "MyStrNoMove.hpp"
#include "jj.hpp"
#include <cstdio> //snprintf()
#include <cstdlib> //RAND_MAX
#include <cstring> //strlen(), memcpy()

int main()
{
    // jj00::test_misc();
    // jj01::test_array();
    long value = 10000000L; // 1000萬
    // jj02::test_vector(value);
    jj03::test_list(value);
}