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

    // Test addLinks and simulate2 with two parallel branches.
    CircuitSimulator sim3;
    sim3.addComponent("power", "p");
    sim3.addComponents("switch", {"s1", "s2"});
    sim3.addComponents("bulb", {"b1", "b2"});
    if (!sim3.addLinks({"p.+", "s1.1", "s2.1"})) {
        std::cerr << "addLinks for positive node failed\n";
        return 1;
    }
    if (!sim3.addLink("s1.2", "b1.1")) {
        std::cerr << "branch 1 link failed\n";
        return 1;
    }
    if (!sim3.addLink("s2.2", "b2.1")) {
        std::cerr << "branch 2 link failed\n";
        return 1;
    }
    if (!sim3.addLinks({"p.-", "b1.2", "b2.2"})) {
        std::cerr << "addLinks for negative node failed\n";
        return 1;
    }
    sim3.setSwitchState("s1", true);
    sim3.setSwitchState("s2", false);
    if (sim3.simulate2() != 0b10U) {
        std::cerr << "simulate2 returned unexpected bit pattern\n";
        return 1;
    }

    std::cout << "linkSeries, linkCircle, addLinks and simulate2 API tests passed\n";
    return 0;
}
