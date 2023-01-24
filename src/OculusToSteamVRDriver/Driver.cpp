#include "Driver.h"

#include "VRTracker.h"

#if false
#ifdef _DEBUG
#include <Windows.h>
#endif
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

	vr::EVRInitError init_error = vr::InitServerDriverContext(pDriverContext);
	if (init_error != vr::EVRInitError::VRInitError_None)
		return init_error;

	dataReceiver = new DataReceiver(1549); //O(15) D(4) S(19) - OculusDataStreamer.
	if (!dataReceiver->WasInitSuccess())
		return vr::VRInitError_IPC_Failed;

	//The following devices will always (or should always) be present.
	//AddDevice(std::make_shared<VRTracker>(0)); //HMD (0).
	AddDevice(std::make_shared<VRTracker>(1)); //Left hand (1).
	AddDevice(std::make_shared<VRTracker>(2)); //Right hand (2).

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

bool OculusToSteamVR_Driver::Driver::AddDevice(std::shared_ptr<IVRDevice> device)
{
    bool result = vr::VRServerDriverHost()->TrackedDeviceAdded(device->GetSerial().c_str(), device->GetDeviceType(), device.get());
    if (result)
        this->devices.push_back(device);
    return result;
}
