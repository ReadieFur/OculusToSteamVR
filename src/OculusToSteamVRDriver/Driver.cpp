#include "Driver.h"

#ifdef _DEBUG
#include <Windows.h>
#endif

vr::EVRInitError OculusToSteamVR_Driver::Driver::Init(vr::IVRDriverContext* pDriverContext)
{
#if false
#ifdef _DEBUG
	while (!IsDebuggerPresent())
		Sleep(100);
	//DebugBreak();
#endif
#endif

	dataReceiver = new DataReceiver(1549); //O(15) D(4) S(19) - OculusDataStreamer.
	if (!dataReceiver->WasInitSuccess())
		return vr::VRInitError_IPC_Failed;

	return vr::VRInitError_None;
}

void OculusToSteamVR_Driver::Driver::Cleanup()
{
	delete dataReceiver;
	dataReceiver = nullptr;
}

void OculusToSteamVR_Driver::Driver::RunFrame()
{
	//No point wasting CPU time updating the devices if there isn't going to be any new data (we aren't connected).
	if (!dataReceiver->IsConnected())
		return;

	bool oculusDataTimedOut = false;
	//TODO: Sample the average frame timings and use that as the timeout in here.
	SOculusData oculusData = dataReceiver->TryGetData(1000 / 160, &oculusDataTimedOut);
	if (oculusDataTimedOut)
		return;
}
