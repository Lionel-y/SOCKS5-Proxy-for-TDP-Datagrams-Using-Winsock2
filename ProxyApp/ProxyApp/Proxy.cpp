#include "Proxy.h"
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <Windows.h>
#include <sstream>
#include <bitset>
#include <iomanip>

// Include the Winsock library (lib) file
#pragma comment (lib, "ws2_32.lib")

Proxy::Proxy(std::string ipAdress, const unsigned short port,
	std::string serverCommunicationAdress, unsigned short serverCommunicationPort, 
	bool debugMode, bool printMessages) :
	m_client(),
	m_expectedClient(),
	m_myAdress(),
	m_serverCommunication(),
	m_inputSocket(INVALID_SOCKET),
	m_commSocket(INVALID_SOCKET),
	m_originalPort(port),
	m_overlapped(),
	m_loopCtrl(false),
	m_debugMode(debugMode),
	m_printMessages(printMessages)
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
		std::cout << "Can't start Winsock!!!!!!!!!!!!!!!! " << wsOk;
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

	m_myAdress.sin_family = AF_INET; // Address format is IPv4
	m_myAdress.sin_port = htons(m_originalPort); // Convert from little to big endian
	inet_pton(AF_INET, ipAdress.c_str(), &m_myAdress.sin_addr);//use user provided addr

	m_serverCommunication.sin_family = AF_INET; // Address format is IPv4
	m_serverCommunication.sin_port = htons(serverCommunicationPort); // Convert from little to big endian
	inet_pton(AF_INET, serverCommunicationAdress.c_str(), &m_serverCommunication.sin_addr);

	// Create a socket for coomunitacing to proxy
	m_commSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
		NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_commSocket == INVALID_SOCKET)
	{
		std::cout << "comm socket failed with error " << WSAGetLastError()<< std::endl;
		WSACleanup();
		return;
	}
	// Try and bind the socket to the IP and port
	if (bind(m_commSocket, (sockaddr*)&m_serverCommunication, sizeof(m_serverCommunication)) == SOCKET_ERROR)
	{
		std::cout << "Can't bind socket! " << WSAGetLastError() << std::endl;
	}

	m_inputSocket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, NULL, WSA_FLAG_OVERLAPPED);

	unsigned long nonblockingMode = 1;
	wsOk = ioctlsocket(m_inputSocket, FIONBIO, &nonblockingMode);
	if (wsOk != NO_ERROR)
	{
		std::cout << "ioctlsocket failed with error! " << WSAGetLastError() << std::endl;
	}

	// Try and bind the socket to the IP and port
	if (bind(m_inputSocket, (sockaddr*)&m_myAdress, sizeof(m_myAdress)) == SOCKET_ERROR)
	{
		std::cout << "Can't bind socket! " << WSAGetLastError() << std::endl;
	}

	if (m_debugMode)
	{
		std::cout << "listening to ip:" << ipAdress << " on port: " << htons(m_myAdress.sin_port) << std::endl;
	}
}


Proxy::~Proxy()
{
	// Close socket
	closesocket(m_inputSocket);
	closesocket(m_commSocket);

	// Shutdown winsock
	WSACleanup();
	std::cout << "destroyed WSA cleaned socket closed! " << std::endl;

}

