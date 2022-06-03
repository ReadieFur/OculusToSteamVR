#include <iostream>
#include <Windows.h>
#include <mutex>
#include <SharedData.hpp>
#include <thread>
#include <chrono>
#include <OVR_CAPI.h>
#include <vector>
#include <iomanip>

/*Exit codes:
* 0 Ok.
* 1 Mutex instance already exists.
* 2 Failed to setup shared data.
* 3 Initialization error.
*/

#define RUN_INTERVAL 90 //Set to 90 becuase that is the max update rate of the Oculus sensors.

inline bool ShouldLog(int frameCount)
{
#if TRUE
	return frameCount % (RUN_INTERVAL * 5) == 0;  //Slower but I'm using this because I don't understand the & (Log every Xs where x is the multiplicate).
#else
	return frameCount & 0x7FF;
#endif
}

void VRLoop(ovrSession oSession, HANDLE sharedMutex, SharedData* sharedBuffer, uint64_t frameCount,
	ovrHapticsBuffer& oHapticsBuffer, uint8_t* hapticsBuffer, unsigned int hapticsBufferSize)
{
	WaitForSingleObject(sharedMutex, INFINITE);

	//Used to indicate to other processes that the client is alive. We do this last after initial setup.
	sharedBuffer->clientTime = std::chrono::high_resolution_clock::now();

	ovrTrackingState oTrackingState = ovr_GetTrackingState(oSession, 0, false);
	sharedBuffer->oTrackingState = oTrackingState;

	bool shouldLog = ShouldLog(frameCount);
	double frameTime = ovr_GetPredictedDisplayTime(oSession, frameCount);

	if (shouldLog)
	{
		//https://stackoverflow.com/questions/62226043/how-to-get-the-current-time-in-c-in-a-safe-way
		auto start = std::chrono::system_clock::now();
		auto legacyStart = std::chrono::system_clock::to_time_t(start);
		char tmBuff[30];
		ctime_s(tmBuff, sizeof(tmBuff), &legacyStart);
		std::cout << tmBuff;
	}

	oTrackingState = ovr_GetTrackingState(oSession, frameTime, ovrTrue);
	sharedBuffer->oTrackingState = oTrackingState;

	//HMD.
	if (shouldLog)
	{
		std::cout << "HMD:"
			<< "\n\tStatus:0x" << std::hex << oTrackingState.StatusFlags << std::dec
			<< "\n\tPX:" << oTrackingState.HeadPose.ThePose.Position.x
			<< " PY:" << oTrackingState.HeadPose.ThePose.Position.y
			<< " PZ:" << oTrackingState.HeadPose.ThePose.Position.z
			<< "\n\tRW:" << oTrackingState.HeadPose.ThePose.Orientation.w
			<< " RX:" << oTrackingState.HeadPose.ThePose.Orientation.x
			<< " RY:" << oTrackingState.HeadPose.ThePose.Orientation.y
			<< " RZ:" << oTrackingState.HeadPose.ThePose.Orientation.z
			<< std::endl;
	}

	//Controllers.
	for (int i = 0; i < 2; i++)
	{
		ovrInputState oInputState;
		ovrResult oResult = ovr_GetInputState(oSession, i == ovrHand_Left ?
			ovrControllerType::ovrControllerType_LTouch : ovrControllerType::ovrControllerType_RTouch, &oInputState);

		//Update input state values.
		sharedBuffer->oInputState[i] = oInputState;

		//Vibration, https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/ovr_test/ovr_test.cpp#L111-L152
		if (sharedBuffer->cHapticEventInfo[i].shouldVibrate)
		{
			sharedBuffer->cHapticEventInfo[i].shouldVibrate = false;

			int duration = sharedBuffer->cHapticEventInfo[i].duration * 320;
			if (duration > 128) duration = 128;
			if (duration < 2) duration = 2;

			oHapticsBuffer.SamplesCount = duration;
			uint32_t ratio = 1;
			if (sharedBuffer->cHapticEventInfo[i].frequency > 80/*300*/) ratio = 4;
			else if (sharedBuffer->cHapticEventInfo[i].frequency > 66/*230*/) ratio = 3;
			else if (sharedBuffer->cHapticEventInfo[i].frequency > 40/*160*/) ratio = 2;
			else ratio = 1;
			for (int j = 0; j < duration; j++)
			{
				//No clue what this is doing.
				if ((j & 3) < ratio)
				{
					float v = (0.1f + (sharedBuffer->cHapticEventInfo[i].amplitude * 0.9f)) * 255.0f;
					uint8_t vb = v;
					if (v > 255.0f) vb = 255;
					else if (v < 0.1f) vb = 256 / 10;

					hapticsBuffer[j] = vb; //(comm_buffer->vib_amplitude[i] < 0.5000001) ? 127 : 255;
				}
				else hapticsBuffer[j] = 0;
			}
			ovr_SubmitControllerVibration(oSession, i == 0 ? ovrControllerType_LTouch : ovrControllerType_RTouch, &oHapticsBuffer);
			for (int j = 0; j < hapticsBufferSize; j++) hapticsBuffer[j] = 255;
		}

		if (shouldLog)
		{
			std::cout << (i == 0 ? "Left Hand" : "Right Hand") << ":"
				<< "\n\tStatus:0x" << std::hex << oTrackingState.HandStatusFlags[i] << std::dec
				<< "\n\tPX:" << oTrackingState.HandPoses[i].ThePose.Position.x
				<< " PY:" << oTrackingState.HandPoses[i].ThePose.Position.y
				<< " PZ:" << oTrackingState.HandPoses[i].ThePose.Position.z
				<< "\n\tRW:" << oTrackingState.HandPoses[i].ThePose.Orientation.w
				<< " RX:" << oTrackingState.HandPoses[i].ThePose.Orientation.x
				<< " RY:" << oTrackingState.HandPoses[i].ThePose.Orientation.y
				<< " RZ:" << oTrackingState.HandPoses[i].ThePose.Orientation.z
				<< std::hex << "\n\tButton:0x" << oInputState.Buttons << std::dec
				<< " Touch:0x" << oInputState.Touches
				<< " Grip:" << oInputState.HandTrigger[i]
				<< " Trigger:" << oInputState.IndexTrigger[i]
				<< "\n\tAmplitude:" << sharedBuffer->cHapticEventInfo[i].amplitude
				<< " Frequency:" << sharedBuffer->cHapticEventInfo[i].frequency
				<< std::endl;
		}
	}

	//Objects.
	//I believe this represents the VRObjects feature that Oculus has, however when I mapped my left controller as an object it never appeared here.
	for (int i = 0; i < sharedBuffer->vrObjectsCount; i++)
	{
		ovrTrackedDeviceType deviceType = (ovrTrackedDeviceType)(ovrTrackedDevice_Object0 + i);
		ovrPoseStatef oPose;
		ovr_GetDevicePoses(oSession, &deviceType, 1, frameTime, &oPose);
		sharedBuffer->vrObjects[i] = oPose;

		//Bit-shift? to display every ? frames. (I don't understand the bit shift so I don't know how often this runs).
		if (shouldLog)
		{
			std::cout << "Object" << i << ":"
				<< "\n\tPX:" << oPose.ThePose.Position.x
				<< " PY:" << oPose.ThePose.Position.y
				<< " PZ:" << oPose.ThePose.Position.z
				<< "\n\tRW:" << oPose.ThePose.Orientation.w
				<< " RX:" << oPose.ThePose.Orientation.x
				<< " RY:" << oPose.ThePose.Orientation.y
				<< " RZ:" << oPose.ThePose.Orientation.z
				<< std::endl;
		}
	}

	//Sensors.
	for (int i = 0; i < sharedBuffer->trackingRefrencesCount; i++)
	{
		sharedBuffer->trackingRefrences[i] = ovr_GetTrackerPose(oSession, i);

		if (shouldLog)
		{
			std::cout << "Sensor" << i << ":"
				<< "\n\tStatus:0x" << std::hex << sharedBuffer->trackingRefrences[i].TrackerFlags << std::dec
				<< "\n\tPX:" << sharedBuffer->trackingRefrences[i].LeveledPose.Position.x
				<< " PY:" << sharedBuffer->trackingRefrences[i].LeveledPose.Position.y
				<< " PZ:" << sharedBuffer->trackingRefrences[i].LeveledPose.Position.z
				<< "\n\tRW:" << sharedBuffer->trackingRefrences[i].LeveledPose.Orientation.w
				<< " RX:" << sharedBuffer->trackingRefrences[i].LeveledPose.Orientation.x
				<< " RY:" << sharedBuffer->trackingRefrences[i].LeveledPose.Orientation.y
				<< " RZ:" << sharedBuffer->trackingRefrences[i].LeveledPose.Orientation.z
				<< std::endl;
		}
	}

	if (shouldLog) std::cout << std::endl;

	ReleaseMutex(sharedMutex);
}

