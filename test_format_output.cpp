#include "circuit/circuit_simulator.h"

#include <iostream>
#include <string>

int main() {
    using namespace circuit;

    CircuitSimulator sim;
    sim.addComponent("power", "p");
    sim.addComponents("switch", {"s1", "s2"});
    sim.addComponents("bulb", {"b1", "b2"});
    sim.addLink("p.+", "s1.1");
    sim.addLink("s1.2", "b1.1");
    sim.addLink("b1.2", "s2.1");
    sim.addLink("s2.2", "b2.1");
    sim.addLink("b2.2", "p.-");

    sim.setSwitchState("s1", true);
    sim.setSwitchState("s2", false);

    std::string formatted = sim.formatOutput("s: @s @ | b: @b @\n");
    if (formatted != "s: 1 0 | b: 0 0\n") {
        std::cerr << "Unexpected formatted output: " << formatted;
        return 1;
    }

    sim.setSwitchState("s2", true);
    formatted = sim.formatOutput("switches: @s @, bulbs: @b @");
    if (formatted != "switches: 1 1, bulbs: 1 1") {
        std::cerr << "Unexpected multi-placeholder output: " << formatted;
        return 1;
    }

    formatted = sim.formatOutput("no placeholders");
    if (formatted != "no placeholders") {
        std::cerr << "Unexpected plain output: " << formatted;
        return 1;
    }

    CircuitSimulator empty_sim;
    formatted = empty_sim.formatOutput("@s @");
    if (formatted != "") {
        std::cerr << "Unexpected empty sim output: " << formatted;
        return 1;
    }

    std::cout << "formatOutput tests passed\n";
    return 0;
}
