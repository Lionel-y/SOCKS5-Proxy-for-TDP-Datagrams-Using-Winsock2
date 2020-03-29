#pragma once
#include <string>
#include <WS2tcpip.h>
// Include the Winsock library (lib) file
#pragma comment (lib, "ws2_32.lib")

class Proxy
{
public:
	/**
	Ctor
	ipAdress string with ip adress
	port: port number
	debugmode: boolean to determine if we should print debug messages.
	*/
	Proxy(std::string ipAdress = "127.123.22.33", const unsigned short port = 8, 
		std::string serverCommunicationAdress = "127.0.0.5", unsigned short serverCommunicationPort = 12345,
		bool debugMode = false, bool printMessages = true);
	/**
	Dtor
	*/
	~Proxy();
	/**
	receives & prints data
	*/
	void receive();
private:
	void receiveUDP();
	void printAddr(sockaddr_in& addrsess, WSABUF& msgbuffer, bool printMsg = true);

	sockaddr_in m_client; // Use to hold the client information (port / ip address)
	sockaddr_in m_expectedClient; // Use to hold the information about the adress client intends to use

	sockaddr_in m_myAdress;// Create a server hint structure for the server

	sockaddr_in m_serverCommunication;

	SOCKET m_inputSocket;
	SOCKET m_commSocket;
	const unsigned short m_originalPort;
	WSAOVERLAPPED m_overlapped;
	bool m_loopCtrl;
	bool m_debugMode;
	bool m_printMessages;
};