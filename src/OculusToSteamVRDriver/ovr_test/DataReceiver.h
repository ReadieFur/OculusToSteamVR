#pragma once

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <mutex>
#include <thread>
#include "../../OculusToSteamVRShared/SOculusData.h"

#pragma comment(lib, "ws2_32.lib")

namespace OculusToSteamVR_Driver
{
	class DataReceiver
	{
	public:
		DataReceiver(int port);
		~DataReceiver() { Cleanup(); }

		bool WasInitSuccess() { return initSuccess; }
		bool IsConnected() { return connected; }
		void TryGetData(int timeoutMS, SOculusData* data, bool* timedOut);

	private:
		bool initSuccess = false;
		bool dispose = true;
		bool connected = false;
		SOCKET sock;
		std::timed_mutex mutex;
		SOculusData oculusData;
		std::thread* udpThread = nullptr;

		void UDPLoop();
		void Cleanup();
	};
}
