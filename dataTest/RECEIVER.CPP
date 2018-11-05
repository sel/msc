// receiver.cpp

#include <winsock.h>
#include <stdio.h>
#include <stdlib.h>
#include "udpcs.h"

int main(int argc, char* argv[])
{
	WSADATA wsd;
	SOCKET s;
	SOCKADDR_IN sender, local;
	int iSenderSize;
	short iPort = SERVER_PORT;
	char recvbuf[BUFFER_LENGTH];
	int ret;
	int dgramCount = 0;

	// load winsock
	if (WSAStartup (MAKEWORD(2,2), &wsd) != 0)
	{
		printf("WSAStartup() failed!\n");
		return 1;
	}

	// create the socket
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET)
	{
		printf("socket() failed: %d\n", WSAGetLastError());
		return 1;
	}

	// name the local socket
	local.sin_family = AF_INET;
	local.sin_port = htons((short) iPort);
	local.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(s, (SOCKADDR *) &local, sizeof(local)) == SOCKET_ERROR)
	{
		printf("bind() failed: %d\n", WSAGetLastError());
		return 1;
	}

	printf("Waiting for datagrams on port %d\n\n", iPort);

	iSenderSize = sizeof(sender);

	while (1)
	{

		// receive datagrams
		ret = recvfrom(s, recvbuf, sizeof(recvbuf), 0, (SOCKADDR *)&sender, &iSenderSize);
		dgramCount++;
	
		if (ret == SOCKET_ERROR)
		{
			printf("recvfrom() failed: %d\n", WSAGetLastError());
			return 1;
		}
		else
			if (ret == 0)
				return 1;
			else
			{

				recvbuf[ret] = '\0';
				printf("received datagram(%3d): [%s]; length: [%d]; sending echo to client...", dgramCount, recvbuf, ret);

				// echo the datagram back to the client
				if (sendto(s, (const char *)&recvbuf, ret, 0, (SOCKADDR *)&sender, sizeof(sender)) == SOCKET_ERROR)
				{
					printf("sendto() failed: %d\n", WSAGetLastError());
					return 1;
				}
				else
					printf("ok\n");
			}
	}

	// cleanup
	closesocket(s);

	WSACleanup();
	return 0;
}
