#include <iostream>
#include <Windows.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <OVR_CAPI.h>
#include <vector>

/*Exit codes:
* 0 Ok.
* 1 Mutex instance already exists.
* 2 Failed to setup shared data.
* 3 Initialization error.
*/

#define RUN_INTERVAL 90 //Set to 90 becuase that is the max update rate of the Oculus sensors.

struct SharedData
{
	HANDLE clientHandle; //Use with: WaitForSingleObject(clientHandle, 0) == STATUS_TIMEOUT. If true then the process is still active.
	//std::string logBuffer; //Broken.
	ovrTrackingState oTrackingState;
	ovrInputState oInputState[2];
	unsigned int vrObjectsCount;
	std::vector<ovrPoseStatef> vrObjects;
	unsigned int trackingRefrencesCount;
	std::vector<ovrTrackerPose> trackingRefrences;
};

inline bool ShouldLog(int frameCount)
{
#if TRUE
	return frameCount % (RUN_INTERVAL * 5) == 0;  //Slower but I'm using this because I don't understand the & (Log every 5s).
#else
	return frameCount & 0x7FF;
#endif
}

void VRLoop(ovrSession oSession, HANDLE sharedMutex, SharedData* sharedBuffer, uint64_t frameCount,
	ovrHapticsBuffer& oHapticsBuffer, uint8_t* hapticsBuffer, unsigned int hapticsBufferSize)
{
	ovrTrackingState oTrackingState = ovr_GetTrackingState(oSession, 0, false);
	sharedBuffer->oTrackingState = oTrackingState;

	WaitForSingleObject(sharedMutex, INFINITE);

	bool shouldLog = ShouldLog(frameCount);
	double frameTime = ovr_GetPredictedDisplayTime(oSession, frameCount);

	oTrackingState = ovr_GetTrackingState(oSession, frameTime, ovrTrue);
	sharedBuffer->oTrackingState = oTrackingState;

	//HMD.
	if (shouldLog)
	{
		std::cout << "HMD"
			<< " X:" << oTrackingState.HeadPose.ThePose.Position.x
			<< " Y:" << oTrackingState.HeadPose.ThePose.Position.y
			<< " Z:" << oTrackingState.HeadPose.ThePose.Position.z
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

		if (shouldLog)
		{
			std::cout << (i == 0 ? "Left Hand" : "Right Hand")
				<< " 0x" << std::fixed << std::hex << oTrackingState.HandStatusFlags[i] << std::dec
				<< " X:" << oTrackingState.HandPoses[i].ThePose.Position.x
				<< " Y:" << oTrackingState.HandPoses[i].ThePose.Position.y
				<< " Z:" << oTrackingState.HandPoses[i].ThePose.Position.z
				<< std::hex << " Button:0x" << oInputState.Buttons
				<< " Touch:0x" << oInputState.Touches
				<< " Grip:" << oInputState.HandTrigger[i]
				<< " Trigger:" << oInputState.IndexTrigger[i]
				<< " Input Result:0x" << oResult
				<< std::dec << std::endl;
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
			std::cout << "Object" << i
				<< " X:" << oPose.ThePose.Position.x
				<< " Y:" << oPose.ThePose.Position.y
				<< " Z:" << oPose.ThePose.Position.z
				<< std::endl;
		}
	}

	//Sensors.
	for (int i = 0; i < sharedBuffer->trackingRefrencesCount; i++) sharedBuffer->trackingRefrences[i] = ovr_GetTrackerPose(oSession, i);

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

	sharedMutex = CreateMutexW(0, true, L"Local\\ovr_client_shared_mutex");
	//No need to wait here.
	/*WaitForSingleObject(
		sharedMutex, //Handle to mutex.
		INFINITE); //No time-out interval.
	ReleaseMutex(sharedMutex);*/

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

	HANDLE hMapFile;
	SharedData* sharedBuffer;
	HANDLE sharedMutex;
	int initSharedBufferResult = InitSharedData(hMapFile, sharedBuffer, sharedMutex);
	if (initSharedBufferResult != 0) return initSharedBufferResult;

	sharedBuffer->clientHandle = GetCurrentProcess();

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

	//std::thread vibThread(vibrationThread, mSession);

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
	sharedBuffer->vrObjects.resize(sharedBuffer->vrObjectsCount);
	sharedBuffer->trackingRefrencesCount = ovr_GetTrackerCount(oSession);
	sharedBuffer->trackingRefrences.resize(sharedBuffer->trackingRefrencesCount);

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
