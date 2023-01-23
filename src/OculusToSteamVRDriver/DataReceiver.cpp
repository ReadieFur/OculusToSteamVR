#include "DataReceiver.h"

OculusToSteamVR_Driver::DataReceiver::DataReceiver(int port)
{
	//Initialize Winsock.
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		Cleanup();
		return;
	}

	//Create a socket.
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET)
	{
		Cleanup();
		return;
	}

	//Create address structure.
	SOCKADDR_IN recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//recvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	recvAddr.sin_port = htons(port);

	//Bind the socket.
	result = bind(sock, (SOCKADDR*)&recvAddr, sizeof(recvAddr));
	if (result == SOCKET_ERROR)
	{
		Cleanup();
		return;
	}

	//Set a timeout for recvfrom.
	int recvTimeout = 1000 / interval;
	result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));
	if (result == SOCKET_ERROR)
	{
		Cleanup();
		return;
	}

	//Update the status members.
	initSuccess = true;
	dispose = false;

	//Start the UDP loop.
	udpThread = new std::thread(&DataReceiver::UDPLoop, this);
}

SOculusData OculusToSteamVR_Driver::DataReceiver::TryGetData(int timeoutMS, bool* timedOut)
{
	std::chrono::milliseconds timeout(timeoutMS);

	if (!mutex.try_lock_for(timeout))
	{
		*timedOut = true;
		return SOculusData();
	}
	
	*timedOut = false;
	SOculusData data = oculusData;

	mutex.unlock();

	return data;
}

void OculusToSteamVR_Driver::DataReceiver::UDPLoop()
{
	std::chrono::steady_clock::time_point lastIterationStartTime = std::chrono::steady_clock::time_point::min();

	while (!dispose)
	{
		//Wait (if needed) before starting the next iteration.
		std::chrono::duration<double> iterationTime = std::chrono::high_resolution_clock::now() - lastIterationStartTime;
		if (iterationTime.count() < 1.0 / interval)
			std::this_thread::sleep_for(std::chrono::milliseconds((int)(1000.0 / interval - iterationTime.count() * 1000.0)));
		lastIterationStartTime = std::chrono::high_resolution_clock::now();

		SOculusData data;
		int result = recvfrom(sock, (char*)&data, sizeof(data), 0, NULL, NULL);
		if (result == SOCKET_ERROR)
		{
			connected = false;
			continue;
		}
		//This should ideally be inside of the mutex lock too.
		connected = true;

		if (!mutex.try_lock_for(std::chrono::milliseconds(1000 / interval)))
			continue;
		oculusData = data;
		mutex.unlock();
	}
}

void OculusToSteamVR_Driver::DataReceiver::Cleanup()
{
	if (udpThread != nullptr)
	{
		dispose = true;
		udpThread->join();
		delete udpThread;
		udpThread = nullptr;
	}

	try { closesocket(sock); } catch (...){}
	try { WSACleanup(); } catch (...){}
}
