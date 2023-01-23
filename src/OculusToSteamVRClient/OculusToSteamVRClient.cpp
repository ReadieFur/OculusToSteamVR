//Modified from: https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/ovr_test/ovr_test.cpp

#include "OculusToSteamVRClient.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <string>

#include "GuardianSystemDemo.h"
#include "SOculusData.h"

const std::chrono::duration<double> OculusToSteamVRClient::FRAME_DURATION = std::chrono::duration<double>(1.0 / 90.0);

bool OculusToSteamVRClient::initialized = FALSE;
int OculusToSteamVRClient::udpSendRate = 90;
float OculusToSteamVRClient::addedPredictionTimeMS = 10.0f;
DataSender* OculusToSteamVRClient::dataSender = nullptr;

void OculusToSteamVRClient::Main(int argc, char** argsv)
{
    if (initialized)
		throw std::exception("OculusToSteamVRClient::Main called more than once.");
    initialized = TRUE;

    bool render = FALSE;
    int udpPort = 1549; //O(15) D(4) S(19) - OculusDataStreamer.
    if (argc == 5)
    {
        render = (std::string(argsv[1]) == "y");
		udpSendRate = std::stoi(argsv[2]);
		addedPredictionTimeMS = std::stof(argsv[3]);
		udpPort = std::stoi(argsv[4]);
    }

	dataSender = new DataSender(udpPort);

    //Currently broken, not a priority to fix as of now.
    if (render)
    {
        GuardianSystemDemo* instance = new(_aligned_malloc(sizeof(GuardianSystemDemo), 16)) GuardianSystemDemo();
        instance->Start(OculusToSteamVRClient::MainLoopCallback);
        delete instance;
    }
    else
    {
        //When not in the rendering mode, the program is able to get the OVR data while unfocused and can be used as a background app.
        NoGraphicsStart(OculusToSteamVRClient::MainLoopCallback);
    }
}

void OculusToSteamVRClient::NoGraphicsStart(std::function<bool(ovrSession, uint64_t)> mainLoopCallback)
{
    ovrSession mSession = nullptr;
    ovrGraphicsLuid luid{};
    ovrInitParams initParams =
    {
        ovrInit_RequestVersion | ovrInit_FocusAware | ovrInit_Invisible,
        OVR_MINOR_VERSION, NULL, 0, 0
    };

    if (!mSession)
    {
        if (OVR_FAILURE(ovr_Initialize(&initParams)))
            std::cout << "ovr_Initialize error" << std::endl;

        if (OVR_FAILURE(ovr_Create(&mSession, &luid)))
            std::cout << "ovr_Create error" << std::endl;

        if (OVR_FAILURE(ovr_SetTrackingOriginType(mSession, ovrTrackingOrigin_FloorLevel)))
            std::cout << "ovr_SetTrackingOriginType error" << std::endl;
    }

    // Main Loop
    uint64_t counter = 0;
    uint64_t frameCount = 0;

    //ovr_RecenterTrackingOrigin(hmd);
    while (TRUE)
    {
        //Get the current time.
        std::chrono::steady_clock::time_point frameStartTime = std::chrono::high_resolution_clock::now();

		//Get the Oculus session status.
        ovrSessionStatus sessionStatus;
        ovr_GetSessionStatus(mSession, &sessionStatus);
        if (sessionStatus.ShouldQuit)
            break;

        //Run the main loop body.
        if (!mainLoopCallback(mSession, frameCount))
            break;

		//Wait (if needed) before starting the next frame.
		std::chrono::duration<double> frameTime = std::chrono::high_resolution_clock::now() - frameStartTime;
		if (frameTime < FRAME_DURATION)
			std::this_thread::sleep_for(FRAME_DURATION - frameTime);

        //Increment the frame counter.
        frameCount++;
    }

    ovr_Destroy(mSession);
    ovr_Shutdown();
}

