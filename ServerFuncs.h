#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <algorithm>

#define BUFF_SIZE 1024
#define TRACE 0
#define DELETE 1
#define PUT 2
#define POST 3
#define HEAD 4
#define GET 5
#define OPTIONS 6


struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int requestType;	// Sending sub-type
	char buffer[BUFF_SIZE];
	int dataLen;
	time_t lastCalled;
};

const int HTTP_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int SEND_TIME = 1;
const int SEND_SECONDS = 2;

bool addSocket(SOCKET id, int what, int socketsCount, SocketState* sockets);
void removeSocket(int index, int socketsCount, SocketState* sockets);
void acceptConnection(int index, int socketCount, SocketState* sockets);
void receiveMessage(int index, int socketCount, SocketState* sockets);
void sendMessage(int index, SocketState* sockets);

void HandleGET(char *sendBuff,int index,SocketState* sockets);
void HandleTRACE(char* sendBuff, int index, SocketState* sockets);
void HandleDELETE(char* sendBuff, int index, SocketState* sockets);