int InitSharedData(HANDLE& hMapFile, SharedData*& sharedBuffer, HANDLE& sharedMutex)
{
	bool didCreateMapping = false;

	//Try to connect to an existing object.
	hMapFile = OpenFileMappingW(
		FILE_MAP_ALL_ACCESS, //Read/write access.
		FALSE, //Do not inherit the name.
		L"Local\\ovr_client_shared_data"); //Name of mapping object.
	if (hMapFile == NULL)
	{
		//If it failed/no object was available, try to create a new one.
		hMapFile = CreateFileMappingW(
			INVALID_HANDLE_VALUE, //Use paging file.
			NULL, //Default security.
			PAGE_READWRITE, //Read/write access.
			0, //Maximum object size (high-order DWORD).
			sizeof(SharedData), //Maximum object size (low-order DWORD).
			L"Local\\ovr_client_shared_data" //Name of mapping object.
		);
		if (hMapFile == NULL)
		{
			std::cout << "Could not open file mapping object " << GetLastError() << std::endl;
			return 2;
		}
	}
	else didCreateMapping = false;

	if (!didCreateMapping)
	{
		sharedBuffer = (SharedData*)MapViewOfFile(hMapFile, //Handle to map object.
			FILE_MAP_ALL_ACCESS, //Read/write permission.
			0,
			0,
			sizeof(SharedData));
	}
	else
	{
		sharedBuffer = new(MapViewOfFile(
			hMapFile, //Handle to map object.
			FILE_MAP_ALL_ACCESS, //Read/write permission.
			0,
			0,
			sizeof(SharedData)))SharedData();
	}

	if (sharedBuffer == NULL)
	{
		std::cout << "Could not map view of file " << GetLastError() << std::endl;
		CloseHandle(hMapFile);
		return 2;
	}

	//If we did create the mapping then we should be able create and be the initial owner of the mutex, if we didn't create the mapping then we shouldn't be the owner of the mutex.
	sharedMutex = CreateMutexW(0, didCreateMapping, L"Local\\ovr_client_shared_mutex");
	WaitForSingleObject(
		sharedMutex, //Handle to mutex.
		INFINITE); //No time-out interval.
	ReleaseMutex(sharedMutex);

	return 0;
}

