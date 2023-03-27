//Modified from: https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/ovr_test/ovr_test.cpp

#include "OculusToSteamVRClient.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <string>

#include "GuardianSystemDemo.h"
#include "SOculusData.h"

#define FRAME_RATE 90

const std::chrono::duration<double> OculusToSteamVRClient::FRAME_INTERVAL = std::chrono::milliseconds(1000 / FRAME_RATE);
const std::chrono::duration<double> OculusToSteamVRClient::UDP_INTERVAL = std::chrono::milliseconds(1000 / UDP_SEND_RATE);
const std::chrono::duration<double> OculusToSteamVRClient::LOG_INTERVAL = std::chrono::seconds(5);

bool OculusToSteamVRClient::initialized = FALSE;
float OculusToSteamVRClient::addedPredictionTimeMS = 10.0f;
//I was having problems trying to use <std::chrono::steady_clock::time_point::min()> and I wasn't bothered to figure out why.
std::chrono::steady_clock::time_point OculusToSteamVRClient::lastSendTime = std::chrono::high_resolution_clock::now();
std::chrono::steady_clock::time_point OculusToSteamVRClient::lastLogTime = std::chrono::high_resolution_clock::now();
DataSender* OculusToSteamVRClient::dataSender = nullptr;

//TODO: Possibly restart the client every 5 minutes to refresh oculus, as if the HMD isn't moved for 5 minutes, oculus will stop tracking.
//However I think when you start up an app (or OVR session, this timeout is reset, so perhaps if I can restart the OVR session every 5 minutes I can work around that timeout).
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
		addedPredictionTimeMS = std::stof(argsv[2]);
		udpPort = std::stoi(argsv[3]);
    }

	dataSender = new DataSender(udpPort);

    //Currently broken, not a priority to fix as of now. The only reason I could think that you would want to do this is to possibly reduce system load by rendering "nothing"?
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
		if (frameTime < FRAME_INTERVAL)
			std::this_thread::sleep_for(FRAME_INTERVAL - frameTime);

        //Increment the frame counter.
        frameCount++;
    }

    ovr_Destroy(mSession);
    ovr_Shutdown();
}

bool OculusToSteamVRClient::MainLoopCallback(ovrSession mSession, uint64_t frameCount)
{
	//Limit to running at the specified UDP_SEND_RATE.
    /*if (frameCount % (1000 / UDP_SEND_RATE) != 0)
        return TRUE;*/
    //Switched to using time as it is more reliable.
    if ((std::chrono::high_resolution_clock::now() - lastSendTime) < UDP_INTERVAL)
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
    lastSendTime = std::chrono::high_resolution_clock::now();
#pragma endregion

#pragma region Logging
    if (std::chrono::high_resolution_clock::now() - lastLogTime >= LOG_INTERVAL)
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

        lastLogTime = std::chrono::high_resolution_clock::now();
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