void Proxy::receive()
{
	const unsigned short Protocolversion = 0x05 & 0xFF;//SOCKS5
	const unsigned short CMD = 0x03 & 0xFF;//UDP associate request 
	const unsigned short RSV = 0x00 & 0xFF;//reserved always zeoes??
	const unsigned short ATYP = 0x01 & 0xFF;//o  IP V4 address : X'01'
	const unsigned short BND_ADDR_0 = (m_myAdress.sin_addr.S_un.S_addr >> 24) & 0xFF;// destination adress first byte
	const unsigned short BND_ADDR_1 = (m_myAdress.sin_addr.S_un.S_addr >> 16) & 0xFF;// destination adress second byte
	const unsigned short BND_ADDR_2 = (m_myAdress.sin_addr.S_un.S_addr >> 8) & 0xFF;// destination adress third byte
	const unsigned short BND_ADDR_3 = (m_myAdress.sin_addr.S_un.S_addr >> 0) & 0xFF;// destination adress fourth byte

	const unsigned short BND_PORT_0 = (m_originalPort >> 8) & 0xFF;;//destination port byte 1
	const unsigned short BND_PORT_1 = (m_originalPort >> 0) & 0xFF;;//destination port byte 2
	unsigned short REP = 0x00 & 0xFF;//o  Reply to request for connection 0x00 means success

	int err = 0;
	int result = 0;

	// Listen for incoming connection requests on the created socket
	if (listen(m_commSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cout << "listen function failed with error! " << WSAGetLastError() << std::endl;
	}

	SOCKET clientSocket = accept(m_commSocket, NULL, NULL);
	if (clientSocket == INVALID_SOCKET)
	{
		std::cout << "problem accepting connection (TCP)! " << WSAGetLastError() << std::endl;
		return;
	}

	//Do handshake
	//Send my port and ip
	int bufferSize = 1024;
	char buf[1024];

	ZeroMemory(buf, bufferSize); // Clear the receive buffer

	result = recv(clientSocket, buf, bufferSize, NULL);

	if (result > 0)
	{
		//extract information from request
		unsigned short version = static_cast<unsigned short>(static_cast<unsigned char>(buf[0]));
		unsigned short cmd = static_cast<unsigned short>(static_cast<unsigned char>(buf[1]));
		unsigned short rsv = static_cast<unsigned short>(static_cast<unsigned char>(buf[2]));
		unsigned short aatyp = static_cast<unsigned short>(static_cast<unsigned char>(buf[3]));

		bool connectionSuccedded = true;
		//Check if request was as expected:
		if (version != Protocolversion || cmd != CMD || rsv != RSV || aatyp != ATYP)
		{
			std::cout << "unsupported request" << std::endl;
			std::cout << "version: " << version << " expected: " << Protocolversion << std::endl;
			std::cout << "CMD:" << cmd << " expected: " << CMD << std::endl;
			std::cout << "RSV" << rsv << " expected: " << RSV << std::endl;
			std::cout << "ATYP:" << aatyp << " expected: " << ATYP << std::endl;

			REP = 0x01 & 0xFF;//o  X'01' general SOCKS server failure
			connectionSuccedded = false;
		}

		//extract client ip & port
		unsigned short adress0 = static_cast<unsigned short>(static_cast<unsigned char>(buf[4]));
		unsigned short adress1 = static_cast<unsigned short>(static_cast<unsigned char>(buf[5]));
		unsigned short adress2 = static_cast<unsigned short>(static_cast<unsigned char>(buf[6]));
		unsigned short adress3 = static_cast<unsigned short>(static_cast<unsigned char>(buf[7]));

		unsigned short port0 = static_cast<unsigned short>(static_cast<unsigned char>(buf[8]));
		unsigned short port1 = static_cast<unsigned short>(static_cast<unsigned char>(buf[9]));


		unsigned short proxyPort = (port0 << 8) | (port1 << 0);

		m_expectedClient.sin_family = AF_INET;
		m_expectedClient.sin_addr.S_un.S_addr = (adress0 << 24) | (adress1 << 16) | (adress2 << 8) | (adress3 << 0);
		m_expectedClient.sin_port = proxyPort;

		if (m_debugMode)
		{//print received request
			std::cout << "Bytes received via TCP! " << result << std::endl;
			std::cout << "Request: ";
			for (int i = 0; i < 10; i++)
			{
				std::cout << static_cast<unsigned short>(static_cast<unsigned char>(buf[i]));
			}
			std::cout << std::endl;

			WSABUF temp;
			printAddr(m_expectedClient, temp, false);
		}

		char reply[1024];
		ZeroMemory(reply, 1024); 
		reply[0] = static_cast<unsigned char>(Protocolversion);
		reply[1] = static_cast<unsigned char>(REP);
		reply[2] = static_cast<unsigned char>(RSV);
		reply[3] = static_cast<unsigned char>(ATYP);
		reply[4] = static_cast<unsigned char>(BND_ADDR_0);
		reply[5] = static_cast<unsigned char>(BND_ADDR_1);
		reply[6] = static_cast<unsigned char>(BND_ADDR_2);
		reply[7] = static_cast<unsigned char>(BND_ADDR_3);
		reply[8] = static_cast<unsigned char>(BND_PORT_0);
		reply[9] = static_cast<unsigned char>(BND_PORT_1);
		reply[10] = NULL;

		int replySize = 1024;

		if (m_debugMode)
		{
			std::cout << "reply Sent: ";

			for (int i = 0; i<10; i++)
			{
				std::cout << static_cast<unsigned short>(static_cast<unsigned char>(reply[i]));
			}
			std::cout << std::endl;

		}
		// send reply to client
		err = send(clientSocket, reply, replySize, 0);
		if (err == SOCKET_ERROR || !connectionSuccedded)
		{
			std::cout << "sending conn info failed with error: " << WSAGetLastError() << std::endl;

			closesocket(clientSocket);
			WSACleanup();
			return;
		}
	}

	
	receiveUDP();

	result = closesocket(clientSocket);
	if (result == SOCKET_ERROR)
	{
		std::cout << "closesocket function failed with error! " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}

}

void Proxy::receiveUDP()
{
	char buf[1024];
	int clientLength = sizeof(m_client); // The size of the client information

	ZeroMemory(&m_client, clientLength); // Clear the client structure
	ZeroMemory(buf, 1024); // Clear the receive buffer

	WSABUF DataBuf;
	DataBuf.len = 1024;
	DataBuf.buf = buf;

	int err = 0;
	DWORD BytesRecv = 0;
	DWORD Flags = 0;
	int result = 0;

	ZeroMemory(DataBuf.buf, DataBuf.len);
	int counter = 0;

	while (true)
	{
		counter++;

		if (counter == 100)
		{
			std::cout << "==================================================================: " << std::endl;
			std::cout << "no packet received!" << std::endl;
			std::cout << "==================================================================: " << std::endl;

			break;
		}

		result = WSARecvFrom(m_inputSocket,
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
				continue;
			}
			else if (err == WSAETIMEDOUT)
			{
				continue;// just a timeout everything is fine
			}
			else
			{
				result = WSAWaitForMultipleEvents(1, &m_overlapped.hEvent, TRUE, INFINITE, TRUE);
				if (result == WSA_WAIT_FAILED)
				{
					std::cout << "WSARecvFrom failed WSAWaitForMultipleEvents with error: " << WSAGetLastError() << std::endl;
					continue;
				}

				result = WSAGetOverlappedResult(m_inputSocket, &m_overlapped, &BytesRecv,
					FALSE, &Flags);
				if (result == FALSE)
				{
					std::cout << "WSARecvFrom failed WSAGetOverlappedResult with error: " << WSAGetLastError() << std::endl;
					continue;
				}
				else if (m_debugMode)
				{
					std::cout << "number of received bytes " << BytesRecv << std::endl;
				}
			}
		}

		//extract client ip & port
		unsigned short adress0 = static_cast<unsigned short>(static_cast<unsigned char>(DataBuf.buf[4]));
		unsigned short adress1 = static_cast<unsigned short>(static_cast<unsigned char>(DataBuf.buf[5]));
		unsigned short adress2 = static_cast<unsigned short>(static_cast<unsigned char>(DataBuf.buf[6]));
		unsigned short adress3 = static_cast<unsigned short>(static_cast<unsigned char>(DataBuf.buf[7]));

		unsigned short port0 = static_cast<unsigned short>(static_cast<unsigned char>(DataBuf.buf[8]));
		unsigned short port1 = static_cast<unsigned short>(static_cast<unsigned char>(DataBuf.buf[9]));


		unsigned short proxyPort = (port0 << 8) | (port1 <<0);
		sockaddr_in clientAdress;
		clientAdress.sin_family = AF_INET;
		clientAdress.sin_addr.S_un.S_addr = (adress0 << 24) | (adress1 << 16) | (adress2 << 8) | (adress3 << 0);
		clientAdress.sin_port = htons(proxyPort);

		//  The UDP relay server MUST acquire from the SOCKS server the expected
		//	IP address of the client that will send datagrams to the BND.PORT
		//	given in the reply to UDP ASSOCIATE.It MUST drop any datagrams
		//	arriving from any source IP address other than the one recorded for
		//	the particular association.
		if (m_expectedClient.sin_addr.S_un.S_addr != clientAdress.sin_addr.S_un.S_addr &&
			m_expectedClient.sin_port != clientAdress.sin_port)
		{
			std::cout << "unexpected client adress!!!" << std::endl;

			char str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(clientAdress.sin_addr), str, INET_ADDRSTRLEN);

			std::cout << "client adress used: " << str << " - port used: " << htons(clientAdress.sin_port) << std::endl;
		}
		else if (m_printMessages)
		{
			printAddr(m_client, DataBuf);
		}

		break;
	}
}