bool OculusToSteamVRClient::MainLoopCallback(ovrSession mSession, uint64_t frameCount)
{
	//Limit to running at the specified udpSendRate.
	if (frameCount % (90 / udpSendRate) != 0)
		return TRUE;

	SOculusData oculusData;

	//Add <addedPredictionTimeMS>*0.001 of prediction time in order to account for the time it takes to send the data over UDP.
    //This is to try and reduce any noticable latency.
    oculusData.trackingState = ovr_GetTrackingState(mSession, (ovr_GetTimeInSeconds() + (addedPredictionTimeMS * 0.001)), ovrTrue);

#pragma region Sensors
    unsigned int sensorCount = ovr_GetTrackerCount(mSession);
    if (sensorCount > MAX_OVR_SENSORS)
        sensorCount = MAX_OVR_SENSORS;
    oculusData.sensorCount = sensorCount;

    for (int i = 0; i < oculusData.sensorCount; i++)
        oculusData.sensorData[i] = ovr_GetTrackerPose(mSession, i);
#pragma endregion

#pragma region HMD
    //Sent along with "trackingState".
#pragma endregion

#pragma region Controllers
    //TODO: Get battery level.
    for (int i = 0; i < 2; i++)
    {
        ovrControllerType controllerType;
        ovrButton buttonMask;
        ovrTouch touchMask;
        if (i == 0)
        {
            controllerType = ovrControllerType::ovrControllerType_LTouch;
            buttonMask = ovrButton_LMask;
            touchMask = (ovrTouch)(ovrTouch_LButtonMask | ovrTouch_LPoseMask);
        }
        else
        {
            controllerType = ovrControllerType::ovrControllerType_RTouch;
            buttonMask = ovrButton_RMask;
            touchMask = (ovrTouch)(ovrTouch_RButtonMask | ovrTouch_RPoseMask);
        }
        ovr_GetInputState(mSession, controllerType, &oculusData.inputState);
        oculusData.inputState.Buttons &= ~buttonMask;
        oculusData.inputState.Buttons |= (buttonMask & oculusData.inputState.Buttons);
        oculusData.inputState.Touches &= ~touchMask;
        oculusData.inputState.Touches |= (touchMask & oculusData.inputState.Touches);

        oculusData.inputState.HandTrigger[i] = oculusData.inputState.HandTrigger[i];
        oculusData.inputState.IndexTrigger[i] = oculusData.inputState.IndexTrigger[i];
        oculusData.inputState.Thumbstick[i] = oculusData.inputState.Thumbstick[i];
    }
#pragma endregion

#pragma region Objects
    //Temporarily removed while I work on sending dynamically sized data over UDP.
    unsigned int objectCount = ((ovr_GetConnectedControllerTypes(mSession) >> 8) & 0xf);
    if (objectCount > MAX_OVR_OBJECTS)
        objectCount = MAX_OVR_OBJECTS;
    oculusData.objectCount = objectCount;

    for (int i = 0; i < oculusData.objectCount; i++)
    {
        ovrTrackedDeviceType deviceType = (ovrTrackedDeviceType)(ovrTrackedDevice_Object0 + i);
        ovr_GetDevicePoses(mSession, &deviceType, 1, (ovr_GetTimeInSeconds() + (16.0f * 0.001)), &oculusData.objectPoses[i]);
    }
#pragma endregion

#pragma region Send UDP data
    dataSender->SendData(&oculusData);
#pragma endregion

#pragma region Logging
    //This won't always log on the given interval due to my return statment above
    //that only updates on the specified udpSendRate value.
    //Log every 5 seconds (90fps * 5 = 450).
    if (frameCount % 450 == 0)
    {
        //Sensors.
        for (int i = 0; i < oculusData.sensorCount; i++)
        {
            LogPose(("Sensor" + std::to_string(i)).c_str(), oculusData.sensorData[i].Pose);

            ovrTrackerFlags trackerFlags = (ovrTrackerFlags)oculusData.sensorData[i].TrackerFlags;

            std::cout << std::boolalpha
                << ", Connected:" << ((trackerFlags & ovrTrackerFlags::ovrTracker_Connected) == ovrTrackerFlags::ovrTracker_Connected)
                << ", Tracked:" << ((trackerFlags & ovrTrackerFlags::ovrTracker_PoseTracked) == ovrTrackerFlags::ovrTracker_PoseTracked);

            std::cout << std::endl;
        }

        //HMD.
        LogPose("HMD", oculusData.trackingState.HeadPose.ThePose);
        
        std::cout << std::boolalpha
            << ", PTracked:" << ((oculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_PositionTracked) == ovrStatusBits::ovrStatus_PositionTracked)
            << ", PValid:" << ((oculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_PositionValid) == ovrStatusBits::ovrStatus_PositionValid)
            << ", OTracked:" << ((oculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_OrientationTracked) == ovrStatusBits::ovrStatus_OrientationTracked)
            << ", OValid:" << ((oculusData.trackingState.StatusFlags & ovrStatusBits::ovrStatus_OrientationValid) == ovrStatusBits::ovrStatus_OrientationValid);

        std::cout << std::endl;

        //Controllers.
        for (int i = 0; i < 2; i++)
        {
            LogPose((i == 0 ? "LHand" : "RHand"), oculusData.trackingState.HandPoses[i].ThePose);

            std::cout << std::hex
                << ", buttons:0x" << oculusData.inputState.Buttons
                << ", touches:0x" << oculusData.inputState.Touches
                << std::dec
                << ", handTrigger:" << oculusData.inputState.HandTrigger[i]
                << ", indexTrigger:" << oculusData.inputState.IndexTrigger[i]
                << ", thumbstick:x" << oculusData.inputState.Thumbstick[i].x << ",y" << oculusData.inputState.Thumbstick[i].y;

            std::cout << std::boolalpha
                << ", PTracked:" << ((oculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_PositionTracked) == ovrStatusBits::ovrStatus_PositionTracked)
                << ", PValid:" << ((oculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_PositionValid) == ovrStatusBits::ovrStatus_PositionValid)
                << ", OTracked:" << ((oculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_OrientationTracked) == ovrStatusBits::ovrStatus_OrientationTracked)
                << ", OValid:" << ((oculusData.trackingState.HandStatusFlags[i] & ovrStatusBits::ovrStatus_OrientationValid) == ovrStatusBits::ovrStatus_OrientationValid);

            std::cout << std::endl;
        }

        //Objects.
        for (int i = 0; i < oculusData.objectCount; i++)
        {
            LogPose(("Object" + std::to_string(i)).c_str(), oculusData.objectPoses[i].ThePose);
            std::cout << std::endl;
        }

        std::cout << std::endl;
    }
#pragma endregion

    return TRUE;
}

void OculusToSteamVRClient::LogPose(const char* prefix, ovrPosef pose)
{
    std::cout << std::dec << std::fixed << std::setprecision(2)
        << prefix
        << ", px:" << pose.Position.x
        << ", py:" << pose.Position.y
        << ", pz:" << pose.Position.z
        << ", rw:" << pose.Orientation.w
        << ", rx:" << pose.Orientation.x
        << ", ry:" << pose.Orientation.y
        << ", rz:" << pose.Orientation.z;
}
