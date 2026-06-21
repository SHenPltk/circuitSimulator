#include "circuit/circuit_simulator.h"
#include <iostream>

using namespace circuit;

int main()
{
	CircuitSimulator sim;

	sim.addComponent("power", "E");
	sim.addComponent("resistor", "R");
	sim.addComponents("bulb", std::vector<std::string>{"L1", "L2","L3","L4"});
	sim.addComponents("switch", std::vector<std::string>{"a", "b", "c","d"});

	sim.linkCircle(std::vector<std::string>{"E","R","c","L4","b","L2","L1"});
	sim.linkSeries("R.r",std::vector<std::string>{"L3","d"},"c.r");
	sim.addLink("R.r","a.l");	sim.addLink("a.r","L1.l");
	sim.addLink("L3.r","b.r");
	sim.addLink("b.l","E.p");

	sim.addSwitchGroup("g", std::vector<std::string>{"d","c", "b", "a"});

	for (int i = 0; i < 1 << 4; i++)
	{
		sim.setSwitchGroupState("g", i);
		std::cout << sim.formatOutput("@s @ | @b @\n");
	}

	std::cout << sim.toString();
	return 0;
}