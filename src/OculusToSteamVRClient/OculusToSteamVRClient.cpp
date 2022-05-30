#include <iostream>
#include <Windows.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <OVR_CAPI.h>
#include <map>

/*Exit codes:
* 0 Ok.
* 1 Mutex instance already exists.
* 2 Failed to setup shared data.
* 3 Initialization error.
*/

#define RUN_INTERVAL 90 //Set to 90 becuase that is the max update rate of the Oculus sensors.

#if TRUE
#define SHOULD_LOG(frameCount) (frameCount % (RUN_INTERVAL * 5) == 0) //Slower but I'm using this because I don't understand the & (Log every 5s).
#else
#define SHOULD_LOG(frameCount) (frameCount & 0x7FF)
#endif

struct SharedData
{
	ovrTrackingState oTrackingState;
	ovrInputState oInputState[2];
	unsigned int vrObjectsCount;
	std::map<int, ovrPoseStatef> vrObjectsPose;
};

//https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L806
ovrQuatf OVRQuatFMul(ovrQuatf q1, ovrQuatf q2)
{
	ovrQuatf result = { 0 };
	result.x = q1.x * q2.w + q1.y * q2.z - q1.z * q2.y + q1.w * q2.x;
	result.y = -q1.x * q2.z + q1.y * q2.w + q1.z * q2.x + q1.w * q2.y;
	result.z = q1.x * q2.y - q1.y * q2.x + q1.z * q2.w + q1.w * q2.z;
	result.w = -q1.x * q2.x - q1.y * q2.y - q1.z * q2.z + q1.w * q2.w;
	return result;
}

//https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L372
ovrVector3f crossProduct(const ovrVector3f v, ovrVector3f p)
{
	return ovrVector3f{ v.y * p.z - v.z * p.y, v.z * p.x - v.x * p.z, v.x * p.y - v.y * p.x };
}

//https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L832
ovrVector3f RotateVector2(ovrVector3f v, ovrQuatf q)
{
	// nVidia SDK implementation

	ovrVector3f uv, uuv;
	ovrVector3f qvec{ q.x, q.y, q.z };
	uv = crossProduct(qvec, v);
	uuv = crossProduct(qvec, uv);
	uv.x *= (2.0f * q.w);
	uv.y *= (2.0f * q.w);
	uv.z *= (2.0f * q.w);
	uuv.x *= 2.0f;
	uuv.y *= 2.0f;
	uuv.z *= 2.0f;

	return ovrVector3f{ v.x + uv.x + uuv.x, v.y + uv.y + uuv.y, v.z + uv.z + uuv.z };
}

void VRLoop(ovrSession oSession/*, HANDLE sharedMutex*/, SharedData* sharedBuffer, uint64_t frameCount,
	ovrHapticsBuffer& oHapticsBuffer, uint8_t* hapticsBuffer, unsigned int hapticsBufferSize)
{
	ovrTrackingState oTrackingState = ovr_GetTrackingState(oSession, 0, false);
	sharedBuffer->oTrackingState = oTrackingState;

	//WaitForSingleObject(sharedMutex, INFINITE);

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

		//TODO.
		if (i == 1)
		{
			oResult = ovr_GetInputState(oSession, ovrControllerType::ovrControllerType_RTouch, &oInputState);
		}
		else
		{
			oResult = ovr_GetInputState(oSession, ovrControllerType::ovrControllerType_LTouch, &oInputState);
		}

#pragma region Offsets
		//https://github.com/mm0zct/Oculus_Touch_Steam_Link/blob/main/CustomHMD/OculusTouchLink.cpp#L872
		const ovrQuatf handQuatOffsets = { 0.3420201, 0, 0, 0.9396926 };
		const ovrVector3f rightHandVectorOffsets = { 0.00571, 0.04078, -0.03531 };
		const ovrVector3f handVectorOffsets2 = { -0.000999998, -0.1, 0.0019 };

		ovrQuatf handInputOrientation = sharedBuffer->oTrackingState.HandPoses[i].ThePose.Orientation;
		ovrQuatf handCorrectedOrientation = OVRQuatFMul(handInputOrientation, handQuatOffsets);
		ovrVector3f handVectorOffsets = { 0,0,0 };

		//Apply left or right offset.
		if (i == ovrHand_Right) handVectorOffsets = RotateVector2(rightHandVectorOffsets, handInputOrientation);
		else
		{
			ovrVector3f leftHandVectorOffset = rightHandVectorOffsets;
			leftHandVectorOffset.x = -leftHandVectorOffset.x;
			handVectorOffsets = RotateVector2(leftHandVectorOffset, handInputOrientation);
		}

		//Update values.
		sharedBuffer->oTrackingState.HandPoses[i].ThePose.Position.x += handVectorOffsets.x/* + handVectorOffsets2.x*/;
		sharedBuffer->oTrackingState.HandPoses[i].ThePose.Position.y += handVectorOffsets.y/* + handVectorOffsets2.y*/;
		sharedBuffer->oTrackingState.HandPoses[i].ThePose.Position.z += handVectorOffsets.z/* + handVectorOffsets2.z*/;
		sharedBuffer->oTrackingState.HandPoses[i].ThePose.Orientation = handCorrectedOrientation;
#pragma endregion

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
		sharedBuffer->vrObjectsPose[i] = oPose;

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

int InitSharedData(HANDLE& hMapFile, SharedData*& sharedBuffer/*, HANDLE& sharedMutex*/)
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

		HANDLE sharedMutex = CreateMutexW(0, true, L"Local\\ovr_client_shared_mutex");
		WaitForSingleObject(
			sharedMutex, //Handle to mutex.
			INFINITE); //No time-out interval.
		ReleaseMutex(sharedMutex);
	}

	if (sharedBuffer == NULL)
	{
		std::cout << "Could not map view of file " << GetLastError() << std::endl;
		CloseHandle(hMapFile);
		return 2;
	}

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
	//HANDLE sharedMutex;
	int initSharedBufferResult = InitSharedData(hMapFile, sharedBuffer/*, sharedMutex*/);
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

	//TODO: Make this dynamic as I believe the count here can change at any time.
	unsigned int vrObjectsCount = (ovr_GetConnectedControllerTypes(oSession) >> 8) & 0xf;
	sharedBuffer->vrObjectsCount = vrObjectsCount;

	//Run up to x times per second.
	//Add this inside the loop to allow for a dynamic rate synced with the SteamVR frame rate?
	long double minFrameTime = (1000 / RUN_INTERVAL) * 1000; //(ms/interval)*ms_to_microseconds.
	while (true)
	{
		std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

		ovrSessionStatus sessionStatus;
		ovr_GetSessionStatus(oSession, &sessionStatus);
		if (sessionStatus.ShouldQuit) break;

		VRLoop(oSession/*, sharedMutex*/, sharedBuffer, frameCount, oHapticsBuffer, hapticsBuffer, hapticsBufferSize);

		frameCount++;

		long double duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
		if (duration < minFrameTime) Sleep((minFrameTime - duration) / 1000);
	}

	return 0;
}
