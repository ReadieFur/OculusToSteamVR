#include "Driver.h"

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
	for (int i = 0; i < 2; i++)
	{
		std::shared_ptr<VRTracker> controllerTrackerDevice = std::make_shared<VRTracker>(i + 1);
		controllerTrackerDevice->UseOVRControllerData(true);
		AddDevice(controllerTrackerDevice);
	}

	return vr::VRInitError_None;
}

void OculusToSteamVR_Driver::Driver::Cleanup()
{
	delete dataReceiver;
	dataReceiver = nullptr;
}

void OculusToSteamVR_Driver::Driver::RunFrame()
{
	//Collect events
	vr::VREvent_t event;
	std::vector<vr::VREvent_t> events;
	while (vr::VRServerDriverHost()->PollNextEvent(&event, sizeof(event)));
	
	//No point wasting CPU time updating the devices if there isn't going to be any new data (we aren't connected).
	if (!dataReceiver->IsConnected())
		return;

	bool oculusDataTimedOut = false;
	//TODO: Sample the average frame timings and use that as the timeout in here.
	SOculusData oculusData = dataReceiver->TryGetData(1000 / 160, &oculusDataTimedOut);
	if (oculusDataTimedOut)
		return;

	//Check if any new devices were added (devices cannot be removed).
	RefreshDevices<VRSensor>(lastOculusData.sensorCount, oculusData.sensorCount, SENSORS_OFFSET);
	RefreshDevices<VRTracker>(lastOculusData.objectCount, oculusData.objectCount, OBJECTS_OFFSET);

	//Update the devices.
	for (std::shared_ptr<IVRDevice> device : devices)
		device->RunFrame(&oculusData);

	lastOculusData = oculusData;

#ifdef _DEBUG
	frameCount++;
	DebugLog();
#endif
}

bool OculusToSteamVR_Driver::Driver::AddDevice(std::shared_ptr<IVRDevice> device)
{
    bool result = vr::VRServerDriverHost()->TrackedDeviceAdded(device->GetSerial().c_str(), device->GetDeviceType(), device.get());
    if (result)
        this->devices.push_back(device);
	    return result;
}

template<typename T>
typename std::enable_if<std::is_base_of<OculusToSteamVR_Driver::IVRDevice, T>::value>::type
OculusToSteamVR_Driver::Driver::RefreshDevices(unsigned int lastDeviceCount, unsigned int deviceCount, unsigned int indexOffset)
{
	if (lastDeviceCount == deviceCount)
		return;

	//Add any new devices we may have.
	for (uint8_t i = lastDeviceCount; i < deviceCount; i++)
		AddDevice(std::make_shared<T>(indexOffset + i));

	//For any extra devices we have previosuly registered, mark them as offline.
	for (uint8_t i = deviceCount; i < lastDeviceCount; i++)
	{
		std::shared_ptr<IVRDevice> device = devices[indexOffset + i];
		vr::DriverPose_t pose = device->GetPose();
		pose.poseIsValid = false;
		pose.result = vr::TrackingResult_Uninitialized;
		pose.deviceIsConnected = false;
		device->SetPose(pose);
		device->ApplyPose();
		device->Disable();
	}
}

#ifdef _DEBUG
#include <string>

void OculusToSteamVR_Driver::Driver::DebugLog()
{
	//Log every 5 seconds (at 90fps).
	if (frameCount % 450 == 0)
	{
		std::string message = "OVR UDP data:\n";
		
		//Sensors.
		for (int i = 0; i < lastOculusData.sensorCount; i++)
		{
			message += LogPose(("Sensor" + std::to_string(i)).c_str(), lastOculusData.sensorData[i].Pose);

			ovrTrackerFlags trackerFlags = (ovrTrackerFlags)lastOculusData.sensorData[i].TrackerFlags;

			message += ", Connected:" + std::to_string(((trackerFlags & ovrTrackerFlags::ovrTracker_Connected) == ovrTrackerFlags::ovrTracker_Connected))
				+ ", Tracked:" + std::to_string(((trackerFlags & ovrTrackerFlags::ovrTracker_PoseTracked) == ovrTrackerFlags::ovrTracker_PoseTracked))
				+ "\n";
		}

		//HMD.
		/*LogPose("HMD", lastOculusData.trackingState.HeadPose.ThePose);

		std::cout << std::boolalpha
			<< ", PTracked:" << ((lastOculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_PositionTracked) == ovrStatusBits::ovrStatus_PositionTracked)
			<< ", PValid:" << ((lastOculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_PositionValid) == ovrStatusBits::ovrStatus_PositionValid)
			<< ", OTracked:" << ((lastOculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_OrientationTracked) == ovrStatusBits::ovrStatus_OrientationTracked)
			<< ", OValid:" << ((lastOculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_OrientationValid) == ovrStatusBits::ovrStatus_OrientationValid);
		
		std::cout << std::endl;*/

		//Controllers.
		for (int i = 0; i < 2; i++)
		{
			message += LogPose((i == 0 ? "LHand" : "RHand"), lastOculusData.trackingState.HandPoses[i].ThePose);

			message += ", buttons:0x" + std::to_string(lastOculusData.inputState.Buttons)
				+ ", touches:0x" + std::to_string(lastOculusData.inputState.Touches)
				+ ", handTrigger:" + std::to_string(lastOculusData.inputState.HandTrigger[i])
				+ ", indexTrigger:" + std::to_string(lastOculusData.inputState.IndexTrigger[i])
				+ ", thumbstick:x" + std::to_string(lastOculusData.inputState.Thumbstick[i].x) + ",y" + std::to_string(lastOculusData.inputState.Thumbstick[i].y);

			message += ", PTracked:" + std::to_string(((lastOculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_PositionTracked) == ovrStatusBits::ovrStatus_PositionTracked))
				+ ", PValid:" + std::to_string(((lastOculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_PositionValid) == ovrStatusBits::ovrStatus_PositionValid))
				+ ", OTracked:" + std::to_string(((lastOculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_OrientationTracked) == ovrStatusBits::ovrStatus_OrientationTracked))
				+ ", OValid:" + std::to_string(((lastOculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_OrientationValid) == ovrStatusBits::ovrStatus_OrientationValid));

			message += "\n";
		}

		//Objects.
		for (int i = 0; i < lastOculusData.objectCount; i++)
		{
			message += LogPose(("Object" + std::to_string(i)).c_str(), lastOculusData.objectPoses[i].ThePose)
				+ "\n";
		}

		message += "\n";
		
		vr::VRDriverLog()->Log(message.c_str());
	}
}

std::string OculusToSteamVR_Driver::Driver::LogPose(const char* prefix, ovrPosef pose)
{
	return std::string(prefix)
		+ ", px:" + std::to_string(pose.Position.x)
		+ ", py:" + std::to_string(pose.Position.y)
		+ ", pz:" + std::to_string(pose.Position.z)
		+ ", rw:" + std::to_string(pose.Orientation.w)
		+ ", rx:" + std::to_string(pose.Orientation.x)
		+ ", ry:" + std::to_string(pose.Orientation.y)
		+ ", rz:" + std::to_string(pose.Orientation.z);
}
#endif
