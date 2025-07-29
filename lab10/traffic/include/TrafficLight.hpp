#ifndef TRAFFIC_LIGHT_HPP
#define TRAFFIC_LIGHT_HPP

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

enum class TrafficLightState {
    Red,
    Green,
    Yellow
};

class TrafficLight {
public:
    TrafficLight();
    ~TrafficLight();

    void startLogicThread();
    void stopLogicThread();

    void resumeCycling();
    void pauseCycling();

    TrafficLightState getCurrentState() const;

private:
    void stateCycleLoop();

    std::thread logic_thread_;
    mutable std::mutex mutex_;
    std::condition_variable condition_variable_;
    std::atomic<bool> run_logic_;
    std::atomic<bool> is_cycling_paused_;
    std::atomic<TrafficLightState> current_state_;
};

#endif // TRAFFIC_LIGHT_HPP