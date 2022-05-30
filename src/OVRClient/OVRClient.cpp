// OVRClient.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <Windows.h>
#include <mutex>
#include <thread>
#include <OVR_CAPI.h>

/*Exit codes:
* 0 Ok.
* 1 Mutex instance already exists.
* 2 Failed to setup shared data.
* 3 Initialization error.
*/

#if TRUE
#define SHOULD_LOG(frameCount) (frameCount % 1000 == 0) //Slower but for debugging (because i dont understand the &).
#else
#define SHOULD_LOG(frameCount) (frameCount & 0x7FF)
#endif

struct SharedData
{
	ovrTrackingState oTrackingState;
	unsigned int vrObjectsCount;
};

void VRLoop(ovrSession oSession, HANDLE sharedMutex, SharedData* sharedBuffer, uint64_t frameCount,
	ovrHapticsBuffer& oHapticsBuffer, uint8_t* hapticsBuffer, unsigned int hapticsBufferSize)
{
	ovrTrackingState oTrackingState = ovr_GetTrackingState(oSession, 0, false);
	sharedBuffer->oTrackingState = oTrackingState;

	WaitForSingleObject(sharedMutex, INFINITE);

	bool shouldLog = SHOULD_LOG(frameCount);
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
		ovrResult oResult;

		if (i == 1)
		{
			oResult = ovr_GetInputState(oSession, ovrControllerType::ovrControllerType_RTouch, &oInputState);
		}
		else
		{
			oResult = ovr_GetInputState(oSession, ovrControllerType::ovrControllerType_LTouch, &oInputState);
		}

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
	unsigned int vrObjectsCount = (ovr_GetConnectedControllerTypes(oSession) >> 8) & 0xf;
	sharedBuffer->vrObjectsCount = vrObjectsCount;
	for (int i = 0; i < vrObjectsCount; i++)
	{
		ovrTrackedDeviceType deviceType = (ovrTrackedDeviceType)(ovrTrackedDevice_Object0 + i);
		ovrPoseStatef oPose;

		ovr_GetDevicePoses(oSession, &deviceType, 1, frameTime, &oPose);

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

	if (shouldLog) std::cout << std::endl;
}

int InitSharedData(HANDLE& hMapFile, SharedData*& sharedBuffer, HANDLE& sharedMutex)
{
	hMapFile = CreateFileMapping(
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

	sharedBuffer = new(MapViewOfFile(
		hMapFile, //Handle to map object.
		FILE_MAP_ALL_ACCESS,  //Read/write permission.
		0,
		0,
		sizeof(SharedData)))SharedData();
	if (sharedBuffer == NULL)
	{
		std::cout << "Could not map view of file " << GetLastError() << std::endl;
		CloseHandle(hMapFile);
		return 2;
	}

	sharedMutex = CreateMutex(0, true, L"Local\\ovr_client_shared_mutex");
	WaitForSingleObject(
		sharedMutex,    // handle to mutex
		INFINITE);  // no time-out interval
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

	HANDLE hMapFile;
	SharedData* sharedBuffer;
	HANDLE sharedMutex;
	int initSharedBufferResult = InitSharedData(hMapFile, sharedBuffer, sharedMutex);
	if (initSharedBufferResult != 0) return initSharedBufferResult;

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

	while (true)
	{
		ovrSessionStatus sessionStatus;
		ovr_GetSessionStatus(oSession, &sessionStatus);
		if (sessionStatus.ShouldQuit) break;

		VRLoop(oSession, sharedMutex, sharedBuffer, frameCount, oHapticsBuffer, hapticsBuffer, hapticsBufferSize);

		frameCount++;
		Sleep(1);
	}

	return 0;
}
