#include "TrafficLight.hpp"
#include <chrono>
#include <iostream>

#include "TrafficLight.hpp"

TrafficLight::TrafficLight()
    : run_logic_(true)
    , is_cycling_paused_(true)
    , current_state_(TrafficLightState::Red)
{
}

TrafficLight::~TrafficLight()
{
    stopLogicThread();
}

void TrafficLight::startLogicThread()
{
    if (!logic_thread_.joinable()) {
        logic_thread_ = std::thread(&TrafficLight::stateCycleLoop, this);
        std::cout << "Traffic Light logic thread started." << std::endl;
    }
}

void TrafficLight::stopLogicThread()
{
    run_logic_.store(false);
    condition_variable_.notify_all();
    if (logic_thread_.joinable()) {
        logic_thread_.join();
        std::cout << "Traffic Light logic thread stopped." << std::endl;
    }
}

void TrafficLight::resumeCycling()
{
    std::unique_lock<std::mutex> lock(mutex_);
    is_cycling_paused_.store(false);
    std::cout << "Traffic Light: Resumed Cycling" << std::endl;
    condition_variable_.notify_one();
}

void TrafficLight::pauseCycling()
{
    std::unique_lock<std::mutex> lock(mutex_);
    is_cycling_paused_.store(true);
    std::cout << "Traffic Light: Paused Cycling" << std::endl;
}

TrafficLightState TrafficLight::getCurrentState() const
{
    return current_state_.load();
}

void TrafficLight::stateCycleLoop()
{
    while (run_logic_.load()) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_variable_.wait(lock, [this] {
            return !is_cycling_paused_.load() || !run_logic_.load();
        });

        if (!run_logic_.load()) {
            break;
        }

        switch (current_state_.load()) {
        case TrafficLightState::Red:
            std::cout << "Traffic Light: Red (10 seconds)" << std::endl;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(10));
            lock.lock();
            current_state_.store(TrafficLightState::Green);
            break;
        case TrafficLightState::Green:
            std::cout << "Traffic Light: Green (8 seconds)" << std::endl;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(8));
            lock.lock();
            current_state_.store(TrafficLightState::Yellow);
            break;
        case TrafficLightState::Yellow:
            std::cout << "Traffic Light: Yellow (2 seconds)" << std::endl;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            lock.lock();
            current_state_.store(TrafficLightState::Red);
            break;
        }
    }
}