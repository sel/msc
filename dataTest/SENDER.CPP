// sender.cpp

#include <winsock.h>
#include <stdio.h>
#include <stdlib.h>
#include "udpcs.h"

int main(int argc, char* argv[])
{
	WSADATA wsd;
	SOCKET s;
	SOCKADDR_IN recipient, local;
	short iRecipientPort = SERVER_PORT;
	short iLocalPort = CLIENT_PORT;
	int ret;
	int iRecipientLen;
	int iLocalLen;
	char sendbuf[BUFFER_LENGTH];
	char recvbuf[BUFFER_LENGTH];
	DWORD dwLength = DATA_LENGTH;
	int loopCount = REPITITIONS;

	loopCount = atoi(&argv[2][0]);

	if (WSAStartup (MAKEWORD(2,2), &wsd) != 0)
	{
		printf("WSAStartup() failed!\n");
		return 1;
	}

	// Create the socket
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET)
	{
		printf("socket() failed: %d\n", WSAGetLastError());
		return 1;
	}


	// name the recipient socket
	recipient.sin_family = AF_INET;
	recipient.sin_port = htons((short) iRecipientPort);
	recipient.sin_addr.s_addr = inet_addr(argv[1]);
	iRecipientLen = sizeof(recipient);

	// name the local socket
	local.sin_family = AF_INET;
	local.sin_port = htons((short) iLocalPort);
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	iLocalLen = sizeof(local);

	if(bind(s, (SOCKADDR *) &local, sizeof(local)) == SOCKET_ERROR)
	{
		printf("bind() failed: %d\n", WSAGetLastError());
		return 1;
	}

	// set the data buffer
	memset (&sendbuf, '*', dwLength);


	for (int i=1; i<=loopCount; i++)
	{
		// send the data

		ret = sendto(s, sendbuf, dwLength, 0, (SOCKADDR *)&recipient, sizeof(recipient));
		sendbuf[dwLength] = '\0';
		printf("sending  datagram(%3d): [%s]; length: [%d]\n", i, sendbuf, ret);

		if (ret == SOCKET_ERROR)
		{
			printf("sendto() failed: %d\n", WSAGetLastError());
			return 1;
		}
		else
		{
			if (ret == 0)
				return 1;
		}

		// receive the server's reply
		ZeroMemory(&recvbuf, sizeof(recvbuf));
		if (recvfrom(s, (char *)&recvbuf, sizeof(recvbuf), 0, (SOCKADDR *)&local, &iLocalLen) == SOCKET_ERROR)
		{
			printf("recvfrom() failed: %d\n", WSAGetLastError());
			return 1;
		}
		else
		{
			recvbuf[ret] = '\0';
			printf("received datagram(%3d): [%s]; length: [%d]\n", i, recvbuf, ret);
		}

	}

	// cleanup
	closesocket(s);
	WSACleanup();
	return 0;
}
