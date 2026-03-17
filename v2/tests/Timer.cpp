#include <chrono>

using namespace std::chrono;

// a classic utility for performance profiling. It uses the C++ <chrono> library to measure execution time with microsecond precision and returns the result in milliseconds
class Timer {
    std::chrono::high_resolution_clock::time_point start;
public:
    Timer(): start(std::chrono::high_resolution_clock::now()) {}

    double elapsed() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    }
};