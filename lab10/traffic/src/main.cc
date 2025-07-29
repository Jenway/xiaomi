#include "TrafficLight.hpp"
#include <iostream>
#include <string>

void userInputHandler(TrafficLight& light)
{
    std::cout << "Press 'o' to RESUME, 'f' to PAUSE the traffic light cycling." << std::endl;
    std::cout << "Press 'q' to QUIT the program." << std::endl;
    std::string command;
    while (std::cin >> command) {
        if (command == "o" || command == "O") {
            light.resumeCycling();
        } else if (command == "f" || command == "F") {
            light.pauseCycling();
        } else if (command == "q" || command == "Q") {
            std::cout << "Exiting program." << std::endl;
            break;
        } else {
            std::cout << "Invalid command. Press 'o' (RESUME), 'f' (PAUSE), or 'q' (QUIT)." << std::endl;
        }
    }
}

int main()
{
    TrafficLight trafficLight;

    trafficLight.startLogicThread();

    userInputHandler(trafficLight);

    return 0;
}