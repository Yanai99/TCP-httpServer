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
			sockets[i].lastCalled = time(0);
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
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].requestType = HEAD;
				return;
			}
			else if (strncmp(sockets[index].buffer, "POST", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].requestType = POST;
				return;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].requestType = PUT;
				return;
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].requestType = OPTIONS;
				return;
			}
		}
	}

}

void sendMessage(int index, SocketState* sockets)
{
	int bytesSent = 0;
	char sendBuff[BUFF_SIZE];

	//set the time
	sockets[index].lastCalled = time(0);

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
	else if (sockets[index].requestType == POST)
	{
		HandlePOST(sendBuff, index, sockets);
	}
	else if (sockets[index].requestType == PUT)
	{
		HandlePUT(sendBuff, index, sockets);
	}
	else if (sockets[index].requestType == HEAD)
	{
		HandleHEAD(sendBuff, index, sockets);
	}
	else if (sockets[index].requestType == OPTIONS)
	{
		HandleOPTIONS(sendBuff, index, sockets);
	}

	sockets[index].lastCalled = time(0);
	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "HTTP Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets[index].send = IDLE;

	// Handle the buffer
	int contLen = FindContLength(sockets[index].buffer);
	
	removeRequest(sockets[index].buffer , sockets[index].dataLen , contLen);
}

void removeRequest(char* buffer, int& length,int contLen) 
{
	// Find the position of the first occurrence of "\r\n\r\n" that marks the end of the first request
	char* pos = std::strstr(buffer, "\r\n\r\n");
	if (pos != nullptr) {
		// Calculate the new length and shift the remaining data to the beginning
		int newPos = pos - buffer + 4; // +4 to skip "\r\n\r\n"
		int newLength = length - newPos - contLen;
		std::memmove(buffer, buffer + newPos, newLength);

		// Update the buffer length
		length = newLength;
	}
}

int FindContLength(string request)
{
	// Find the position of "Content-Length"
	std::size_t startPos = request.find("Content-Length:");
	if (startPos != std::string::npos) {
		// Find the position of the value after "Content-Length"
		std::size_t valuePos = startPos + 15; // length of "Content-Length:" is 15
		std::size_t endPos = request.find("\r\n", valuePos);

		// Extract the Content-Length value
		std::string contentLength = request.substr(valuePos, endPos - valuePos);

		// Remove leading and trailing white spaces
		contentLength.erase(0, contentLength.find_first_not_of(" \t"));
		contentLength.erase(contentLength.find_last_not_of(" \t") + 1);

		// Print the Content-Length value
		std::cout << "Content-Length: " << contentLength << std::endl;
		return atoi(contentLength.c_str());
	}
	return -1;
}

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
	time_t now = time(0);
	char* dateTime = ctime(&now);
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
		message+= "HTTP/ 1.1 404 Not Found\r\n";
		message += "Content-Type: text/html\r\n\r\n";
		message += "Date: ";
		message += dateTime;
		message += "<html><body><h1>404 Not Found</h1></body></html>";
		strcpy(sendBuff, message.c_str());
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
		message += "Date: ";
		message += dateTime;
		message += "Content-Length: ";
		char fileSizeStr[16];
		sprintf(fileSizeStr, "%d", fileSize);
		message += fileSizeStr;
		message += "\r\n\r\n";
		message += fileBuffer;
		message += "\r\n\r\n";
		delete[] fileBuffer;
		strcpy(sendBuff, message.c_str());
	}
}

void HandleTRACE(char* sendBuff, int index, SocketState* sockets) {
	time_t now = time(0);
	char* dateTime = ctime(&now);
	string message;
	// build the response message
	message = "HTTP/1.1 200 OK\r\nContent-Type: message/http\r\nDate: ";
	message += dateTime;
	message += "\r\nTRACE request received:\r\n%s\r\n\r\n";
	

	// print some debug information
	cout << "HTTP Server: Handled TRACE request: \"" << sockets[index].buffer << "\"" << endl;
	strcpy(sendBuff, message.c_str());
}

