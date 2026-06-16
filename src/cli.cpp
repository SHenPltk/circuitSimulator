#include "circuit/cli.h"
#include "circuit/circuit_simulator.h"

#include <iostream>
#include <string>
#include <vector>

namespace circuit {

int run(int /*argc*/, char** /*argv*/) {
    CircuitSimulator simulator;
    std::string line;
    std::cout << "Circuit Simulator DSL (type 'exit' to quit)\n";
    std::cout << "> " << std::flush;

    while (std::getline(std::cin, line)) {
        std::vector<std::string> outputLines;
        std::vector<std::string> errorLines;
        const bool keepRunning =
            simulator.executeCommand(line, outputLines, errorLines);

        for (const auto& error : errorLines) {
            std::cerr << "Error: " << error << '\n';
        }
        for (const auto& output : outputLines) {
            std::cout << output << '\n';
        }

        if (!keepRunning) {
            break;
        }

        std::cout << "> " << std::flush;
    }

    return 0;
}

}  // namespace circuit