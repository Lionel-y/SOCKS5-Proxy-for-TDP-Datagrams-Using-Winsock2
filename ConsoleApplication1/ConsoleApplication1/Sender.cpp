#include "pch.h"
#include "Sender.h"
#include <WS2tcpip.h>
#include <iostream>
#include <string>
#include <Windows.h>
#include <cstring>
#include <sstream>
#include <bitset>
#include <iomanip>


#define ASCII_OFFSET 48;

// Include the Winsock library (lib) file
#pragma comment (lib, "ws2_32.lib")

Sender::Sender(sockaddr_in& serverCommunicationAdress, WSABUF& dataTobeSent, bool debug)
{
    struct sockaddr_in senderAdress;//Any untaken adress/port for sending
    senderAdress.sin_family = AF_INET;
    inet_pton(AF_INET, "127.33.22.11", &senderAdress.sin_addr);
    senderAdress.sin_port = htons(1515);
    int senderAdressSize = sizeof(senderAdress);

    const unsigned short Protocolversion = 0x05;//SOCKS5
    const unsigned short CMD = 0x03;//UDP associate request 
    const unsigned short RSV = 0x00;//reserved always zeoes??
    const unsigned short FRAGMENT = 0x00; //value of X'00' indicates that this datagram is standalone
    const unsigned short ATYP = 0x01;//o  IP V4 address : X'01'

    const unsigned short DST_ADDR_0 = (senderAdress.sin_addr.S_un.S_addr >> 24) & 0xFF;// destination adress first byte
    const unsigned short DST_ADDR_1 = (senderAdress.sin_addr.S_un.S_addr >> 16) & 0xFF;// destination adress second byte
    const unsigned short DST_ADDR_2 = (senderAdress.sin_addr.S_un.S_addr >> 8) & 0xFF;// destination adress third byte
    const unsigned short DST_ADDR_3 = (senderAdress.sin_addr.S_un.S_addr >> 0) & 0xFF;// destination adress fourth byte

    const unsigned short DST_PORT_0 = (senderAdress.sin_port >> 8) & 0xFF;//destination port byte 1
    const unsigned short DST_PORT_1 = (senderAdress.sin_port >> 0) & 0xFF;//destination port byte 2

    const unsigned short REP_success = 0x00;//Reply from proxy0 success anything else is a failure


    WSADATA wsaData;

    SOCKET proxyUDPSocket = INVALID_SOCKET;
    SOCKET proxyIPSocket = INVALID_SOCKET;

    int proxySize = sizeof(serverCommunicationAdress);

    DWORD BytesSent = 0;
    DWORD Flags = 0;

    int rc, err;

    // Initialize Winsock
    // Load Winsock
    rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != 0)
    {
        std::cout << "Unable to load Winsock " << rc << std::endl;
        return;
    }

    //---------------------------------------------
    // Create a socket for sending data
    proxyUDPSocket = WSASocket(AF_INET, SOCK_DGRAM, 
        IPPROTO_UDP, NULL, 
        0, WSA_FLAG_OVERLAPPED);

    if (proxyUDPSocket == INVALID_SOCKET)
    {
        std::cout << "socket failed with error " << WSAGetLastError()<< std::endl;
        error(proxyIPSocket, proxyUDPSocket);
        return;
    }

    // Create a socket for coomunitacing to proxy
    proxyIPSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 
        NULL, 0, WSA_FLAG_OVERLAPPED);
    if (proxyIPSocket == INVALID_SOCKET)
    {
        std::cout << "comm socket failed with error " << WSAGetLastError()<< std::endl;
        error(proxyIPSocket, proxyUDPSocket);
        return;
    }

    for (int i = 0; i < 10; i++)
    {
        // Connect to server.
        rc = connect(proxyIPSocket, (SOCKADDR*)&serverCommunicationAdress, proxySize);
        if (rc == SOCKET_ERROR)
        {
            err = WSAGetLastError();
            if (i != 10)
            {
                if (err == WSAECONNREFUSED || err == WSAENETUNREACH || err == WSAETIMEDOUT)
                {
                    continue;
                }
            }

            std::cout << "connect function failed with error " << WSAGetLastError() << std::endl;

            error(proxyIPSocket, proxyUDPSocket);
            return;
        }
        else
        {
            break;
        }
    }

    int requestSize = 1024;
    char request[1024];
    ZeroMemory(request, requestSize);
    request[0] = static_cast<unsigned char>(Protocolversion);
    request[1] = static_cast<unsigned char>(CMD);
    request[2] = static_cast<unsigned char>(RSV);
    request[3] = static_cast<unsigned char>(ATYP);
    request[4] = static_cast<unsigned char>(DST_ADDR_0);
    request[5] = static_cast<unsigned char>(DST_ADDR_1);
    request[6] = static_cast<unsigned char>(DST_ADDR_2);
    request[7] = static_cast<unsigned char>(DST_ADDR_3);
    request[8] = static_cast<unsigned char>(DST_PORT_0);
    request[9] = static_cast<unsigned char>(DST_PORT_1);
    request[10] = NULL;


    const char* sendbuf = request;//buffer;

    if (debug)
    {
        std::cout << "send request: ";
        for (int i = 0; i < 10; i++)
        {
            std::cout << static_cast<unsigned short>(static_cast<unsigned char>(sendbuf[i]));
        }
        std::cout << std::endl;
    }

    // Send an initial buffer
    rc = send(proxyIPSocket, sendbuf, requestSize, 0);
    if (rc == SOCKET_ERROR) 
    {
        std::cout << "send failed with error " << WSAGetLastError() << std::endl;
        error(proxyIPSocket, proxyUDPSocket);
        return;
    }

    // shutdown the connection since no more data will be sent
    rc = shutdown(proxyIPSocket, SD_SEND);
    if (rc == SOCKET_ERROR) 
    {
        std::cout << "shutdown failed with error " << WSAGetLastError() << std::endl;
        error(proxyIPSocket, proxyUDPSocket);
        return;
    }

    const int bufferSize = 1024;
    char buffer[bufferSize];
    ZeroMemory(buffer, bufferSize);

    //get reply from proxy
    rc = recv(proxyIPSocket, buffer, bufferSize, NULL);

    if (rc > 0)
    {
        //extract information from reply
        unsigned short version = static_cast<unsigned short>(static_cast<unsigned char>(buffer[0]));
        unsigned short reply = static_cast<unsigned short>(static_cast<unsigned char>(buffer[1]));
        unsigned short rsv = static_cast<unsigned short>(static_cast<unsigned char>(buffer[2]));
        unsigned short aatyp = static_cast<unsigned short>(static_cast<unsigned char>(buffer[3]));

        //Check if reply is as expected:
        if (version != Protocolversion || reply != REP_success || rsv != RSV || aatyp != ATYP)
        {
            std::cout << "unsupported request" << std::endl;
            std::cout << "version: " << version << " expected: " << Protocolversion << std::endl;
            std::cout << "REP:     " << reply << " expected: " << REP_success << std::endl;
            std::cout << "RSV:     " << rsv << " expected: " << RSV << std::endl;
            std::cout << "ATYP:    " << aatyp << " expected: " << ATYP << std::endl;

            error(proxyIPSocket, proxyUDPSocket);
            return;
        }

        if (debug)
        {//print received reply
            std::cout << "Bytes received via TCP! " << rc << std::endl;
            for (int i = 0; i < 10; i++)
            {
                std::cout<< static_cast<unsigned short>(static_cast<unsigned char>(buffer[i]));
            }
            std::cout << std::endl;
        }
    }
    else
    {
        std::cout << "recv reply from proxy failed with " << WSAGetLastError() << std::endl;
        error(proxyIPSocket, proxyUDPSocket);
        return;
    }

    //extract ip and port of udp proxy server from reply
    unsigned short adress0 = static_cast<unsigned short>(static_cast<unsigned char>(buffer[4]));
    unsigned short adress1 = static_cast<unsigned short>(static_cast<unsigned char>(buffer[5]));
    unsigned short adress2 = static_cast<unsigned short>(static_cast<unsigned char>(buffer[6]));
    unsigned short adress3 = static_cast<unsigned short>(static_cast<unsigned char>(buffer[7]));

    unsigned short port0 = static_cast<unsigned short>(static_cast<unsigned char>(buffer[8]));
    unsigned short port1 = static_cast<unsigned short>(static_cast<unsigned char>(buffer[9]));


    unsigned short proxyPort = (port0 << 8) | (port1 << 0);

    struct sockaddr_in proxyAdressUDP;
    proxyAdressUDP.sin_family = AF_INET;
    proxyAdressUDP.sin_addr.S_un.S_addr = (adress0 << 24) | (adress1 << 16) | (adress2 << 8) | (adress3 << 0);
    proxyAdressUDP.sin_port = htons(proxyPort);

    if (debug)
    {
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(proxyAdressUDP.sin_addr), str, INET_ADDRSTRLEN);

        SYSTEMTIME lt;
        GetLocalTime(&lt);

        std::cout << "Proxy adress used: " << str << " - port used: " << htons(proxyAdressUDP.sin_port) << std::endl;
    }

    //---------------------------------------------
    // Bind the sending socket to the proxy structure
    // that has the internet address family, local IP address
    // and specified port number.  
    rc = bind(proxyUDPSocket, (struct sockaddr*)& senderAdress, senderAdressSize);
    if (rc == SOCKET_ERROR)
    {
        std::cout << "bind failed with " << WSAGetLastError()<< std::endl;
        error(proxyIPSocket, proxyUDPSocket);
        return;
    }
    

    //Form  UDP request header
    //Each UDP datagram carries a UDP request header with it :
    int datagramSize = 1024;
    char datagram[1024];

    ZeroMemory(datagram, datagramSize);

    datagram[0] = static_cast<unsigned char>(RSV);
    datagram[1] = static_cast<unsigned char>(RSV);
    datagram[2] = static_cast<unsigned char>(FRAGMENT);
    datagram[3] = static_cast<unsigned char>(ATYP);
    datagram[4] = static_cast<unsigned char>(DST_ADDR_0);
    datagram[5] = static_cast<unsigned char>(DST_ADDR_1);
    datagram[6] = static_cast<unsigned char>(DST_ADDR_2);
    datagram[7] = static_cast<unsigned char>(DST_ADDR_3);
    datagram[8] = static_cast<unsigned char>(DST_PORT_0);
    datagram[9] = static_cast<unsigned char>(DST_PORT_1);

    //add original data that is meant to be sent
    for (int i = 0; i < 1023-10; i++)
    {
        datagram[i + 10] = dataTobeSent.buf[i];
    }


    if (debug)
    {
        std::cout << "send datagram: ";

        for (int i = 0; i < 1023; i++)
        {
            if (i < 10)
            {//Printheader as numbers, data as ascii
                std::cout << static_cast<unsigned short>(static_cast<unsigned char>(datagram[i]));
            }
            else if (datagram[i] != '\0')
            {
                std::cout << datagram[i];
            }
        }
        std::cout << std::endl;
    }

    //Send datagram with prepared header and data
    WSABUF newDataTobeSent;
    newDataTobeSent.len = datagramSize;
    newDataTobeSent.buf = datagram;

    rc = WSASendTo(proxyUDPSocket, &newDataTobeSent, 1,
        &BytesSent, Flags, (SOCKADDR*)&proxyAdressUDP,
        proxySize, NULL/*&Overlapped*/, NULL);

    if ((rc == SOCKET_ERROR) && (WSA_IO_PENDING != (WSAGetLastError())))
    {
        std::cout << "WSASendTo failed with error " << WSAGetLastError()<< std::endl;
        error(proxyIPSocket, proxyUDPSocket);
        return;
    }
    else if (rc != 0)
    {
        std::cout << "WSASendTo failed with error " << WSAGetLastError()<< std::endl;
    }

    if (debug)
    {
        std::cout << "Number of sent bytes = " << BytesSent << std::endl;
    }

    error(proxyIPSocket, proxyUDPSocket);
}

