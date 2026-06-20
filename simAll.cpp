#include "circuit/circuit_simulator.h"
#include <iostream>

using namespace circuit;

int main()
{
	CircuitSimulator sim;

	sim.addComponent("power", "E");
	sim.addComponent("resistor", "R");
	sim.addComponents("bulb", std::vector<std::string>{"L1", "L2"});
	sim.addComponents("switch", std::vector<std::string>{"a", "b", "c", "d"});

	sim.addLink("E.p", "R.l");
	sim.addLink("R.r", "L1.l");
	sim.addLink("L1.r", "c.l");
	sim.addLink("c.r", "E.n");
	sim.addLink("R.r", "d.l");
	sim.addLink("d.r", "b.l");
	sim.addLink("b.r", "L2.l");
	sim.addLink("L2.r", "E.n");
	sim.addLink("L1.r", "a.l");
	sim.addLink("a.r", "d.r");

	sim.addSwitchGroup("g", std::vector<std::string>{"d", "c", "b", "a"});

	for (int i = 0; i < 1 << 4; i++)
	{
		sim.setSwitchGroupState("g", i);
		auto results = sim.simulate();
		for (const auto &result : results)
		{
			std::cout << result.state << ' ';
		}
		std::cout << std::endl;
	}

	return 0;
}