void Proxy::printAddr(sockaddr_in& addrsess, WSABUF& msgbuffer, bool printMsg)
{
	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addrsess.sin_addr), str, INET_ADDRSTRLEN);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	std::cout << "==================================================================: " << std::endl;
	std::cout << "Adress used: " << str << " - port used: " << htons(addrsess.sin_port) << std::endl;
	if (printMsg)
	{
		std::cout << "Message: ";
		for (int i = 10; i < 1023; i++)//skip first 10 bytes that show header
		{
			if (msgbuffer.buf[i] != '\0')
			{
				std::cout << msgbuffer.buf[i];
			}
		}
		std::cout <<" Time:" << lt.wHour << ":" << lt.wMinute << ":" << lt.wSecond << ":" << lt.wMilliseconds << std::endl;
	}
	std::cout << "==================================================================: " << std::endl;
}

//6.  Replies
//
//The SOCKS request information is sent by the client as soon as it has
//established a connection to the SOCKS server, and completed the
//authentication negotiations.The server evaluates the request, and
//returns a reply formed as follows :
//
//+---- + ---- - +------ - +------ + ---------- + ---------- +
//| VER | REP | RSV | ATYP | BND.ADDR | BND.PORT |
//+---- + ---- - +------ - +------ + ---------- + ---------- +
//| 1 | 1 | X'00' | 1 | Variable | 2 |
//+---- + ---- - +------ - +------ + ---------- + ---------- +
//
//Where:
//
//o  VER    protocol version : X'05'
//o  REP    Reply field :
//o  X'00' succeeded
//o  X'01' general SOCKS server failure
//o  X'02' connection not allowed by ruleset
//o  X'03' Network unreachable
//o  X'04' Host unreachable
//o  X'05' Connection refused
//o  X'06' TTL expired
//o  X'07' Command not supported
//o  X'08' Address type not supported
//o  X'09' to X'FF' unassigned
//o  RSV    RESERVED
//o  ATYP   address type of following address
//
//
//
//Leech, et al                Standards Track[Page 5]
//
//RFC 1928                SOCKS Protocol Version 5              March 1996
//
//
//o  IP V4 address : X'01'
//o  DOMAINNAME : X'03'
//o  IP V6 address : X'04'
//o  BND.ADDR       server bound address
//o  BND.PORT       server bound port in network octet order
//
//Fields marked RESERVED(RSV) must be set to X'00'.
