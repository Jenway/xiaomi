#include <iostream>
#include <memory>

int main()
{
    // Create a unique pointer to an integer
    std::unique_ptr<int> ptr = std::make_unique<int>(42);

    // Create a shared_ptr from the unique_ptr by releasing ownership
    std::shared_ptr<int> shared1 = std::shared_ptr<int>(ptr.release());

    // Print the value and reference count
    std::cout << "Value: " << *shared1 << '\n';
    std::cout << "Reference count: " << shared1.use_count() << '\n';

    // Check if the original pointer is null (it should be after release)
    if (!ptr) {
        std::cout << "Original unique_ptr is now null after release.\n";
    }

    return 0;
}