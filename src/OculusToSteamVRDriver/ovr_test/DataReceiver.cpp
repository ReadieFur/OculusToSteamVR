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
	int recvTimeout = 1000 / UDP_SEND_RATE;
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

void OculusToSteamVR_Driver::DataReceiver::TryGetData(int timeoutMS, SOculusData* data, bool* timedOut)
{
	std::chrono::milliseconds timeout(timeoutMS);

	if (data == nullptr)
		throw std::invalid_argument("data cannot be null.");

	*timedOut = !mutex.try_lock_for(timeout);
	if (*timedOut)
		return;

	//Copy the data.
	data->sensorCount = oculusData.sensorCount;
	for (int i = 0; i < MAX_OVR_SENSORS; i++)
		data->sensorData[i] = oculusData.sensorData[i];
	data->trackingState = oculusData.trackingState;
	data->inputState = oculusData.inputState;
	data->objectCount = oculusData.objectCount;
	for (int i = 0; i < MAX_OVR_OBJECTS; i++)
		data->objectPoses[i] = oculusData.objectPoses[i];

	mutex.unlock();
}

void OculusToSteamVR_Driver::DataReceiver::UDPLoop()
{
	std::chrono::duration<double> UDP_INTERVAL = std::chrono::milliseconds(1000 / UDP_SEND_RATE);
	std::chrono::steady_clock::time_point lastIterationReceiveTime = std::chrono::high_resolution_clock::now();

	while (!dispose)
	{
		//Wait (if needed) before starting the next iteration.
		std::chrono::duration<double> iterationTime = std::chrono::high_resolution_clock::now() - lastIterationReceiveTime;
		if (iterationTime < UDP_INTERVAL)
			std::this_thread::sleep_for(UDP_INTERVAL - iterationTime);

		SOculusData data;
		int result = recvfrom(sock, (char*)&data, sizeof(data), 0, NULL, NULL);
		if (result == SOCKET_ERROR)
		{
			connected = false;
			//I want to update the lastIterationReceiveTime here so that
			//we don't get stuck in a 0 wait loop if the connection is closed.
			lastIterationReceiveTime = std::chrono::high_resolution_clock::now();
			continue;
		}
		//This should ideally be inside of the mutex lock too.
		connected = true;

		if (!mutex.try_lock_for(UDP_INTERVAL))
			continue;
		lastIterationReceiveTime = std::chrono::high_resolution_clock::now();
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

	try { closesocket(sock); }
	catch (...) {}
	try { WSACleanup(); }
	catch (...) {}
}