// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "Receiver.h"
#include "ConfigList.h"

#include <memory>
#include <vector>
#include <iostream>
#include <string>

int main()
{
	//ip and port to be used for establishing communication with proxy server
	const std::string serverCommunicationAdress = "127.192.33.55";//proxy must use the same
	const unsigned short serverCommunicationPort = 12345;
	const bool debugRun = false;//ReadFromUser?

	//getCongig
	ConfigList configList;
	std::vector<std::pair<std::string, unsigned short>>& config = configList.getList();

	std::vector<std::unique_ptr<Receiver>> receivers;

	//init receivers defined in configList
	for (auto &singleConfig : config)
	{
		receivers.emplace_back(std::make_unique<Receiver>(singleConfig.first, singleConfig.second, 
			serverCommunicationAdress, serverCommunicationPort, debugRun));
	}

	std::cout << "VPN tunnel app" << std::endl;

	while (true)
	{
				
		for (auto& singleReceiver : receivers)
		{
			if (singleReceiver)
			{
				singleReceiver->receive();
				//std::cout << "received smth listen to another" << std::endl;
			}
			else
			{
				std::cerr << "bad receiver created | nullptr" << std::endl;
				return 0;
			}
			//proxy.receive();
		}
	}
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
