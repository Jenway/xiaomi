//----------------------------------------------------
#include <cstdio> //snprintf()
#include <cstdlib> //RAND_MAX
#include <cstring> //strlen(), memcpy()
#include <iostream>
#include <string>
using std::cin;
using std::cout;
using std::string;

#include "MyStrNoMove.hpp"

size_t MyStrNoMove::DCtor = 0;
size_t MyStrNoMove::Ctor = 0;
size_t MyStrNoMove::CCtor = 0;
size_t MyStrNoMove::CAsgn = 0;
size_t MyStrNoMove::MCtor = 0;
size_t MyStrNoMove::MAsgn = 0;
size_t MyStrNoMove::Dtor = 0;
