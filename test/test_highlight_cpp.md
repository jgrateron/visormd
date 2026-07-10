# C++ Highlight Test

Classes, templates, namespaces, and STL types.

```cpp
#include <iostream>
#include <vector>
#include <string>
#include <memory>

template<typename T>
class Stack {
private:
    std::vector<T> data;
    size_t capacity;

public:
    explicit Stack(size_t cap = 64) : capacity(cap) {
        data.reserve(cap);
    }

    void push(const T& value) {
        if (data.size() < capacity)
            data.push_back(value);
        else
            throw std::runtime_error("Stack overflow");
    }

    T pop() {
        if (data.empty())
            throw std::runtime_error("Stack underflow");
        T val = data.back();
        data.pop_back();
        return val;
    }

    [[nodiscard]] bool empty() const noexcept {
        return data.empty();
    }
};

int main() {
    auto stack = std::make_unique<Stack<std::string>>(10);

    try {
        stack->push("Hello");
        stack->push("World");

        std::cout << "Top: " << stack->pop() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Null pointer
    int *ptr = nullptr;
    // Smart pointer
    auto sp = std::shared_ptr<int>(new int(42));

    return 0;
}
```
