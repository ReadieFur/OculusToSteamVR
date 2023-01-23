#define _WINSOCKAPI_
#define NOMINMAX
#define OVR_D3D_VERSION 11

#include <OVR_CAPI_D3D.h> //Oculus SDK.
#include <chrono>
#include <functional>
#include "DataSender.h"

class OculusToSteamVRClient
{
public:
	static void Main(int argc, char** argsv);

private:
	static const std::chrono::duration<double> FRAME_DURATION;

	static bool initialized;
	static int udpSendRate;
	//Possibly change this so that the value is calculated at runtime.
	static float addedPredictionTimeMS;
	static DataSender* dataSender;

	OculusToSteamVRClient();

	static void NoGraphicsStart(std::function<bool(ovrSession, uint64_t)> mainLoopCallback);
	static bool MainLoopCallback(ovrSession mSession, uint64_t sessionStatus);
	static void LogPose(const char* prefix, ovrPosef pose);
};