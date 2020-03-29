#pragma once
#include <string>
#include <WS2tcpip.h>
// Include the Winsock library (lib) file
#pragma comment (lib, "ws2_32.lib")
//Class for listening for listening single or multiple ip's on specific port. 
//upon catching a datagram it forwards it using 'Sender' class
class Receiver
{
public:
	/**
	Ctor
	ipAdress string with ip adress to listen to. 0.0.0.0 listens to all ip's
	port: port number to listen to
	serverCommunicationAdress string with ip adress to to establish comm with server
	serverCommunicationPort: port number to establish comm with server
	debugmode: boolean to determine if we should print debug messages.
	*/
	Receiver(std::string ipAdress/* = "122.122.122.122"*/, const unsigned short port/* = 54000*/, 
			std::string serverCommunicationAdress, const unsigned short serverCommunicationPort,
			bool debugMode /*= true*/);
	/**
	Dtor
	*/
	~Receiver();
	/**
	receives & prints data if debug enabled
	forwards data to SOCKS5 server
	*/
	void receive();
private:
	void printAddr(sockaddr_in& addrsess, std::string& msgbuffer);

	sockaddr_in m_client; // Use to hold the client information (port / ip address)

	sockaddr_in m_serverHint;// Create a server hint structure for the server

	sockaddr_in m_proxy;// Create a server hint structure for the server

	SOCKET m_inputSocket;
	WSAOVERLAPPED m_overlapped;
	const bool m_useUDP;
	bool m_loopCtrl;
	bool m_debugMode;
};

