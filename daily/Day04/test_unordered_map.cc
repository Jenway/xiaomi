#include <iostream>
#include <string>
#include <unordered_map>

struct Student {
    int id;
    std::string name;
};

int main()
{
    auto student_map = std::unordered_map<int, Student>();

    auto student1 = Student { .id = 1, .name = "Alice" };
    auto student2 = Student { .id = 2, .name = "Bob" };
    student_map[student1.id] = student1;
    student_map[student2.id] = student2;

    auto it = student_map.find(1);
    if (it != student_map.end()) {
        auto& student = it->second;
        std::cout << "Found student: " << student.name << '\n';
    } else {
        std::cout << "Student with id 1 not found." << '\n';
    }
}