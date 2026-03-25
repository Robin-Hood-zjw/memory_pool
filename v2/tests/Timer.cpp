#include <chrono>

using namespace std::chrono;

/**
 * @brief RAII-style Timer class for high-resolution performance measurement.
 */
class Timer {
    // store the fixed point in time when the object is created
    high_resolution_clock::time_point start;
public:
    /**
     * @brief constructor: start the clock immediately upon object instantiation
     * initialize the 'start' member with the "now" timestamp via the system's high-resolution steady clock
     **/
    Timer(): start(high_resolution_clock::now()) {}

    /**
     * @brief calculate the time elapsed since the Timer was created
     * @return double The elapsed time in milliseconds (ms)
     **/
    double elapsed() {
        // capture the current time point ('end')
        auto end = high_resolution_clock::now();
        // calculate the duration between 'end' and the 'start'
        auto duration = end - start;
        //cast that duration into microseconds, convert to double and divide by 1000.0 to get milliseconds
        return duration_cast<microseconds>(duration).count() / 1000.0;
    }
};