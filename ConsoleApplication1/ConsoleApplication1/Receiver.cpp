#include "pch.h"
#include "Receiver.h"
#include "Sender.h"

#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <Windows.h>



// Include the Winsock library (lib) file
#pragma comment (lib, "ws2_32.lib")

Receiver::Receiver(std::string ipAdress, const unsigned short port, 
		std::string serverCommunicationAdress, const unsigned short serverCommunicationPort,
		bool debugMode) :
	m_client(),
	m_serverHint(),
	m_proxy(),
	m_inputSocket(INVALID_SOCKET),
	m_overlapped(),
	m_useUDP(true),
	m_loopCtrl(false),
	m_debugMode(debugMode)
{
	////////////////////////////////////////////////////////////
	// INITIALIZE WINSOCK
	////////////////////////////////////////////////////////////

	// Structure to store the WinSock version. This is filled in
	// on the call to WSAStartup()
	WSADATA data;

	// To start WinSock, the required version must be passed to
	// WSAStartup(). This server is going to use WinSock version
	// 2 so I create a word that will store 2 and 2 in hex i.e.
	// 0x0202
	WORD version = MAKEWORD(2, 2);

	// Start WinSock
	int wsOk = WSAStartup(version, &data);
	if (wsOk != 0)
	{
		// Not ok! Get out quickly
		std::cout << "Can't start Winsock! " << wsOk;
		return;
	}

	// Make sure the Overlapped struct is zeroed out
	SecureZeroMemory((PVOID)&m_overlapped, sizeof(m_overlapped));

	// Create an event handle and setup the overlapped structure.
	m_overlapped.hEvent = WSACreateEvent();
	if (m_overlapped.hEvent == nullptr) 
	{
		std::cout << "setsockopt() failed with error code: " << WSAGetLastError() << std::endl;
		return;
	}

	if (m_useUDP)//TODO tcp
	{
		m_inputSocket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	}
	else
	{
		m_inputSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}

	m_proxy.sin_family = AF_INET;
	m_proxy.sin_port = htons(serverCommunicationPort);
	inet_pton(AF_INET, serverCommunicationAdress.c_str(), &m_proxy.sin_addr);

	m_serverHint.sin_family = AF_INET; // Address format is IPv4
	m_serverHint.sin_port = htons(port); // Convert from little to big endian
	inet_pton(AF_INET, ipAdress.c_str(), &m_serverHint.sin_addr);//use user provided port
	//m_serverHint.sin_addr.s_addr = htonl(INADDR_ANY);//receive from any adress

	unsigned long nonblockingMode = 1;
	wsOk = ioctlsocket(m_inputSocket, FIONBIO, &nonblockingMode);
	if (wsOk != NO_ERROR)
	{
		std::cout << "ioctlsocket failed with error! " << WSAGetLastError() << std::endl;
		return;
	}


	// Try and bind the socket to the IP and port
	if (bind(m_inputSocket, (sockaddr*)&m_serverHint, sizeof(m_serverHint)) == SOCKET_ERROR)
	{
		std::cout << "Can't bind socket! " << WSAGetLastError() << std::endl;
		return;
	}

	if (m_debugMode)
	{
		if (ipAdress == "0.0.0.0")
		{
			std::cout << "listening to all ip's on port: " << htons(m_serverHint.sin_port) << std::endl;
		}
		else
		{
			std::cout << "listening to ip:" << ipAdress << " on port: " << htons(m_serverHint.sin_port) << std::endl;
		}
	}
}


Receiver::~Receiver()
{
	// Close socket
	closesocket(m_inputSocket);

	// Shutdown winsock
	WSACleanup();
	std::cout << "Receiver destroyed WSA cleaned socket closed! " << std::endl;

}

void Receiver::receive()
{
	const unsigned int bufferSize = 1024;
	char buf[bufferSize];
	int clientLength = sizeof(m_client); // The size of the client information

	ZeroMemory(&m_client, clientLength); // Clear the client structure
	ZeroMemory(buf, bufferSize); // Clear the receive buffer

	WSABUF DataBuf;
	DataBuf.len = 1024;
	DataBuf.buf = buf;

	int err = 0;
	DWORD BytesRecv = 0;
	DWORD Flags = 0;
	ZeroMemory(DataBuf.buf, DataBuf.len);


	//Get datagram passing through
	int result = WSARecvFrom(m_inputSocket,
		&DataBuf,
		1,
		&BytesRecv,
		&Flags,
		(sockaddr*)&m_client,
		&clientLength, 
		/*&m_overlapped*/NULL,//to avoid waiting forever we do not have overlaps
		NULL);
	if (result != 0)
	{
		err = WSAGetLastError();

		if (err == WSAEWOULDBLOCK)
		{
			//there is nothing for now let other sockets enjoy receive time
			return;
		}
		else if (err == WSAETIMEDOUT)
		{
			return;// just a timeout everything is fine
		}
		else if (err != WSA_IO_PENDING)
		{
			std::cout << "WSARecvFrom failed with error: " << WSAGetLastError() << std::endl;
			return;
		}
		else
		{
			result = WSAWaitForMultipleEvents(1, &m_overlapped.hEvent, TRUE, INFINITE, TRUE);
			if (result == WSA_WAIT_FAILED)
			{
				std::cout << "WSARecvFrom failed with error: " << WSAGetLastError()<< std::endl;
				return;
			}

			result = WSAGetOverlappedResult(m_inputSocket, &m_overlapped, &BytesRecv,
				FALSE, &Flags);
			if (result == FALSE)
			{
				std::cout << "WSARecvFrom failed with error: " << WSAGetLastError() << std::endl;
				return;
			}
			else if (m_debugMode)
			{
				std::cout << "number of received bytes " << BytesRecv << std::endl;
			}
		}
	}

	std::string message(DataBuf.buf);

	if (m_debugMode)
	{
		printAddr(m_client, message);
	}

	//Fowrward data that was receivedd
	Sender sender(m_proxy, DataBuf, m_debugMode);
}

void Receiver::printAddr(sockaddr_in& addrsess, std::string& msgbuffer)
{
	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addrsess.sin_addr), str, INET_ADDRSTRLEN);

	SYSTEMTIME lt;
	GetLocalTime(&lt);

	std::cout <<"adress used: "<< str<<" - port used: " << htons(addrsess.sin_port) << std::endl;
	std::cout << "Message: " << msgbuffer << 
		" Time:"<<lt.wHour<<":"<<lt.wMinute<<":"<<lt.wSecond<< ":"<<lt.wMilliseconds << std::endl;
}

