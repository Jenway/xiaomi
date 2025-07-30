#include <cstddef>
#include <iostream>
#include <iterator>
#include <ostream>
#include <queue>
#include <random>

using std::greater;
using std::priority_queue;
using std::vector;

vector<int> min_k_numbers(const vector<int>& input, size_t k)
{
    if (k > input.size()) {
        return {};
    }

    priority_queue<int, vector<int>, greater<>> minHeap;

    for (int num : input) {
        minHeap.push(num);
    }

    vector<int> result;
    for (size_t i = 0; i < k; ++i) {
        result.push_back(minHeap.top());
        minHeap.pop();
    }

    return result;
}

int main()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    vector<int> input;
    input.reserve(20);
    for (size_t i = 0; i < 20; ++i) {
        input.push_back(dis(gen));
    }
    size_t k = 5;
    vector<int> minNumbers(min_k_numbers(input, k));
    std::cout << "Input numbers: ";
    std::copy(input.begin(), input.end(), std::ostream_iterator<int>(std::cout, " "));
    std::cout << "\nMinimum " << k << " numbers: ";
    std::copy(minNumbers.begin(), minNumbers.end(), std::ostream_iterator<int>(std::cout, " "));
    std::cout << '\n';
}