void HandleDELETE(char* sendBuff, int index, SocketState* sockets) 
{
	string message;
	time_t now = time(0);
	char* dateTime = ctime(&now);
	char reqParams[BUFF_SIZE];
	strcpy(reqParams, &sockets[index].buffer[7]);
	string reqP(reqParams);
	string fileName = reqP.substr(0, reqP.find(".html"));
	fileName += ".html";
	string fileAddress = "C:\\htmlFiles\\" + fileName;

	int res = remove(fileAddress.c_str());
	if (res == 0) {
		message = sendBuff, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nDate:";
		message += dateTime;
		message += "\r\n<html><body><h1>File deleted successfully.</h1><p>Message: File deleted successfully.</p></body></html>\r\n\r\n";
	}
	else {
		message = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nDate:";
		message += dateTime;
		message += "\r\n<html><body><h1>File not found.</h1><p>Message: File not found.</p></body></html>\r\n\r\n";
	}
	strcpy(sendBuff, message.c_str());
}

void HandlePOST(char* sendBuff, int index, SocketState* sockets)
{
	time_t now = time(0);
	char* dateTime = ctime(&now);
	char* request = sockets[index].buffer;
	string content = GetQueryParam(request, "content");
	cout << "\nThe message to post is:\n" << content << endl;
	string message;
	// Send response with file contents
	message += "HTTP/1.1 200 OK\r\n";
	message += "Content-Type: text/html\r\nDate: ";
	message += dateTime;
	message += "Content-Length: 0";
	message += "\r\n\r\n";
	strcpy(sendBuff, message.c_str());
}

void HandlePUT(char* sendBuff, int index, SocketState* sockets)
{
	time_t now = time(0);
	char* dateTime = ctime(&now);
	char* request = sockets[index].buffer;
	string fileName = GetQueryParam(request, "page");
	string content = GetQueryParam(request, "content");
	string path = "C:\\htmlFiles\\" + fileName;
	string message;

	FILE* file = fopen(path.c_str() , "r+");
	if (file)
	{
		message += "HTTP/1.1 200 OK\r\n";
		message += "Content-Type: text/html\r\nDate: ";
		message += dateTime;
		message += "Content-Length: 0";
		message += "\r\n\r\n";
		std::fseek(file, 0, SEEK_END);
		std::fprintf(file, "%s", content.c_str()); // Write the string content to the file
		std::fclose(file); // Close the file
	}
	else
	{
		message += "HTTP/1.1 404 File not Found\r\n";
		message += "Content-Type: text/html\r\nDate: ";
		message += dateTime;
		message += "Content-Length: 0";
		message += "\r\n\r\n";
	}
	strcpy(sendBuff, message.c_str());
}

void HandleHEAD(char* sendBuff, int index, SocketState* sockets)
{
	time_t now = time(0);
	char* dateTime = ctime(&now);
	string message;
	string fileName = "NotFound";
	// Get the requested URL from the request string
	char* request = sockets[index].buffer;
	string queryParams = GetQueryParam(request, "lang");
	if (queryParams.size() == 0 || queryParams == "en")
		fileName = "index.html";
	else if (queryParams == "fr")
		fileName = "indexfr.html";
	else if (queryParams == "he")
		fileName = "indexhe.html";

	// Open the requested file
	char fileAddress[256];
	sprintf(fileAddress, "C:\\htmlFiles\\%s", fileName.c_str());
	FILE* file = fopen(fileAddress, "rb");

	if (file == NULL) {
		// File not found - send error response
		message += "HTTP/ 1.1 404 Not Found\r\n";
		message += "Content-Type: text/html\r\nDate: ";
		message += dateTime;
		message += "\r\n";
		message += "<html><body><h1>404 Not Found</h1></body></html>";
		message += "\r\n\r\n";
	}
	else {
		message += "HTTP/1.1 200 OK\r\n";
		message += "Content-Type: text/html\r\nDate: ";
		message += dateTime;
		message += "\r\n\r\n";
		fclose(file);
	}
	strcpy(sendBuff, message.c_str());
}

void HandleOPTIONS(char* sendBuff, int index, SocketState* sockets)
{
	string message;
	message += "HTTP/1.1 200 OK\nAllow: GET, POST, HEAD, OPTIONS, PUT, DELETE, TRACE\nContent - Length : 0";
	message += "\r\n\r\n";
	strcpy(sendBuff, message.c_str());
}
