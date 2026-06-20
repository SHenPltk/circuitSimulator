#include "circuit/circuit_simulator.h"

#include <iostream>

int main() {
    using namespace circuit;

    // Test linkSeries with power in the series
    CircuitSimulator sim1;
    sim1.addComponent("power", "p");
    sim1.addComponent("bulb", "b");
    sim1.addComponent("switch", "s");
    if (!sim1.linkSeries("p.+", {"s"}, "b.r")) {
        std::cerr << "linkSeries failed\n";
        return 1;
    }
    if (!sim1.addLink("b.l", "p.-")) {
        std::cerr << "addLink failed\n";
        return 1;
    }
    sim1.setSwitchState("s", true);
    auto r1 = sim1.simulate();
    if (r1.empty() || r1[0].state != 1) {
        std::cerr << "linkSeries circuit did not light bulb\n";
        return 1;
    }

    // Test linkCircle with power in the circle
    CircuitSimulator sim2;
    sim2.addComponent("power", "p");
    sim2.addComponent("bulb", "b");
    sim2.addComponent("switch", "s");
    if (!sim2.linkCircle({"p", "s", "b"})) {
        std::cerr << "linkCircle failed\n";
        return 1;
    }
    sim2.setSwitchState("s", true);
    auto r2 = sim2.simulate();
    if (r2.empty() || r2[0].state != 1) {
        std::cerr << "linkCircle circuit did not light bulb\n";
        return 1;
    }

    std::cout << "linkSeries and linkCircle API tests passed\n";
    return 0;
}
