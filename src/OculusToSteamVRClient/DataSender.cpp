#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "DataSender.h"

#include <iostream>

DataSender::DataSender(int port)
{
    //Initialize Winsock.
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed: %d\n", iResult);
        throw std::exception("WSAStartup failed.");
    }

    //Create socket.
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        printf("Socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        throw std::exception("socket failed.");
    }

    //Allow reuse of the socket.
    int optval = 1;
    iResult = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval));
    if (iResult == SOCKET_ERROR)
    {
        printf("Setsockopt failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        throw std::exception("setsockopt failed.");
    }

    //Fill in the address structure.
    broadcastAddr.sin_family = AF_INET;
    //broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastAddr.sin_addr.s_addr = inet_addr("192.168.1.163");
    broadcastAddr.sin_port = htons(port);
}

DataSender::~DataSender()
{
    closesocket(sock);
    WSACleanup();
}

bool DataSender::SendData(SOculusData* oculusData)
{
    int iResult = sendto(sock, (char*)oculusData, sizeof(SOculusData), 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr));
    if (iResult == SOCKET_ERROR)
    {
        printf("Sendto failed: %d\n", WSAGetLastError());
        /*closesocket(sock);
        WSACleanup();
        throw std::exception("sendto failed.");*/
        return FALSE;
    }
    return TRUE;
}
