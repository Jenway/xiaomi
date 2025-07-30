//----------------------------------------------------
#include <cstdio> //snprintf()
#include <cstdlib> //RAND_MAX
#include <cstring> //strlen(), memcpy()
#include <iostream>
#include <string>
using std::cin;
using std::cout;
using std::string;

//-----------------
// 以下 MyStrNoMove 是為了測試 containers with no-moveable elements 效果.
class MyStrNoMove {
public:
    static size_t DCtor;
    // 累計 default-ctor 的呼叫次數
    static size_t Ctor;
    // 累計 ctor 的呼叫次數
    static size_t CCtor;
    // 累計 copy-ctor 的呼叫次數
    static size_t CAsgn;
    // 累計 copy-asgn 的呼叫次數
    static size_t MCtor;
    // 累計 move-ctor 的呼叫次數
    static size_t MAsgn;
    // 累計 move-asgn 的呼叫次數
    static size_t Dtor;
    // 累計 dtor 的呼叫次數
private:
    char* _data;
    size_t _len;
    void _init_data(const char* s)
    {
        _data = new char[_len + 1];
        memcpy(_data, s, _len);
        _data[_len] = '\0';
    }

public:
    // default ctor
    MyStrNoMove()
        : _data(NULL)
        , _len(0)
    {
        ++DCtor;
        _init_data("jj");
    }
    // ctor
    MyStrNoMove(const char* p)
        : _len(strlen(p))
    {
        ++Ctor;
        _init_data(p);
    }
    // copy ctor
    MyStrNoMove(const MyStrNoMove& str)
        : _len(str._len)
    {
        ++CCtor;
        _init_data(str._data);
        // COPY
    }
    // copy assignment
    MyStrNoMove& operator=(const MyStrNoMove& str)
    {
        ++CAsgn;
        if (this != &str) {
            if (_data)
                delete _data;
            _len = str._len;
            _init_data(str._data);
            // COPY!
        } else {
            // Self Assignment, Nothing to do.
        }
        return *this;
    }
    // dtor
    virtual ~MyStrNoMove()
    {
        ++Dtor;
        if (_data) {
            delete _data;
        }
    }
    bool
    operator<(const MyStrNoMove& rhs) const
    // 為了讓 set ⽐較
    {
        return string(this->_data) < string(rhs._data); // 借⽤事實：string 已能
    }
    bool
    operator==(const MyStrNoMove& rhs) const
    // 為了讓 set 判斷相等.
    {
        return string(this->_data) == string(rhs._data); // 借⽤事實：string 已
    }
    char* get() const
    {
        return _data;
    }
};

namespace std
// 必須放在 std 內
{
template <>
struct hash<MyStrNoMove>
// 這是為了 unordered containers
{
    size_t
    operator()(const MyStrNoMove& s) const noexcept
    {
        return hash<string>()(string(s.get()));
    }
    // 借⽤現有的 hash<string> (in ...\4.9.2\include\c++\bits\basic_string.h
};
}