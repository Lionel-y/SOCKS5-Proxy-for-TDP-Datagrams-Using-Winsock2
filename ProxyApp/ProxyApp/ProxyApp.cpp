// ProxyApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "Proxy.h"

int main(int argc, char* argv[])
{
	std::string proxyAdress = "127.0.0.7";
	unsigned short proxyPort = 8;
	bool printmessages = true;
	const std::string serverCommunicationAdress = "127.192.33.55";//client must use the same
	const unsigned short serverCommunicationPort = 12345;
	bool debugMode = false;

	if (argc == 4)
	{
		std::cout << "You have entered arguments:" << argc << std::endl;

		proxyAdress = argv[1];
		std::string temp = argv[2];
		
		proxyPort = stoi(temp);

		temp = argv[3];
		printmessages = (stoi(temp) == 1);

		std::cout << "IP: " << proxyAdress<< std::endl;
		std::cout << "port: " << proxyPort << std::endl;
		std::cout << "Printing messages: " << (printmessages? " true" : "false") << std::endl;
	}
	else
	{
		std::cout << "Not enough/too many args, using defaults" << std::endl;
	}

	Proxy proxy(proxyAdress, proxyPort, 
		serverCommunicationAdress, serverCommunicationPort, 
		debugMode, printmessages);

	while (true)
	{
		proxy.receive();
	}
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