int main()
{
	//Limit to one instance.
	CreateMutexA(0, FALSE, "Local\\ovr_client_instance_mutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		std::cout << "An instance of OVRClient is already running." << std::endl;
		return 1;
	}

	/*std::cout config
	* Limit to 2dp, https://stackoverflow.com/questions/5907031/printing-the-correct-number-of-decimal-points-with-cout
	*/
	std::cout << std::fixed << std::setprecision(2);

	HANDLE hMapFile;
	SharedData* sharedBuffer;
	HANDLE sharedMutex;
	int initSharedBufferResult = InitSharedData(hMapFile, sharedBuffer, sharedMutex);
	if (initSharedBufferResult != 0) return initSharedBufferResult;

	WaitForSingleObject(sharedMutex, INFINITE);

	ovrResult oResult;
	ovrErrorInfo oErrorInfo;
	ovrSession oSession = nullptr;
	ovrGraphicsLuid oGraphicsLuid{};
	ovrInitParams oInitParams = { ovrInit_RequestVersion | ovrInit_FocusAware | ovrInit_Invisible, OVR_MINOR_VERSION, NULL, 0, 0 };

	if (OVR_FAILURE(ovr_Initialize(&oInitParams)
		|| OVR_FAILURE(ovr_Create(&oSession, &oGraphicsLuid))
		|| OVR_FAILURE(ovr_SetTrackingOriginType(oSession, ovrTrackingOrigin_FloorLevel))
	))
	{
		ovr_GetLastErrorInfo(&oErrorInfo);
		std::cout << "Failed to initialize OVR. Code " << oErrorInfo.Result << ". " << oErrorInfo.ErrorString << std::endl;
		return 3;
	}

	//Main loop.
	uint64_t counter = 0;
	uint64_t frameCount = 0;

	uint8_t hapticsBuffer[128] = { 0 };
	unsigned int hapticsBufferSize = sizeof(hapticsBuffer);
	ovrHapticsBuffer oHapticsBuffer;
	oHapticsBuffer.Samples = hapticsBuffer;
	oHapticsBuffer.SamplesCount = hapticsBufferSize;
	oHapticsBuffer.SubmitMode = ovrHapticsBufferSubmit_Enqueue;
	for (int i = 0; i < hapticsBufferSize; i++) hapticsBuffer[i] = 255;

	//TODO: Make this dynamic as I believe the count here can change at any time.
	sharedBuffer->vrObjectsCount = (ovr_GetConnectedControllerTypes(oSession) >> 8) & 0xf;
	if (sharedBuffer->vrObjectsCount > 4) sharedBuffer->vrObjectsCount = 4;
	sharedBuffer->trackingRefrencesCount = ovr_GetTrackerCount(oSession);
	if (sharedBuffer->trackingRefrencesCount > 4) sharedBuffer->trackingRefrencesCount = 4;

	ReleaseMutex(sharedMutex);

	//Run up to x times per second.
	//Add this inside the loop to allow for a dynamic rate synced with the SteamVR frame rate?
	long double minFrameTime = (1000 / RUN_INTERVAL) * 1000; //(ms/interval)*ms_to_microseconds.
	while (true)
	{
		std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

		ovrSessionStatus sessionStatus;
		ovr_GetSessionStatus(oSession, &sessionStatus);
		if (sessionStatus.ShouldQuit) break;

		VRLoop(oSession, sharedMutex, sharedBuffer, frameCount, oHapticsBuffer, hapticsBuffer, hapticsBufferSize);

		frameCount++;

		long double duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
		if (duration < minFrameTime) Sleep((minFrameTime - duration) / 1000);
	}

	return 0;
}
