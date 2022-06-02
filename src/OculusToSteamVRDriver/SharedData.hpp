#pragma once

#define NOMINMAX

#include <Windows.h>
#include <OVR_CAPI.h>

struct SharedData
{
	HANDLE clientHandle; //Use with: WaitForSingleObject(clientHandle, 0) == STATUS_TIMEOUT. If true then the process is still active.
	//std::string logBuffer; //Broken.
	ovrTrackingState oTrackingState;
	ovrInputState oInputState[2];
	unsigned int vrObjectsCount;
	ovrPoseStatef vrObjects[4]; //For now we will only support up to 4 objects. (Im not too sure how to resize the shared memeory so a vector wouldn't work).
	unsigned int trackingRefrencesCount;
	ovrTrackerPose trackingRefrences[4];
};