void Sender::error(SOCKET& proxyIPSocket, SOCKET& proxyUDPSocket)
{
    int rc = 0;
    // When the application is finished sending, close the sockets
    rc = closesocket(proxyIPSocket);
    if (rc == SOCKET_ERROR)
    {
        closesocket(proxyUDPSocket);

        std::cout << "closesocket ip function failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }

    rc = closesocket(proxyUDPSocket);
    if (rc == SOCKET_ERROR)
    {
        std::cout << "closesocket udp function failed with error " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }

    //---------------------------------------------
    // Clean up and quit.
    WSACleanup();
    return;

}


Sender::~Sender()
{
}
//7.  Procedure for UDP - based clients
//
//A UDP - based client MUST send its datagrams to the UDP relay server at
//the UDP port indicated by BND.PORT in the reply to the UDP ASSOCIATE
//request.If the selected authentication method provides
//encapsulation for the purposes of authenticity, integrity, and /or
//confidentiality, the datagram MUST be encapsulated using the
//appropriate encapsulation.Each UDP datagram carries a UDP request
//header with it :
//+ ---- + ------ + ------ + ---------- + ---------- + ---------- +
//| RSV | FRAG | ATYP | DST.ADDR | DST.PORT | DATA |
//+---- + ------ + ------ + ---------- + ---------- + ---------- +
//| 2 | 1 | 1 | Variable | 2 | Variable |
//+---- + ------ + ------ + ---------- + ---------- + ---------- +
//
//The fields in the UDP request header are :
//
//o  RSV  Reserved X'0000'
//o  FRAG    Current fragment number
//o  ATYP    address type of following addresses :
//o  IP V4 address : X'01'
//o  DOMAINNAME : X'03'
//o  IP V6 address : X'04'
//o  DST.ADDR       desired destination address
//o  DST.PORT       desired destination port
//o  DATA     user data



//The SOCKS request is formed as follows :
//
//+---- + ---- - +------ - +------ + ---------- + ---------- +
//| VER | CMD | RSV | ATYP | DST.ADDR | DST.PORT |
//+---- + ---- - +------ - +------ + ---------- + ---------- +
//| 1 | 1 | X'00' | 1 | Variable | 2 |
//+---- + ---- - +------ - +------ + ---------- + ---------- +
//
//Where:
//
//o  VER    protocol version : X'05'
//o  CMD
//o  CONNECT X'01'
//o  BIND X'02'
//o  UDP ASSOCIATE X'03'
//o  RSV    RESERVED
//o  ATYP   address type of following address
//o  IP V4 address : X'01'
//o  DOMAINNAME : X'03'
//o  IP V6 address : X'04'
//o  DST.ADDR       desired destination address
//o  DST.PORT desired destination port in network octet
//order

//UDP ASSOCIATE
//
//The UDP ASSOCIATE request is used to establish an association within
//the UDP relay process to handle UDP datagrams.The DST.ADDRand
//DST.PORT fields contain the addressand port that the client expects
//to use to send UDP datagrams on for the association.The server MAY
//use this information to limit access to the association.If the
//client is not in possesion of the information at the time of the UDP
//ASSOCIATE, the client MUST use a port numberand address of all
//zeros.
//
//A UDP association terminates when the TCP connection that the UDP
//ASSOCIATE request arrived on terminates.
//
//In the reply to a UDP ASSOCIATE request, the BND.PORTand BND.ADDR
//fields indicate the port number / address where the client MUST send
//UDP request messages to be relayed.
