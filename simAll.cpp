#include "circuit/circuit_simulator.h"
#include <iostream>

using namespace circuit;

int main()
{
	CircuitSimulator sim;

	sim.addComponent("power", "E");
	sim.addComponent("resistor", "R");
	sim.addComponents("bulb", std::vector<std::string>{"L1", "L2"});
	sim.addComponents("switch", std::vector<std::string>{"a", "b", "c"});

	sim.linkCircle(std::vector<std::string>{"E","R","a","L1"});
	sim.linkSeries("R.r",std::vector<std::string>{"b","c"},"E.p");
	sim.addLink("a.r","L2.l");	sim.addLink("L2.r","c.l");

	sim.addSwitchGroup("g", std::vector<std::string>{"c", "b", "a"});

	for (int i = 0; i < 1 << 3; i++)
	{
		sim.setSwitchGroupState("g", i);
		std::cout << sim.formatOutput("@s @ | @b @\n");
	}

	std::cout << sim.toString();
	return 0;
}