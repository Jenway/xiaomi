#include <iostream>
#include <ostream>
#include <string>

class Book {
private:
    std::string _title {};
    std::string _author {};
    int _version {};

public:
    ;
    int getVersion() const { return _version; }
    void setVersion(int version)
    {
        this->_version = version;
    }
    Book(std::string title, std::string author, int version)
        : _title(title)
        , _author(author)
        , _version(version)
    {
    }
    friend std::ostream& operator<<(std::ostream& os, const Book& book);
};

std::ostream& operator<<(std::ostream& os, const Book& book)
{
    os << "Title: " << book._title << ", Author: " << book._author
       << ", Version: " << book._version;
    return os;
}

int main(void)
{
    auto booka = Book("Book A", "Someone", 1);
    auto bookb = Book("Book B", "Another one", 3);
    std::cout << "Book A -> " << booka << "\n";
    std::cout << "Book B -> " << bookb << "\n";

    std::cout << "Version of Book A -> " << booka.getVersion() << "\n";
    std::cout << "Version of Book B -> " << bookb.getVersion() << "\n";
    bookb.setVersion(4);
    std::cout << "After set Book B's version...\n";
    std::cout << "Version of Book B -> " << bookb.getVersion() << "\n";
}