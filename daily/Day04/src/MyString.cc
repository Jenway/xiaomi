#include <cstdio> //snprintf()
#include <cstdlib> //RAND_MAX
#include <cstring> //strlen(), memcpy()
#include <iostream>
#include <string>
using std::cin;
using std::cout;
using std::string;

#include "MyString.hpp"

size_t MyString::DCtor = 0;
size_t MyString::Ctor = 0;
size_t MyString::CCtor = 0;
size_t MyString::CAsgn = 0;
size_t MyString::MCtor = 0;
size_t MyString::MAsgn = 0;
size_t MyString::Dtor = 0;
