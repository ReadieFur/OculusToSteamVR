#pragma once

#define NOMINMAX

#include <Windows.h>
#include <OVR_CAPI.h>
#include <vector>

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