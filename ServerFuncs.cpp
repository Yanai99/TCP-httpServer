#include "ServerFuncs.h"

bool addSocket(SOCKET id, int what, int socketsCount, SocketState* sockets)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].dataLen = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index, int socketsCount, SocketState* sockets)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index, int socketCount, SocketState* sockets)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE, socketCount, sockets) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index, int socketCount, SocketState* sockets)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].dataLen;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "HTTP Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index, socketCount, sockets);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index, socketCount, sockets);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "HTTP Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].dataLen += bytesRecv;

		if (sockets[index].dataLen > 0)
		{

			// this section handles the recieved requests
			if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].requestType = TRACE;
				return;
			}
			else if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].requestType = GET;
				return;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].requestType = DELETE;
				return;
			}
		}
	}

}

void sendMessage(int index, SocketState* sockets)
{
	int bytesSent = 0;
	char sendBuff[BUFF_SIZE];

	SOCKET msgSocket = sockets[index].id;
	if (sockets[index].requestType == GET)
	{
		HandleGET(sendBuff, index, sockets);
	}
	else if (sockets[index].requestType == TRACE)
	{
		HandleTRACE(sendBuff, index, sockets);
	}
	else if (sockets[index].requestType == DELETE)
	{
		HandleDELETE(sendBuff, index, sockets);
	}

	// cut the old request using strtok to all methods or null

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "HTTP Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets[index].send = IDLE;
}

#include <string>

// Get the value of a query parameter from an HTTP request message
std::string GetQueryParam(const char* request, const char* paramName) 
{
	std::string queryString(request);
	std::string paramValue;
	std::size_t startPos = queryString.find(paramName);
	if (startPos != std::string::npos) {
		startPos += std::strlen(paramName) + 1; // move past the parameter name and the equals sign
		std::size_t end_pos = min(queryString.find('&', startPos), queryString.find(' ', startPos));
		if (end_pos == std::string::npos) {
			end_pos = queryString.length();
		}
		paramValue = queryString.substr(startPos, end_pos - startPos);
	}
	return paramValue;
}


void HandleGET(char *sendBuff, int index, SocketState* sockets)
{
	string message;
	string fileName = "NotFound";
	// Get the requested URL from the request string
	char* request = sockets[index].buffer;
	string queryParams = GetQueryParam(request,"lang");
	if(queryParams.size() == 0 || queryParams == "en")
		fileName = "index.html";
	else if(queryParams == "fr")
		 fileName = "indexfr.html";
	else if (queryParams == "he")
		 fileName = "indexhe.html";


	// Open the requested file
	char fileAddress[256];
	sprintf(fileAddress, "C:\\htmlFiles\\%s", fileName.c_str());
	FILE* file = fopen(fileAddress, "rb");

	if (file == NULL) {
		// File not found - send error response
		strcpy(sendBuff, "HTTP/ 1.1 404 Not Found\r\n");
		strcat(sendBuff, "Content-Type: text/html\r\n\r\n");
		strcat(sendBuff, "<html><body><h1>404 Not Found</h1></body></html>");
	}
	else {
		// File found - read contents into buffer
		fseek(file, 0, SEEK_END);
		int fileSize = ftell(file);
		fseek(file, 0, SEEK_SET);
		char* fileBuffer = new char[fileSize];
		fread(fileBuffer, 1, fileSize, file);
		fileBuffer[fileSize - 1] = '\0';
		fclose(file);

		// Send response with file contents
		message += "HTTP/1.1 200 OK\r\n";
		message += "Content-Type: text/html\r\n";
		message += "Content-Length: ";
		char fileSizeStr[16];
		sprintf(fileSizeStr, "%d", fileSize);
		message += fileSizeStr;
		message += "\r\n\r\n";
		message += fileBuffer;
		delete[] fileBuffer;
		strcpy(sendBuff, message.c_str());
	}
}

void HandleTRACE(char* sendBuff, int index, SocketState* sockets) {
	// build the response message
	const char* responseTemplate = "HTTP/1.1 200 OK\r\nContent-Type: message/http\r\n\r\nTRACE request received:\r\n%s\r\n";
	sprintf(sendBuff, responseTemplate, sockets[index].buffer);

	// print some debug information
	cout << "HTTP Server: Handled TRACE request: \"" << sockets[index].buffer << "\"" << endl;
}

void HandleDELETE(char* sendBuff, int index, SocketState* sockets) 
{
	char reqParams[BUFF_SIZE];
	strcpy(reqParams, &sockets[index].buffer[7]);
	string reqP(reqParams);
	string fileName = reqP.substr(0, reqP.find(".html"));
	fileName += ".html";
	string fileAddress = "C:\\htmlFiles\\" + fileName;

	int res = remove(fileAddress.c_str());
	if (res == 0) {
		sprintf(sendBuff, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>File deleted successfully.</h1></body></html>");
	}
	else {
		sprintf(sendBuff, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>File not found.</h1></body></html>");
	}
}
