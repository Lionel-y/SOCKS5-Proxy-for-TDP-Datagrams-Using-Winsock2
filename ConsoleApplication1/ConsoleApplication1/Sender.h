#pragma once
#include <WS2tcpip.h>
// Include the Winsock library (lib) file
#pragma comment (lib, "ws2_32.lib")
/**
* Classs used for sending data to proxy via SOCKS 5 protocol
It establishes communication  wwith proxy server request to listen to datagram, 
then sends it if server agrees
*/
class Sender
{
public:
	/**
	Ctor
	proxy: adress structure with ip and port of server used to establish communication
	dataTobeSent: buffer with message that we want to senbd
	debugmode: boolean to determine if we should print debug messages.
	*/
	Sender(sockaddr_in& proxy, WSABUF& dataTobeSent, bool debug);
	/**
	Dtor
	*/
	~Sender();
private:
	void error(SOCKET& proxyIPSocket, SOCKET& proxyUDPSocket);
};