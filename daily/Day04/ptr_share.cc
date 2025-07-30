#include <iostream>
#include <memory>
#include <string>

struct StuRecord {
    int id;
    std::string name;
    double score;

    StuRecord()
    {
        std::cout << "[DefaultCtor] StuRecord()\n";
    }

    StuRecord(int id, std::string name, double score)
        : id(id)
        , name(std::move(name))
        , score(score)
    {
        std::cout << "[CustomCtor] StuRecord(id=" << id << ", name=" << this->name << ", score=" << score << ")\n";
    }

    StuRecord(const StuRecord& other)
        : id(other.id)
        , name(other.name)
        , score(other.score)
    {
        std::cout << "[CopyCtor] StuRecord copied\n";
    }

    StuRecord(StuRecord&& other) noexcept
        : id(other.id)
        , name(std::move(other.name))
        , score(other.score)
    {
        std::cout << "[MoveCtor] StuRecord moved\n";
    }

    ~StuRecord()
    {
        std::cout << "[Dtor] StuRecord destroyed\n";
    }
};

namespace {
void testShared()
{
    auto recordPtr1 = std::make_shared<StuRecord>(StuRecord(1, "Alice", 95.5));
    std::cout << "Ref count of 1: " << recordPtr1.use_count() << '\n';
    auto recordPtr2 = std::shared_ptr<StuRecord>(recordPtr1);
    std::cout << "Ref count of 1: " << recordPtr2.use_count() << '\n';
    auto recordPtr3 = std::make_shared<StuRecord>(2, "Bob", 95.6);
    std::cout << "Ref count of recordPtr3: " << recordPtr3.use_count() << '\n';
    auto recordPtr4 = std::weak_ptr<StuRecord>(recordPtr3);
    std::cout << "Ref count of recordPtr3: " << recordPtr3.use_count() << '\n';
    if (auto recordPtr5 = recordPtr4.lock()) {
        std::cout << "Ref count of recordPtr3 after lock: " << recordPtr3.use_count() << '\n';
        std::cout << "RecordPtr5 points to: " << recordPtr5->name << " with score " << recordPtr5->score << '\n';
    } else {
        std::cout << "RecordPtr5 is empty, recordPtr3 has been destroyed.\n";
    }
}
}

int main()
{
    testShared();
}
