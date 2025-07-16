#include <iostream>
#include "ox/stacktrace/stacktrace.hpp"

template <typename T>
void Print() {
    ox::stacktrace::print(2);
}

template <typename T>
void Print(T&& t) {
    std::cout << t << std::endl;
    if (t == 9) {
        ox::stacktrace::print(0);
    }
}

template <typename T, typename... Args>
void Print(T&& t, Args&&... args) {
    std::cout << t << std::endl;
    Print(std::forward<Args>(args)...);
}

int main() {
    Print<int, int, int, int, int, int, int, int, int>(1, 2, 3, 4, 5, 6, 7, 8, 9);
    return 0;
}
