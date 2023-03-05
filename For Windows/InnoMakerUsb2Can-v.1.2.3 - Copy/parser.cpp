#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <fstream>
#include "thrustConfig.cpp"
#include <filesystem>
#include "CommandControl.cpp"

namespace fs = std::filesystem;

AbstractCommand* makeCommand(std::string commandType, std::istringstream& iss) {
    if (commandType == "CONSTANT") {

        double thrust_per;
        double duration;

        if (!(iss >> thrust_per)) { throw new std::invalid_argument("Could not read in thrust per"); }
        if (!(iss >> duration)) { throw new std::invalid_argument("Could not read in time"); }

        return new CommandConstant(Duration(duration), thrust_per);
    }
    else if (commandType == "LINEAR") {
        double duration;
        double startPercentage;
        double endPercentage;

        if (!(iss >> startPercentage)) { throw new std::invalid_argument("Could not read in start per"); }
        if (!(iss >> endPercentage)) { throw new std::invalid_argument("Could not read in end per"); }
        if (!(iss >> duration)) { throw new std::invalid_argument("Could not read in time"); }
        return new CommandLinear(std::chrono::duration<double, std::milli>(duration), startPercentage, endPercentage);
    }
    else {
        throw new std::invalid_argument("Invalid command type");
    }


}
std::vector<AbstractCommand*> parse(std::string file_loc) {
    std::string path = "./";
    for (const auto& entry : fs::directory_iterator(path))
        std::cout << entry.path() << std::endl;

    std::ifstream infile;
    infile.open(file_loc);
    if (!infile.is_open()) throw new std::invalid_argument("Could not open file");

    std::string line;
    std::vector<AbstractCommand*> commands;
    while (std::getline(infile, line))
    {
        // if any metadata needs to be handled first add it here

        std::istringstream iss(line);
        std::string commandType;
        
        if (!(iss >> commandType)) { throw new std::invalid_argument("Could not read in command"); } 
        std::transform(commandType.begin(), commandType.end(), commandType.begin(), [](unsigned char c) { return std::toupper(c); });
        commands.push_back(makeCommand(commandType, iss));

    }

    //assumes we only have throttle commands for now
    for (int i = 1; i < commands.size(); i++) {
        ((ThrottleCommand*)commands[i - 1])->setNext((ThrottleCommand*)commands[i]);
    }
    return commands;
}

