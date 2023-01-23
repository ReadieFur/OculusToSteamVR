#include <winsock2.h>

#include "SOculusData.h"

#pragma comment(lib, "ws2_32.lib")

class DataSender
{
public:
	//TODO: Modify this to use a specified IP address.
	DataSender(int port);
	~DataSender();

	bool SendData(SOculusData* oculusData);

private:
	SOCKET sock;
	sockaddr_in broadcastAddr;
};
